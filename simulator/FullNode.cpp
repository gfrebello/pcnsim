#include "globals.h"
#include "crypto.h"
#include "baseMessage_m.h"
#include "payment_m.h"
#include "PaymentChannel.h"
#include "invoice_m.h"
#include "commitmentSigned_m.h"
#include "revokeAndAck_m.h"
#include "paymentRefused_m.h"
#include "HTLC.h"

class FullNode : public cSimpleModule {

    protected:
        // Protected data structures
        std::map<std::string, std::string> _myPreImages; // paymentHash to preImage
        std::map<std::string, std::string> _myInFlights; // paymentHash to nodeName (who owes me)
        std::map<std::string, std::string> _myPayments; //paymentHash to status (PENDING, COMPLETED, FAILED, or CANCELED)
        std::map<std::string, BaseMessage *> _myStoredMessages; // paymentHash to baseMsg (for finding reverse path)
        std::map<std::string, cModule*> _senderModules; // paymentHash to Module

        // Omnetpp functions
        virtual void initialize() override;
        virtual void handleMessage(cMessage *msg) override;
        virtual void refreshDisplay() const;

        // Routing functions
        virtual std::vector<std::string> getPath(std::map<std::string, std::string> parents, std::string target);
        virtual std::string minDistanceNode (std::map<std::string, double> distances, std::map<std::string, bool> visited);
        virtual std::vector<std::string> dijkstraWeightedShortestPath (std::string src, std::string target, std::map<std::string, std::vector<std::pair<std::string, double> > > graph);

        // Message handlers
        virtual void initHandler (BaseMessage *baseMsg);
        virtual void invoiceHandler (BaseMessage *baseMsg);
        virtual void updateAddHTLCHandler (BaseMessage *baseMsg);
        virtual void updateFulfillHTLCHandler (BaseMessage *baseMsg);
        virtual void updateFailHTLCHandler (BaseMessage *baseMsg);
        virtual void paymentRefusedHandler (BaseMessage *baseMsg);
        virtual void commitSignedHandler (BaseMessage *baseMsg);
        virtual void revokeAndAckHandler (BaseMessage *baseMsg);

        // HTLC senders
        virtual void sendFirstFulfillHTLC (HTLC *htlc, std::string firstHop);
        virtual void sendFirstFailHTLC (HTLC *htlc, std::string firstHop);

        // HTLC committers
        virtual void commitUpdateAddHTLC (HTLC *htlc, std::string neighbor);
        virtual void commitUpdateFulfillHTLC (HTLC *htlc, std::string neighbor);
        virtual void commitUpdateFailHTLC (HTLC *htlc, std::string neighbor);

        // Util functions
        virtual bool tryUpdatePaymentChannel (std::string nodeName, double value, bool increase);
        virtual bool hasCapacityToForward (std::string nodeName, double value);
        virtual bool tryCommitTxOrFail (std::string, bool);
        virtual Invoice* generateInvoice (std::string srcName, double value);
        virtual void setInFlight (HTLC *htlc, std::string nextHop);
        virtual bool isInFlight (HTLC *htlc, std::string nextHop);
        virtual std::vector <HTLC *> getSortedPendingHTLCs (std::vector<HTLC *> HTLCs, std::string neighbor);
        virtual std::string createHTLCId (std::string paymentHash, int htlcType);

    public:
        // Public data structures
        bool _isFirstSelfMessage;
        cTopology *_localTopology;
        int localCommitCounter;
        typedef std::map<std::string, int> RoutingTable;  // neighborName to gateIndex
        RoutingTable rtable;
        std::map<std::string, PaymentChannel> _paymentChannels; // neighborName to PaymentChannel
        std::map<std::string, int> _signals; // myName to signal
};

// Define module and initialize random number generator
Define_Module(FullNode);

/***********************************************************************************************************************/
/* OMNETPP FUNCTIONS                                                                                                   */
/***********************************************************************************************************************/

void FullNode::initialize() {

    // Get name (id) and initialize local topology based on the global topology created by netBuilder
    _localTopology = globalTopology;
    this->localCommitCounter = 0;
    std::string myName = getName();
    std::map<std::string, std::vector<std::tuple<std::string, double, simtime_t>>> localPendingPayments = pendingPayments;

    // Initialize payment channels
    for (auto& neighborToPCs : nameToPCs[myName]) {
        std::string neighborName = neighborToPCs.first;
        std::tuple<double, double, double, int, double, double, cGate*, cGate*> pcTuple = neighborToPCs.second;
        double capacity = std::get<0>(pcTuple);
        double fee = std::get<1>(pcTuple);
        double quality = std::get<2>(pcTuple);
        int maxAcceptedHTLCs = std::get<3>(pcTuple);
        int numHTLCs = 0;
        double HTLCMinimumMsat = std::get<4>(pcTuple);
        double channelReserveSatoshis = std::get<5>(pcTuple);
        cGate* localGate = std::get<6>(pcTuple);
        cGate* neighborGate = std::get<7>(pcTuple);

        PaymentChannel pc = PaymentChannel(capacity, fee, quality, maxAcceptedHTLCs, numHTLCs, HTLCMinimumMsat, channelReserveSatoshis, localGate, neighborGate);
        _paymentChannels[neighborName] = pc;

        // Register signals and statistics
        std::string signalName = myName +"-to-" + neighborName + ":capacity";
        simsignal_t signal = registerSignal(signalName.c_str());
        _signals[signalName] = signal;
        emit(_signals[signalName], _paymentChannels[neighborName]._capacity);

        std::string statisticName = myName +"-to-" + neighborName + ":capacity";
        cProperty *statisticTemplate = getProperties()->get("statisticTemplate", "pcCapacities");
        getEnvir()->addResultRecorders(this, signal, statisticName.c_str(), statisticTemplate);
    }


    // Build routing table
    cTopology::Node *thisNode = _localTopology->getNodeFor(this);
    for (int i = 0; i < _localTopology->getNumNodes(); i++) {
        if (_localTopology->getNode(i) == thisNode)
            continue;  // skip ourselves

        _localTopology->calculateWeightedSingleShortestPathsTo(_localTopology->getNode(i));

        if (thisNode->getNumPaths() == 0)
            continue;  // not connected

        cGate *parentModuleGate = thisNode->getPath(0)->getLocalGate();
        int gateIndex = parentModuleGate->getIndex();
        std::string nodeName = _localTopology->getNode(i)->getModule()->getName();
        rtable[nodeName] = gateIndex;
        EV << "  towards " << nodeName << " gateIndex is " << gateIndex << endl;
    }

    // Schedule payments according to workload
    std::map<std::string, std::vector<std::tuple<std::string, double, simtime_t>>>::iterator it = pendingPayments.find(myName);
    if (it != pendingPayments.end()) {
        std::vector<std::tuple<std::string, double, simtime_t>> myWorkload = it->second;

        for (const auto& paymentTuple: myWorkload) {

             std::string srcName = std::get<0>(paymentTuple);
             double value = std::get<1>(paymentTuple);
             simtime_t time = std::get<2>(paymentTuple);
             char msgname[100];
             sprintf(msgname, "%s-to-%s;value:%0.1f", srcName.c_str(), myName.c_str(), value);

             // Create payment message
             Payment *trMsg = new Payment(msgname);
             trMsg->setSource(srcName.c_str());
             trMsg->setDestination(myName.c_str());
             trMsg->setValue(value);
             trMsg->setHopCount(0);

             // Create base message
             BaseMessage *baseMsg = new BaseMessage();
             baseMsg->setMessageType(TRANSACTION_INIT);
             baseMsg->setHopCount(0);

             // Encapsulate and schedule
             baseMsg->encapsulate(trMsg);
             scheduleAt(simTime()+time, baseMsg);
             _isFirstSelfMessage = true;
        }
    } else {
        EV << "No workload found for " << myName.c_str() << ".\n";
    }
}

void FullNode::handleMessage(cMessage *msg) {
    // Decapsulates and treats messages according to their message types

    BaseMessage *baseMsg = check_and_cast<BaseMessage *>(msg);

    switch(baseMsg->getMessageType()) {

        case TRANSACTION_INIT: {
            initHandler(baseMsg);
            break;
        }
        case INVOICE: {
            invoiceHandler(baseMsg);
            break;
        }
        case UPDATE_ADD_HTLC: {
            updateAddHTLCHandler(baseMsg);
            break;
        }
        case UPDATE_FULFILL_HTLC: {
            updateFulfillHTLCHandler(baseMsg);
            break;
        }
        case UPDATE_FAIL_HTLC: {
            updateFailHTLCHandler(baseMsg);
            break;
        }
        case PAYMENT_REFUSED: {
            paymentRefusedHandler(baseMsg);
            break;
        }
        case COMMITMENT_SIGNED: {
            commitSignedHandler(baseMsg);
            break;
        }
        case REVOKE_AND_ACK: {
            revokeAndAckHandler(baseMsg);
            break;
        }
    }
}

void FullNode::refreshDisplay() const {

    for(auto& it : _paymentChannels) {
        char buf[30];
        std::string neighborName = it.first;
        std::string neighborPath = "PCN." + neighborName;
        float capacity = it.second.getCapacity();
        cGate *gate = it.second.getLocalGate();
        cChannel *channel = gate->getChannel();
        sprintf(buf, "%0.1f\n", capacity);
        channel->getDisplayString().setTagArg("t", 0, buf);
        channel->getDisplayString().setTagArg("t", 1, "l");
    }
}


/***********************************************************************************************************************/
/* ROUTING FUNCTIONS                                                                                                   */
/***********************************************************************************************************************/

std::string FullNode::minDistanceNode (std::map<std::string, double> distances, std::map<std::string, bool> visited) {
    // Selects the node with minimum distance out of a distances list while disregarding visited nodes

    // Initialize min value
    double minDist = INT_MAX;
    std::string minName = "";

    for (auto& node : distances) {
        std::string nodeName = node.first;
        double dist = node.second;
        bool isNodeVisited = visited.find(nodeName)->second;
        if (!isNodeVisited && dist <= minDist) {
            minDist = dist;
            minName = nodeName;
        }
    }
    if (minDist == INT_MAX){
        return NULL;
    }
    else{
        return minName;
    }

}

std::vector<std::string> FullNode::getPath (std::map<std::string, std::string> parents, std::string target) {
    // Return the path to source given a target and its parent nodes

    std::vector<std::string> path;
    std::string node = target;

    // Recursively traverse the parents path
    while (parents[node] != node) {
        path.insert(path.begin(), node);
        node = parents[node];
    }

    // Set ourselves as the source node
    path.insert(path.begin(), getName());

    return path;
}

std::vector<std::string> FullNode::dijkstraWeightedShortestPath (std::string src, std::string target, std::map<std::string, std::vector<std::pair<std::string, double> > > graph) {
    // This function returns the Dijkstra's shortest path from a source to some target given an adjacency matrix

    int numNodes = graph.size();
    std::map<std::string, double> distances;
    std::map<std::string, bool> visited;
    std::map<std::string, std::string> parents; // a.k.a nodeToParent

    // Initialize distances as infinite and visited array as false
    for (auto & node : graph) {
        distances[node.first] = INT_MAX;
        visited[node.first] = false;
        parents[node.first] = "";
    }

    // Set source node distance to zero and parent to itself
    distances[src] = 0;
    parents[src] = src;

    for (int i = 0; i < numNodes-1; i++) {
        std::string node = minDistanceNode(distances, visited);
        visited[node] = true;

        std::vector<std::pair<std::string,double>>::iterator it;

        // Update distance value of neighbor nodes of the current node
        for (it = graph[node].begin(); it != graph[node].end(); it++){
            std::string neighbor = it->first;
            double linkWeight = it->second;
            if (!visited[neighbor]) {
                if(distances[node] + linkWeight < distances[neighbor]) {
                    parents[neighbor] = node;
                    distances[neighbor] = distances[node] + linkWeight;
                }
            }
        }
    }
    return getPath(parents, target);
}


/***********************************************************************************************************************/
/* MESSAGE HANDLERS                                                                                                    */
/***********************************************************************************************************************/

void FullNode::initHandler (BaseMessage *baseMsg) {

    Payment *initMsg = check_and_cast<Payment *> (baseMsg->decapsulate());
    EV << "TRANSACTION_INIT received. Starting payment "<< initMsg->getName() << "\n";

    // Create ephemeral communication channel with the payment source
    std::string srcName = initMsg->getSource();
    std::string srcPath = "PCN." + srcName;
    double value = initMsg->getValue();
    cModule* srcMod = getModuleByPath(srcPath.c_str());
    cGate* myGate = this->getOrCreateFirstUnconnectedGate("out", 0, false, true);
    cGate* srcGate = srcMod->getOrCreateFirstUnconnectedGate("in", 0, false, true);
    cDelayChannel *tmpChannel = cDelayChannel::create("tmpChannel");
    tmpChannel->setDelay(100);
    myGate->connectTo(srcGate, tmpChannel);

    // Create invoice and send it to the payment source
    Invoice *invMsg = generateInvoice(srcName, value);
    baseMsg->setMessageType(INVOICE);
    baseMsg->encapsulate(invMsg);
    baseMsg->setName("INVOICE");
    send(baseMsg, myGate);

    // Close ephemeral connection
    myGate->disconnect();
}

void FullNode::invoiceHandler (BaseMessage *baseMsg) {

    Invoice *invMsg = check_and_cast<Invoice *> (baseMsg->decapsulate());
    EV << "INVOICE received. Payment hash: " << invMsg->getPaymentHash() << "\n";

    std::string dstName = invMsg->getDestination();
    std::string myName = getName();
    std::string paymentHash = invMsg->getPaymentHash();
    int htlcType = UPDATE_ADD_HTLC;
    std::string htlcId = createHTLCId(paymentHash, htlcType);
    double value = invMsg->getValue();

    // Find route to destination
    std::vector<std::string> path = this->dijkstraWeightedShortestPath(myName, dstName, adjMatrix);
    std::string firstHop = path[1];

    // If payment is larger than our capacity in the outbound payment channel, mark is as canceled and return
   if (!hasCapacityToForward(firstHop, value)) {
       _myPayments[paymentHash] = "CANCELED";
       EV << "WARNING: Canceling payment " + paymentHash + " on node " + myName + " due to insufficient funds in the first hop.\n";
       return;
   }

   // Add payment into payment list and set status = pending
   _myPayments[paymentHash] = "PENDING";

   // Print route
    std::string printPath = "Full route to destination: ";
    for (auto hop: path)
        printPath = printPath + hop + ", ";
    printPath += "\n";
    EV << printPath;

    //Create HTLC
    EV << "Creating HTLC to kick off the payment process \n";
    BaseMessage *newMessage = new BaseMessage();
    newMessage->setDestination(dstName.c_str());
    newMessage->setMessageType(UPDATE_ADD_HTLC);
    newMessage->setHopCount(1);
    newMessage->setHops(path);
    newMessage->setName("UPDATE_ADD_HTLC");
    newMessage->setDisplayString("i=block/encrypt;is=s");

    UpdateAddHTLC *firstUpdateAddHTLC = new UpdateAddHTLC();
    firstUpdateAddHTLC->setHtlcId(htlcId.c_str());
    firstUpdateAddHTLC->setSource(myName.c_str());
    firstUpdateAddHTLC->setPaymentHash(paymentHash.c_str());
    firstUpdateAddHTLC->setValue(value);

    HTLC *firstHTLC = new HTLC(firstUpdateAddHTLC);
    _paymentChannels[firstHop].setPendingHTLC(htlcId, firstHTLC);
    _paymentChannels[firstHop].setLastPendingHTLCFIFO(firstHTLC);
    _paymentChannels[firstHop].setPreviousHopUp(htlcId, myName);

    newMessage->encapsulate(firstUpdateAddHTLC);
    cGate *gate = _paymentChannels[firstHop].getLocalGate();

    //Sending HTLC out
    EV << "Sending HTLC to " + firstHop + " with payment hash " + paymentHash + "\n";
    send(newMessage, gate);

}

void FullNode::updateAddHTLCHandler (BaseMessage *baseMsg) {


    EV << "UPDATE_ADD_HTLC received at " + std::string(getName()) + " from " + std::string(baseMsg->getSenderModule()->getName()) + ".\n";
    std::string myName = getName();

    // If the message is a self message, it means we already attempted to commit changes but failed because the batch size was insufficient. So we wait for the timeout.
    // Otherwise, we attempt to commit normally.
    if (!baseMsg->isSelfMessage()){

        // Decapsulate message and get path
        UpdateAddHTLC *updateAddHTLCMsg = check_and_cast<UpdateAddHTLC *> (baseMsg->decapsulate());
        std::string dstName = baseMsg->getDestination();
        std::vector<std::string> path = baseMsg->getHops();
        std::string sender = baseMsg->getSenderModule()->getName();
        std::string paymentHash = updateAddHTLCMsg->getPaymentHash();
        int htlcType = UPDATE_ADD_HTLC;
        std::string htlcId = updateAddHTLCMsg->getHtlcId();
        double value = updateAddHTLCMsg->getValue();

        if ((myName == "node2") && (sender == "node0"))
        {
            // Set breakpoint here
            int x = 1;
        }


        // Create new HTLC in the backward direction and set it as pending
        HTLC *htlcBackward = new HTLC(updateAddHTLCMsg);
        EV << "Storing UPDATE_ADD_HTLC from node " + sender + " as pending.\n";
        EV << "Payment hash:" + paymentHash + ".\n";
        _paymentChannels[sender].setPendingHTLC(htlcId, htlcBackward);
        _paymentChannels[sender].setLastPendingHTLCFIFO(htlcBackward);
        _paymentChannels[sender].setPreviousHopUp(htlcId, sender);

        // If I'm the destination, trigger commit immediately and return
        if (dstName == this->getName()){
            EV << "Payment reached its destination. Not forwarding.\n";

            // Store base message for retrieving path on the first fulfill message later
            _myStoredMessages[paymentHash] = baseMsg;

            if (!tryCommitTxOrFail(sender, false)){
                EV << "Setting timeout for node " + myName + "\n";
                scheduleAt((simTime() + SimTime(500,SIMTIME_MS)),baseMsg);
            }
            return;
        }

        // If I'm not the destination, forward the message to the next hop in the UPSTREAM path
        std::string nextHop = path[baseMsg->getHopCount() + 1];
        std::string previousHop = path[baseMsg->getHopCount()-1];


        // Check if we have sufficient funds before forwarding
        if (!hasCapacityToForward(nextHop, value)) {
            // Not enough capacity to forward payment. Remove pending HTLCs and send a PAYMENT_REFUSED
            // message to the previous hop.
            _paymentChannels[sender].removePendingHTLC(htlcId);
            _paymentChannels[sender].removeLastPendingHTLCFIFO();
            _paymentChannels[sender].removePreviousHopUp(htlcId);

            BaseMessage *newMessage = new BaseMessage();
            newMessage->setDestination(previousHop.c_str());
            newMessage->setMessageType(PAYMENT_REFUSED);
            newMessage->setHopCount(baseMsg->getHopCount()-1);
            newMessage->setHops(path);
            newMessage->setName("PAYMENT_REFUSED");
            newMessage->setDisplayString("i=status/stop");

            PaymentRefused *paymentRefusedMsg = new PaymentRefused();
            paymentRefusedMsg->setPaymentHash(paymentHash.c_str());
            paymentRefusedMsg->setErrorReason("INSUFFICIENT CAPACITY");
            paymentRefusedMsg->setValue(value);

            newMessage->encapsulate(paymentRefusedMsg);

            cGate *gate = _paymentChannels[previousHop].getLocalGate();
            EV << "Sending PAYMENT_REFUSED to " + path[(newMessage->getHopCount())] + " with payment hash " + paymentRefusedMsg->getPaymentHash() + "\n";
            send(newMessage, gate);

        } else {
            // Enough funds. Forward HTLC.
            EV << "Creating HTLC to kick off the payment process \n";
            BaseMessage *newMessage = new BaseMessage();
            newMessage->setDestination(dstName.c_str());
            newMessage->setMessageType(UPDATE_ADD_HTLC);
            newMessage->setHopCount(baseMsg->getHopCount() + 1);
            newMessage->setHops(path);
            newMessage->setDisplayString("i=block/encrypt;is=s");
            newMessage->setName("UPDATE_ADD_HTLC");

            UpdateAddHTLC *newUpdateAddHTLC = new UpdateAddHTLC();
            newUpdateAddHTLC->setHtlcId(htlcId.c_str());
            newUpdateAddHTLC->setSource(myName.c_str());
            newUpdateAddHTLC->setPaymentHash(paymentHash.c_str());
            newUpdateAddHTLC->setValue(value);

            HTLC *htlcForward = new HTLC(newUpdateAddHTLC);

            // Add HTLC as pending in the forward direction and set previous hop as ourselves
            _paymentChannels[nextHop].setPendingHTLC(htlcId, htlcForward);
            _paymentChannels[nextHop].setLastPendingHTLCFIFO(htlcForward);
            _paymentChannels[nextHop].setPreviousHopUp(htlcId, myName);

            newMessage->encapsulate(newUpdateAddHTLC);

            cGate *gate = _paymentChannels[nextHop].getLocalGate();

            //Sending HTLC out
            EV << "Sending HTLC to " + path[(newMessage->getHopCount())] + " with payment hash " + paymentHash + "\n";
            send(newMessage, gate);

            if (!tryCommitTxOrFail(sender, false)){
                EV << "Setting timeout for node " + myName + ".\n";
                scheduleAt((simTime() + SimTime(500,SIMTIME_MS)),baseMsg);
            }
        }

    } else {
        // The message is the result of a timeout.
        EV << myName + " timeout expired. Creating commit.\n";
        std::vector<std::string> path = baseMsg->getHops();
        std::string previousHop = path[baseMsg->getHopCount()-1];
        std::string dstName = baseMsg->getDestination();
        tryCommitTxOrFail(previousHop, true);
    }
}

void FullNode::updateFulfillHTLCHandler (BaseMessage *baseMsg) {

    EV << "UPDATE_FULFILL_HTLC received at " + std::string(getName()) + " from " + std::string(baseMsg->getSenderModule()->getName()) + ".\n";
    std::string myName = getName();

    // If the message is a self message, it means we already attempted to commit changes but failed because the batch size was insufficient. So we wait for the timeout.
    // Otherwise, we attempt to commit normally.
    if (!baseMsg->isSelfMessage()){

        // Decapsulate message, get path, and preimage
        UpdateFulfillHTLC *fulfillHTLCMsg = check_and_cast<UpdateFulfillHTLC *> (baseMsg->decapsulate());
        std::string dstName = baseMsg->getDestination();
        std::vector<std::string> path = baseMsg->getHops();
        std::string sender = baseMsg->getSenderModule()->getName();
        std::string paymentHash = fulfillHTLCMsg->getPaymentHash();
        std::string preImage = fulfillHTLCMsg->getPreImage();
        double value = fulfillHTLCMsg->getValue();
        int htlcType = UPDATE_FULFILL_HTLC;
        std::string htlcId = fulfillHTLCMsg->getHtlcId();

         // Verify preimage
         if (sha256(preImage) != paymentHash){
             throw std::invalid_argument("ERROR: Failed to fulfill HTLC. Different hash value.");
         }

         // Create new HTLC in the backward direction and set it as pending
         HTLC *htlcBackward = new HTLC(fulfillHTLCMsg);
         EV << "Storing UPDATE_FULFILL_HTLC from node " + sender + " as pending.\n";
         EV << "Payment hash:" + paymentHash + ".\n";
         _paymentChannels[sender].setPendingHTLC(htlcId, htlcBackward);
         _paymentChannels[sender].setLastPendingHTLCFIFO(htlcBackward);
         _paymentChannels[sender].setPreviousHopDown(htlcId, sender);

         // If we are the destination, just try to commit the payment and return
         if (getName() == dstName) {
             EV << "Payment fulfillment has reached the payment's origin. Trying to commit...\n";
             if (!tryCommitTxOrFail(sender, false)) {
                 EV << "Setting timeout for node " + myName + "\n";
                 scheduleAt((simTime() + SimTime(500,SIMTIME_MS)),baseMsg);
             }
             return;
         }

        // If we're not the destination, forward the message to the next hop in the DOWNSTREAM path
        std::string nextHop = path[baseMsg->getHopCount()-1];
        std::string previousHop = path[baseMsg->getHopCount()+1];

        EV << "Forwarding UPDATE_FULFILL_HTLC in the downstream direction...\n";
        BaseMessage *newMessage = new BaseMessage();
        newMessage->setDestination(dstName.c_str());
        newMessage->setMessageType(UPDATE_FULFILL_HTLC);
        newMessage->setHopCount(baseMsg->getHopCount()-1);
        newMessage->setHops(path);
        newMessage->setDisplayString("i=block/decrypt;is=s");
        newMessage->setName("UPDATE_FULFILL_HTLC");

        UpdateFulfillHTLC *forwardFulfillHTLC = new UpdateFulfillHTLC();
        forwardFulfillHTLC->setHtlcId(htlcId.c_str());
        forwardFulfillHTLC->setPaymentHash(paymentHash.c_str());
        forwardFulfillHTLC->setPreImage(preImage.c_str());
        forwardFulfillHTLC->setValue(value);

        // Set UPDATE_FULFILL_HTLC as pending and invert the previous hop (now we're going downstream)
        HTLC *forwardBaseHTLC  = new HTLC(forwardFulfillHTLC);
        _paymentChannels[nextHop].setPendingHTLC(htlcId, forwardBaseHTLC);
        _paymentChannels[nextHop].setLastPendingHTLCFIFO(forwardBaseHTLC);
        _paymentChannels[nextHop].setPreviousHopDown(htlcId, myName);

        newMessage->encapsulate(forwardFulfillHTLC);

        cGate *gate = _paymentChannels[nextHop].getLocalGate();

        //Sending HTLC out
        EV << "Sending preimage " + preImage + " to " + path[(newMessage->getHopCount())] + " for payment hash " + paymentHash + "\n";
        send(newMessage, gate);

        // Try to commit
        if (!tryCommitTxOrFail(sender, false)){
            EV << "Setting timeout for node " + myName + ".\n";
            scheduleAt((simTime() + SimTime(500,SIMTIME_MS)),baseMsg);
        }

    } else {
        // The message is the result of a timeout.
        EV << myName + " timeout expired. Creating commit.\n";
        std::vector<std::string> path = baseMsg->getHops();
        std::string previousHop = path[baseMsg->getHopCount()+1];
        tryCommitTxOrFail(previousHop, true);
    }
}

void FullNode::updateFailHTLCHandler (BaseMessage *baseMsg) {

    EV << "UPDATE_FAIL_HTLC received at " + std::string(getName()) + " from " + std::string(baseMsg->getSenderModule()->getName()) + ".\n";
    std::string myName = getName();
    std::vector<std::string> path = baseMsg->getHops();
    std::string dstName = baseMsg->getDestination();

    // If the message is a self message, it means we already attempted to commit changes but failed because the batch size was insufficient. So we wait for the timeout.
    // Otherwise, we attempt to commit normally.
    if (!baseMsg->isSelfMessage()) {

        // Decapsulate message, get path, and preimage
        UpdateFailHTLC *failHTLCMsg = check_and_cast<UpdateFailHTLC *> (baseMsg->decapsulate());
        std::string sender = baseMsg->getSenderModule()->getName();
        std::string paymentHash = failHTLCMsg->getPaymentHash();
        std::string errorReason = failHTLCMsg->getErrorReason();
        double value = failHTLCMsg->getValue();
        int htlcType = UPDATE_FAIL_HTLC;
        std::string htlcId = failHTLCMsg->getHtlcId();

        // Create new HTLC in the backward direction and set it as pending
        HTLC *htlcBackward = new HTLC(failHTLCMsg);
        EV << "Storing UPDATE_FAIL_HTLC from node " + sender + " as pending.\n";
        EV << "Payment hash:" + paymentHash + ".\n";
        _paymentChannels[sender].setPendingHTLC(htlcId, htlcBackward);
        _paymentChannels[sender].setLastPendingHTLCFIFO(htlcBackward);
        _paymentChannels[sender].setPreviousHopDown(htlcId, sender);

        // If we are the destination, just try to commit and return
        if (getName() == dstName) {
            EV << "Payment fail has reached the payment's origin. Trying to commit...\n";
            if (!tryCommitTxOrFail(sender, false)) {
                EV << "Setting timeout for node " + myName + "\n";
                scheduleAt((simTime() + SimTime(500,SIMTIME_MS)),baseMsg);
            }
            return;
        }

        // If we're not the destination, forward the message to the next hop in the DOWNSTREAM path
        std::string nextHop = path[baseMsg->getHopCount()-1];
        std::string previousHop = path[baseMsg->getHopCount()+1];

        EV << "Forwarding UPDATE_FAIL_HTLC in the downstream direction...\n";
        BaseMessage *newMessage = new BaseMessage();
        newMessage->setDestination(dstName.c_str());
        newMessage->setMessageType(UPDATE_FAIL_HTLC);
        newMessage->setHopCount(baseMsg->getHopCount()-1);
        newMessage->setHops(path);
        newMessage->setDisplayString("i=status/stop");
        newMessage->setName("UPDATE_FAIL_HTLC");

        UpdateFailHTLC *forwardFailHTLC = new UpdateFailHTLC();
        forwardFailHTLC->setHtlcId(htlcId.c_str());
        forwardFailHTLC->setPaymentHash(paymentHash.c_str());
        forwardFailHTLC->setErrorReason(errorReason.c_str());
        forwardFailHTLC->setValue(value);

        // Set UPDATE_FAIL_HTLC as pending and invert the previous hop (now we're going downstream)
        HTLC *forwardBaseHTLC  = new HTLC(forwardFailHTLC);
        _paymentChannels[nextHop].setPendingHTLC(htlcId, forwardBaseHTLC);
        _paymentChannels[nextHop].setLastPendingHTLCFIFO(forwardBaseHTLC);
        //_paymentChannels[nextHop].removePreviousHopUp(htlcId);
        _paymentChannels[nextHop].setPreviousHopDown(htlcId, myName);

        newMessage->encapsulate(forwardFailHTLC);

        cGate *gate = _paymentChannels[nextHop].getLocalGate();

        //Sending HTLC out
        EV << "Sending UPDATE_FAIL_HTLC to " + path[(newMessage->getHopCount())] + "for payment hash " + paymentHash + "\n";
        send(newMessage, gate);

        // Try to commit
        if (!tryCommitTxOrFail(sender, false)){
            EV << "Setting timeout for node " + myName + ".\n";
            scheduleAt((simTime() + SimTime(500,SIMTIME_MS)),baseMsg);
        }

    } else {
        // The message is the result of a timeout.
        EV << myName + " timeout expired. Creating commit.\n";
        std::string previousHop = path[baseMsg->getHopCount()+1];
        tryCommitTxOrFail(previousHop, true);
    }
}

void FullNode::paymentRefusedHandler (BaseMessage *baseMsg) {

    std::string myName = getName();

    EV << "PAYMENT_REFUSED received at " + std::string(getName()) + " from " + std::string(baseMsg->getSenderModule()->getName()) + ".\n";

    PaymentRefused *paymentRefusedMsg = check_and_cast<PaymentRefused *> (baseMsg->getEncapsulatedPacket());
    std::string sender = baseMsg->getSenderModule()->getName();
    std::string paymentHash = paymentRefusedMsg->getPaymentHash();
    std::string errorReason = paymentRefusedMsg->getErrorReason();
    double value = paymentRefusedMsg->getValue();

    if ((myName == "node0"))
    {
        // Set breakpoint here
        int x = 1;
    }

    // If it's a self message, we know we are waiting for an UPDATE_ADD_HTLC to be committed before sending
    if (baseMsg->isSelfMessage()) {

        std::vector<std::string> path = baseMsg->getHops();
        std::string nextHop = path[baseMsg->getHopCount()-1];

        // Create dummy UPDATE_ADD_HTLC to use in lookup
        HTLC *tempHTLC = new HTLC();
        tempHTLC->setPaymentHash(paymentHash);
        tempHTLC->setType(UPDATE_ADD_HTLC);

        // Check if the UPDATE_ADD_HTLC has been committed
        if (!_paymentChannels[nextHop].isCommittedHTLC(tempHTLC)) {
            EV << "Waiting to send first UPDATE_FAIL_HTLC of payment " + paymentHash + ".\n";
            scheduleAt((simTime() + SimTime(500,SIMTIME_MS)),baseMsg);
        } else {
            EV << "Sending delayed first UPDATE_FAIL_HTLC for payment " + paymentHash + ".\n";

            int htlcType = UPDATE_FAIL_HTLC;
            std::string htlcId = createHTLCId(paymentHash, htlcType);

            UpdateFailHTLC *failHTLC = new UpdateFailHTLC();
            failHTLC->setHtlcId(htlcId.c_str());
            failHTLC->setPaymentHash(paymentHash.c_str());
            failHTLC->setValue(value);
            failHTLC->setErrorReason(errorReason.c_str());

            HTLC *baseHTLC = new HTLC(failHTLC);
            sendFirstFailHTLC(baseHTLC, nextHop);
        }
        return;
    }

    // Create temporary HTLC for status check
    HTLC *tempHTLC = new HTLC();
    tempHTLC->setPaymentHash(paymentHash);
    tempHTLC->setType(UPDATE_ADD_HTLC);

    EV << "Payment " + paymentHash +  "has been refused at node " + sender + ". Error reason: " + errorReason + ". Undoing updates...\n";

    if(!_paymentChannels[sender].isPendingHTLC(tempHTLC)) {
        // If we don't find the HTLC in our pending list, we look into our committed HTLCs list.
        if (!_paymentChannels[sender].isInFlight(tempHTLC)) {
            // The received HTLC is neither pending nor in flight. Something unexpected happened...
            throw std::invalid_argument( "ERROR: Unknown PAYMENT_REFUSED received!" );
        } else {
            // The HTLC is in flight. This cannot happen.
            throw std::invalid_argument( "ERROR: Refused HTLC is already in flight!" );
        }
    } else {

        int htlcType = UPDATE_ADD_HTLC;
        std::string htlcId = createHTLCId(paymentHash, htlcType);
        HTLC* addHTLC = _paymentChannels[sender].getPendingHTLC(htlcId);

        // The HTLC is still pending so we need to remove it upstream before the next commitment and trigger UPDATE_FAIL_HTLC downstream.
        _paymentChannels[sender].removePendingHTLC(htlcId);
        _paymentChannels[sender].removePendingHTLCFIFOByValue(addHTLC);
        _paymentChannels[sender].removePreviousHopUp(htlcId);

        // We should also look in our HTLCs waiting for ack (in case our payment has been refused after we sent
        // a commitment signed message with it)
        std::map<int, std::vector <HTLC *>> allHTLCsWaitingForAck = _paymentChannels[sender].getAllHTLCsWaitingForAck();

        for (const auto & pair : allHTLCsWaitingForAck) {
            int ackId = pair.first;
            std::vector<HTLC*> htlcVector = pair.second;
            for (const auto & htlc : htlcVector) {
                if (createHTLCId(htlc->getPaymentHash(), htlc->getType()) == htlcId)
                    _paymentChannels[sender].removeHTLCFromWaitingForAck(ackId, htlc);
            }
        }

        std::vector<std::string> path = baseMsg->getHops();

        // If I'm not the origin of the payment, trigger update fail downstream
        if (myName != path[0]) {
            // Store base message for retrieving path on the first fail message
            _myStoredMessages[paymentHash] = baseMsg;

            std::string nextHop = path[baseMsg->getHopCount()-1];

            // Create dummy UPDATE_ADD_HTLC to use in lookup
            HTLC *addHTLC = new HTLC();
            addHTLC->setPaymentHash(paymentHash);
            addHTLC->setType(UPDATE_ADD_HTLC);

            // If the corresponding UPDATE_ADD_HTLC has not been committed in the next downstream hop, wait to send
            // the UPDATE_FULFILL_HTLC. This way we avoid sending a fail message for a pending payment.
            if (_paymentChannels[nextHop].isCommittedHTLC(addHTLC)) {
                EV << "Waiting to send first UPDATE_FAIL_HTLC of payment " + paymentHash + ".\n";
                scheduleAt((simTime() + SimTime(500,SIMTIME_MS)),baseMsg);

            } else {
                UpdateFailHTLC *failHTLC = new UpdateFailHTLC();
                failHTLC->setHtlcId(htlcId.c_str());
                failHTLC->setPaymentHash(paymentHash.c_str());
                failHTLC->setValue(value);
                failHTLC->setErrorReason(errorReason.c_str());

                HTLC *baseHTLC = new HTLC(failHTLC);
                sendFirstFailHTLC(baseHTLC, nextHop);
            }
        }
    }
}

void FullNode::commitSignedHandler (BaseMessage *baseMsg) {
    EV << "COMMITMENT_SIGNED received at " + std::string(getName()) + " from " + std::string(baseMsg->getSenderModule()->getName()) + ".\n";
    commitmentSigned *commitMsg = check_and_cast<commitmentSigned *>(baseMsg->decapsulate());

    std::string sender = baseMsg->getSenderModule()->getName();
    HTLC *htlc = NULL;
    std::string myName = getName();
    std::string paymentHash;
    unsigned short index = 0;
    std::vector<HTLC *> HTLCs = commitMsg->getHTLCs();
    size_t numberHTLCs = HTLCs.size();
    std::vector<HTLC *> sortedHTLCs = this->getSortedPendingHTLCs(HTLCs, sender);

    // Iterate through the sorted HTLC list and attempt to commit them
    for (const auto & htlc : sortedHTLCs) {

        paymentHash = htlc->getPaymentHash();
        double value = htlc->getValue();
        int htlcType = htlc->getType();

        // Skip HTLC if it has already been committed
        if (_paymentChannels[sender].isCommittedHTLC(htlc)) {
            EV << "WARNING: Skipped " + std::to_string(htlcType) + " with paymentHash " + paymentHash + " on node " + myName + ".\n";
            continue;
        }

        switch(htlc->getType()) {
            case UPDATE_ADD_HTLC: {
                commitUpdateAddHTLC(htlc, sender);
                break;
            }
            case UPDATE_FULFILL_HTLC: {
                commitUpdateFulfillHTLC(htlc, sender);
                break;
            }
            case UPDATE_FAIL_HTLC: {
                commitUpdateFailHTLC(htlc, sender);
                break;
            }
        }
     }

    std::string signalName = myName +"-to-" + sender + ":capacity";
    emit(_signals[signalName], _paymentChannels[sender]._capacity);

    revokeAndAck *ack = new revokeAndAck();
    ack->setAckId(commitMsg->getId());
    ack->setHTLCs(sortedHTLCs);

    BaseMessage *newMessage = new BaseMessage();
    newMessage->setDestination(sender.c_str());
    newMessage->setMessageType(REVOKE_AND_ACK);
    newMessage->setHopCount(0);
    newMessage->setName("REVOKE_AND_ACK");
    //newMessage->setUpstreamDirection(!baseMsg->getUpstreamDirection());

    newMessage->encapsulate(ack);

    cGate *gate = _paymentChannels[sender].getLocalGate();

    //Sending pre image out
    EV << "Sending ack to " + sender + "with id " + std::to_string(commitMsg->getId()) + "\n";
    send(newMessage, gate);
}

void FullNode::revokeAndAckHandler (BaseMessage *baseMsg) {

    EV << "REVOKE_AND_ACK received at " + std::string(getName()) + " from " + std::string(baseMsg->getSenderModule()->getName()) + ".\n";
    revokeAndAck *ackMsg = check_and_cast<revokeAndAck *> (baseMsg->decapsulate());

    std::string sender = baseMsg->getSenderModule()->getName();
    int ackId = ackMsg->getAckId();

    std::vector<HTLC *> HTLCs = _paymentChannels[sender].getHTLCsWaitingForAck(ackId);
    HTLC *htlc;
    std::string myName = getName();
    std::string paymentHash;
    size_t index = 0;
    std::vector<HTLC *> sortedHTLCs = this->getSortedPendingHTLCs(HTLCs, sender);

    // Iterate through the sorted HTLC list and attempt to commit them
    for (const auto & htlc : sortedHTLCs) {

        paymentHash = htlc->getPaymentHash();
        double value = htlc->getValue();
        int htlcType = htlc->getType();

        if (_paymentChannels[sender].isCommittedHTLC(htlc)) {
            EV << "WARNING: Skipped " + std::to_string(htlcType) + " with paymentHash " + paymentHash + " on node " + myName + ".\n";
            continue;
        }

        switch(htlc->getType()) {
            case UPDATE_ADD_HTLC: {
                commitUpdateAddHTLC(htlc, sender);
                break;
            }
            case UPDATE_FULFILL_HTLC: {
                commitUpdateFulfillHTLC(htlc, sender);
                break;
            }
            case UPDATE_FAIL_HTLC: {
                commitUpdateFailHTLC(htlc, sender);
                break;
            }
        }
    }
    _paymentChannels[sender].removeHTLCsWaitingForAck(ackId);
    _paymentChannels[sender].setWaitingForAck(false);
}


/***********************************************************************************************************************/
/* HTLC SENDERS                                                                                                        */
/***********************************************************************************************************************/

void FullNode::sendFirstFulfillHTLC (HTLC *htlc, std::string firstHop) {
    // This function creates and sends an UPDATE_FULFILL_HTLC to the first hop in the downstream direction, triggering the beginning of payment completion

    EV << "Payment reached its destination. Releasing preimage... \n";

    //Get the stored pre image
    std::string htlcId = htlc->getHtlcId();
    std::string paymentHash = htlc->getPaymentHash();
    std::string preImage = _myPreImages[paymentHash];
    BaseMessage *storedBaseMsg = _myStoredMessages[paymentHash];
    std::vector<std::string> path = storedBaseMsg->getHops();
    std::string myName = getName();
    int htlcType = UPDATE_FULFILL_HTLC;

    //std::string htlcId = createHTLCId(paymentHash, htlcType);

    //Generate an UPDATE_FULFILL_HTLC message
    BaseMessage *newMessage = new BaseMessage();
    newMessage->setDestination(path[0].c_str());
    newMessage->setMessageType(UPDATE_FULFILL_HTLC);
    newMessage->setHopCount(storedBaseMsg->getHopCount() - 1);
    newMessage->setHops(storedBaseMsg->getHops());
    newMessage->setName("UPDATE_FULFILL_HTLC");
    newMessage->setDisplayString("i=block/decrypt;is=s");

    UpdateFulfillHTLC *firstFulfillHTLC = new UpdateFulfillHTLC();
    firstFulfillHTLC->setHtlcId(htlcId.c_str());
    firstFulfillHTLC->setPaymentHash(paymentHash.c_str());
    firstFulfillHTLC->setPreImage(preImage.c_str());
    firstFulfillHTLC->setValue(htlc->getValue());

    // Set UPDATE_FULFILL_HTLC as pending and invert the previous hop (now we're going downstream)
    HTLC *baseHTLC  = new HTLC(firstFulfillHTLC);
    _paymentChannels[firstHop].setPendingHTLC(htlcId, baseHTLC);
    _paymentChannels[firstHop].setLastPendingHTLCFIFO(baseHTLC);
    //_paymentChannels[firstHop].removePreviousHopUp(htlcId);
    _paymentChannels[firstHop].setPreviousHopDown(htlcId, myName);

    newMessage->encapsulate(firstFulfillHTLC);

    cGate *gate = _paymentChannels[firstHop].getLocalGate();

    _myPreImages.erase(paymentHash);
    _myStoredMessages.erase(paymentHash);

    //Sending HTLC out
    EV << "Sending pre image " + preImage + " to " + path[(newMessage->getHopCount()-1)] + "for payment hash " + paymentHash + "\n";
    send(newMessage, gate);
}

void FullNode::sendFirstFailHTLC (HTLC *htlc, std::string firstHop) {
    // This function creates and sends an UPDATE_FAIL_HTLC to the first hop in the downstream direction, triggering the beginning of payment failure

    //Get the stored base message
    std::string myName = getName();
    std::string htlcId = htlc->getHtlcId();
    std::string paymentHash = htlc->getPaymentHash();
    BaseMessage *storedBaseMsg = _myStoredMessages[paymentHash];
    std::vector<std::string> failPath = storedBaseMsg->getHops();
    double value = htlc->getValue();
    int htlcType = UPDATE_FAIL_HTLC;

    EV << "Initializing downstream unlocking of HTLCs for payment " + paymentHash + "... \n";

    //Generate an UPDATE_FAIL_HTLC message
    BaseMessage *newMessage = new BaseMessage();
    newMessage->setDestination(failPath[0].c_str());
    newMessage->setMessageType(UPDATE_FAIL_HTLC);
    newMessage->setHopCount(storedBaseMsg->getHopCount()-1);
    newMessage->setHops(failPath);
    newMessage->setName("UPDATE_FAIL_HTLC");
    newMessage->setDisplayString("i=status/stop");

    UpdateFailHTLC *firstFailHTLC = new UpdateFailHTLC();
    firstFailHTLC->setHtlcId(htlcId.c_str());
    firstFailHTLC->setPaymentHash(paymentHash.c_str());
    firstFailHTLC->setValue(htlc->getValue());
    firstFailHTLC->setErrorReason(htlc->getErrorReason().c_str());

    // Set UPDATE_FAIL_HTLC as pending and invert the previous hop (now we're going downstream)
    HTLC *baseHTLC  = new HTLC(firstFailHTLC);
    _paymentChannels[firstHop].setPendingHTLC(htlcId, baseHTLC);
    _paymentChannels[firstHop].setLastPendingHTLCFIFO(baseHTLC);
    _paymentChannels[firstHop].setPreviousHopDown(htlcId, myName);

    newMessage->encapsulate(firstFailHTLC);

    cGate *gate = _paymentChannels[firstHop].getLocalGate();

    _myPreImages.erase(paymentHash);
    _myStoredMessages.erase(paymentHash);

    //Sending HTLC out
    EV << "Sending first UPDATE_FAIL_HTLC to " + failPath[(newMessage->getHopCount())] + "for payment hash " + paymentHash + "\n";
    send(newMessage, gate);
}


/***********************************************************************************************************************/
/* HTLC COMMITTERS                                                                                                     */
/***********************************************************************************************************************/

void FullNode::commitUpdateAddHTLC (HTLC *htlc, std::string neighbor) {

    std::string myName = getName();
    std::string paymentHash = htlc->getPaymentHash();
    int htlcType = htlc->getType();
    std::string htlcId = htlc->getHtlcId();
    //std::string htlcId = createHTLCId(paymentHash, htlcType);
    std::string previousHop = _paymentChannels[neighbor].getPreviousHopUp(htlcId);

    EV << "Committing UPDATE_ADD_HTLC on channel " + myName + "->" + neighbor + " with payment hash " + paymentHash + "...\n";


    // If our neighbor is the HTLC's previous hop, we should commit but not set inFlight (that's the neighbors's responsibility)
    if (_paymentChannels[neighbor].getPreviousHopUp(htlcId) == neighbor) {

        // If we are the destination, commit and trigger first UPDATE_FULFILL_HTLC function
        if (!_myPreImages[paymentHash].empty()) {
            _paymentChannels[neighbor].removePendingHTLC(htlcId);
            _paymentChannels[neighbor].removePendingHTLCFIFOByValue(htlc);
            _paymentChannels[neighbor].setCommittedHTLC(htlcId, htlc);
            _paymentChannels[neighbor].setLastCommittedHTLCFIFO(htlc);

            sendFirstFulfillHTLC(htlc, neighbor);

        // Otherwise just commit
        } else {
            // Remove from pending list and queue
            _paymentChannels[neighbor].removePendingHTLC(htlcId);
            _paymentChannels[neighbor].removePendingHTLCFIFOByValue(htlc);
            _paymentChannels[neighbor].setCommittedHTLC(htlcId, htlc);
            _paymentChannels[neighbor].setLastCommittedHTLCFIFO(htlc);
        }
    // If our neighbor is the HTLC's next hop, we must set it as in flight and decrement the channel balance
    } else if (_paymentChannels[neighbor].getPreviousHopUp(htlcId) == myName) {
        setInFlight(htlc, neighbor);

        // Remove from pending list and queue
        _paymentChannels[neighbor].removePendingHTLC(htlcId);
        _paymentChannels[neighbor].removePendingHTLCFIFOByValue(htlc);
        _paymentChannels[neighbor].setCommittedHTLC(htlcId, htlc);
        _paymentChannels[neighbor].setLastCommittedHTLCFIFO(htlc);

    // If either case is satisfied, this is unexpected behavior
    } else {
        throw std::invalid_argument("ERROR: Could not commit UPDATE_ADD_HTLC. Reason: previousHop unknown.");
    }
}

void FullNode::commitUpdateFulfillHTLC (HTLC *htlc, std::string neighbor) {

    std::string myName = getName();
    std::string htlcId = htlc->getHtlcId();
    std::string paymentHash = htlc->getPaymentHash();
    std::string previousHop = _paymentChannels[neighbor].getPreviousHopDown(htlcId);
    double value = htlc->getValue();
    int htlcType = htlc->getType();
    //std::string htlcId = createHTLCId(paymentHash, htlcType);

    EV << "Committing UPDATE_FULFILL_HTLC on channel " + myName + "->" + neighbor + " with payment hash " + paymentHash + "...\n";

    // If our neighbor is the fulfill's previous hop, we must remove the in flight HTLCs
    if (_paymentChannels[neighbor].getPreviousHopDown(htlcId) == neighbor) {
        _paymentChannels[neighbor].removeInFlight(htlcId);

        // Remove from pending list and queue
        _paymentChannels[neighbor].removePendingHTLC(htlcId);
        _paymentChannels[neighbor].removePendingHTLCFIFOByValue(htlc);
        _paymentChannels[neighbor].setCommittedHTLC(htlcId, htlc);
        _paymentChannels[neighbor].setLastCommittedHTLCFIFO(htlc);

        // If we are the destination, the payment has completed successfully
        if(_myPayments[paymentHash] == "PENDING") {
            bubble("Payment completed!");
            EV << "Payment " + paymentHash + " completed!\n";
            _myPayments[paymentHash] = "COMPLETED";
        }

        // If we are the fulfill's previous hop, we should claim our money
    } else if (_paymentChannels[neighbor].getPreviousHopDown(htlcId) == myName) {

        // Remove from pending list and queue
        _paymentChannels[neighbor].removePendingHTLC(htlcId);
        _paymentChannels[neighbor].removePendingHTLCFIFOByValue(htlc);
        _paymentChannels[neighbor].setCommittedHTLC(htlcId, htlc);
        _paymentChannels[neighbor].setLastCommittedHTLCFIFO(htlc);

        tryUpdatePaymentChannel(neighbor, value, true);

    // If either case is satisfied, this is unexpected behavior
    } else {
        throw std::invalid_argument("ERROR: Could not commit UPDATE_FULFILL_HTLC. Reason: previousHop unknown.");
    }
}

void FullNode::commitUpdateFailHTLC (HTLC *htlc, std::string neighbor) {

    std::string myName = getName();
    std::string htlcId = htlc->getHtlcId();
    std::string paymentHash = htlc->getPaymentHash();
    std::string previousHop = _paymentChannels[neighbor].getPreviousHopDown(htlcId);
    double value = htlc->getValue();
    int htlcType = htlc->getType();

    EV << "Committing UPDATE_FAIL_HTLC on channel " + myName + "->" + neighbor + " with payment hash " + paymentHash + "...\n";

    // If our neighbor is the fail's previous hop, we should we must remove the in flight HTLCs and claim our money back
    if (_paymentChannels[neighbor].getPreviousHopDown(htlcId) == neighbor) {
        _paymentChannels[neighbor].removeInFlight(htlcId);

        // Remove from pending list and queue
        _paymentChannels[neighbor].removePendingHTLC(htlcId);
        _paymentChannels[neighbor].removePendingHTLCFIFOByValue(htlc);
        _paymentChannels[neighbor].setCommittedHTLC(htlcId, htlc);
        _paymentChannels[neighbor].setLastCommittedHTLCFIFO(htlc);

        tryUpdatePaymentChannel(neighbor, value, true);

        EV << "Claiming HTLC back. Value: " + std::to_string(value) + "\n.";


        // If we are the destination, the payment has failed successfully
        if(_myPayments[paymentHash] == "PENDING") {
            bubble("Payment failed!");
            EV << "Payment " + paymentHash + " failed!\n";
            _myPayments[paymentHash] = "FAIL";
        }

    // If our neighbor is the fails's next hop, just remove from pending (the updates have been applied in the sender node)
    } else if (_paymentChannels[neighbor].getPreviousHopDown(htlcId) == myName) {

        // Remove from pending list and queue
        _paymentChannels[neighbor].removePendingHTLC(htlcId);
        _paymentChannels[neighbor].removePendingHTLCFIFOByValue(htlc);
        _paymentChannels[neighbor].setCommittedHTLC(htlcId, htlc);
        _paymentChannels[neighbor].setLastCommittedHTLCFIFO(htlc);

    // If either case is satisfied, this is unexpected behavior
    } else {
        throw std::invalid_argument("ERROR: Could not commit UPDATE_FAIL_HTLC. Reason: previousHop unknown.");
    }
}


/***********************************************************************************************************************/
/* UTIL FUNCTIONS                                                                                                      */
/***********************************************************************************************************************/

bool FullNode::tryUpdatePaymentChannel (std::string nodeName, double value, bool increase) {
    // Helper function to update payment channels. If increase = true, the function attemps to increase
    // the capacity of the channel. Else, check whether the channel has enough capacity to process the payment.

    if(increase) {
        _paymentChannels[nodeName].increaseCapacity(value);
        return true;
    } else {
        double capacity = _paymentChannels[nodeName]._capacity;
        if(capacity - value < 0)
            return false;
        else {
            _paymentChannels[nodeName].decreaseCapacity(value);
            return true;
        }
    }
}

bool FullNode::hasCapacityToForward  (std::string nodeName, double value) {
    // Helper function that calculates the payment channel capacity after applying the pending HTLCs and checks if the node has sufficient funds to forward a payment.

    std::deque< HTLC*> pendingHTLCsFIFO = _paymentChannels[nodeName].getPendingHTLCsFIFO();
    std::string myName = getName();

    // Calculate the capacity after applying pending HTLCs
    double capacity = _paymentChannels[nodeName].getCapacity();
    for (const auto & htlc : pendingHTLCsFIFO) {
        std::string htlcId  = htlc->getHtlcId();
        int htlcType = htlc->getType();

        // If it's an add update, subtract value from capacity if we are the previous hop uptstream
        // (because we'll have less money when we commmit it)
        if (htlcType == UPDATE_ADD_HTLC) {
            if (_paymentChannels[nodeName].getPreviousHopUp(htlcId) == myName) {
                capacity -= htlc->getValue();
                if (capacity <= 0)
                    return false;
            }
        } else if (htlcType == UPDATE_FAIL_HTLC) {
            // If it's a fail update, add value to capacity if we are not the previous hop downstream
            // (because we'll recover money when we commmit it)
            if (_paymentChannels[nodeName].getPreviousHopDown(htlcId) == nodeName) {
                capacity += htlc->getValue();
            }
        } else {}; // If it's a fulfill update, do nothing (fulfills don't change the capacity in the upstream direction)
    }

    // Check if the capacity would become negative after forwarding the next payment
    if ((capacity - value) <= 0)
        return false;
    else
        return true;
}

bool FullNode::tryCommitTxOrFail(std::string sender, bool timeoutFlag) {
    /***********************************************************************************************************************/
    /* tryCommitOrFail verifies whether the pending transactions queue has reached the defined commitment batch size       */
    /* or not. If the batch is full, tryCommitOrFail creates a commitment_signed message and sends it to the node that     */
    /* shares the payment channel where the batch is full. If the batch has not yet reached its limit, tryCommitOrFail     */
    /* does nothing.                                                                                                       */
    /***********************************************************************************************************************/

    unsigned short int index = 0;
    HTLC *htlc = NULL;
    std::vector<HTLC *> HTLCVector;
    std::string paymentHash;
    bool through = false;
    std::string myName = getName();


    EV << "Entered tryCommitTxOrFail. Current batch size: " + std::to_string(_paymentChannels[sender].getPendingBatchSize()) + "\n";

    if (_paymentChannels[sender].getPendingBatchSize() >= COMMITMENT_BATCH_SIZE || timeoutFlag == true) {
        for (const auto & htlc : _paymentChannels[sender].getPendingHTLCsFIFO()) {
            HTLCVector.push_back(htlc);
        }

        EV << "Setting through to true\n";
        through = true;
        _paymentChannels[sender].setWaitingForAck(true);

        commitmentSigned *commitTx = new commitmentSigned();
        commitTx->setHTLCs(HTLCVector);
        commitTx->setId(localCommitCounter);

        _paymentChannels[sender].setHTLCsWaitingForAck(localCommitCounter, HTLCVector);

        localCommitCounter += 1;
        //int gateIndex = rtable[sender];
        cGate *gate = _paymentChannels[sender].getLocalGate();

        BaseMessage *baseMsg = new BaseMessage();
        baseMsg->setDestination(sender.c_str());
        baseMsg->setMessageType(COMMITMENT_SIGNED);
        baseMsg->setHopCount(0);
        baseMsg->setName("COMMITMENT_SIGNED");

        baseMsg->encapsulate(commitTx);

        EV << "Sending Commitment Signed from node " + std::string(getName()) + "to " + sender + ". localCommitCounter: " + std::to_string(localCommitCounter) + "\n";

        send (baseMsg, gate);
    }
    return through;
}

Invoice* FullNode::generateInvoice(std::string srcName, double value) {

    std::string preImage;
    std::string preImageHash;
    preImage = generatePreImage();
    preImageHash = sha256(preImage);

    EV<< "Generated pre image " + preImage + " with hash " + preImageHash + "\n";

    _myPreImages[preImageHash] = preImage;

    Invoice *invoice = new Invoice();
    invoice->setSource(srcName.c_str());
    invoice->setDestination(getName());
    invoice->setValue(value);
    invoice->setPaymentHash(preImageHash.c_str());

    return invoice;
}

void FullNode::setInFlight(HTLC *htlc, std::string nextHop) {
    // Sets payment in flight and removes from pending

    std::string htlcId = htlc->getHtlcId();
    std::string paymentHash = htlc->getPaymentHash();
    int htlcType = htlc->getType();

    // If payment is already in flight, do nothing.
    if (_paymentChannels[nextHop].getInFlight(htlcId)) {
        EV << "Payment " + paymentHash + "already in flight! Ignoring...\n";

    // Else, try to put the HTLC in flight and decrease capacity on the forward direction
    } else {
        if (!tryUpdatePaymentChannel(nextHop, htlc->getValue(), false)) {
            // Insufficient funds. Trigger UPDATE_FAIL_HTLC
            throw std::invalid_argument("ERROR: Could not commit UPDATE_ADD_HTLC. Reason: Insufficient funds.");
        }
        _paymentChannels[nextHop].setInFlight(htlcId, htlc);
        EV << "Payment hash " + paymentHash + " set in flight.\n";
    }
}

bool FullNode::isInFlight(HTLC *htlc, std::string nextHop) {
    // Checks if payment is already in flight

    if(!_paymentChannels[nextHop].getInFlight(htlc->getHtlcId()))
        return false;
    else
        return true;
}


std::vector <HTLC *> FullNode::getSortedPendingHTLCs (std::vector<HTLC *> HTLCs, std::string neighbor) {
    // Util function that receies a vector of HTLCs and sorts them according to the local order
    // (also discards HTLCs that are not in the pending list)
    std::deque<HTLC *> pendingHTLCsFIFO = _paymentChannels[neighbor].getPendingHTLCsFIFO();
    std::vector<HTLC *> sortedHTLCs;

    for (const auto & pendingHTLC : pendingHTLCsFIFO) {
        for (const auto & htlc : HTLCs) {
            std::string htlcId = htlc->getHtlcId();
           if (htlcId == pendingHTLC->getHtlcId()) {
               sortedHTLCs.push_back(htlc);
           }
        }
    }
    return sortedHTLCs;
}

std::string FullNode::createHTLCId (std::string paymentHash, int htlcType) {
    return paymentHash + ":" + std::to_string(htlcType);
}
