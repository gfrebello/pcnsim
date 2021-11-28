#include "globals.h"
#include "crypto.h"
#include "updateAddHTLC_m.h"
#include "updateFulfillHTLC_m.h"
#include "failHTLC_m.h"
#include "baseMessage_m.h"
#include "payment_m.h"
#include "PaymentChannel.h"
#include "invoice_m.h"
#include "commitmentSigned_m.h"
#include "revokeAndAck_m.h"

class FullNode : public cSimpleModule {

    protected:
        virtual void initialize() override;
        virtual void handleMessage(cMessage *msg) override;
        virtual void forwardMessage(BaseMessage *baseMsg);
        virtual void refreshDisplay() const;

        // Routing functions
        virtual std::vector<std::string> getPath(std::map<std::string, std::string> parents, std::string target);
        virtual std::string minDistanceNode (std::map<std::string, double> distances, std::map<std::string, bool> visited);
        virtual std::vector<std::string> dijkstraWeightedShortestPath (std::string src, std::string target, std::map<std::string, std::vector<std::pair<std::string, double> > > graph);
        virtual bool tryCommitTxOrFail(std::string, bool);

        std::map<std::string, std::string> _myPreImages; // paymentHash to preImage
        std::map<std::string, std::string> _myInFlights; // paymentHash to nodeName (who owes me)
        std::map<std::string, BaseMessage *> _myTransactions;

        //std::map<std::string, int*> _inFlightsPath;
        std::map<std::string, cModule*> _senderModules; // PaymentHash to Module
        //virtual void setPreImage (std::string paymentHash, std::string preImage) { this->_myPreImages[paymentHash] = preImage; };
        //virtual std::string getPreImageByHash (std::string paymentHash) { return this->_myPreImages[paymentHash]; };
        //virtual void setMyInFlight (std::string paymentHash, int destination) { this->_myInFlights[paymentHash] = destination; };
        //virtual int getInFlightChannel (std::string paymentHash) { return this->_myInFlights[paymentHash]; };

//        virtual BaseMessage* generateHTLC (BaseMessage *ttmsg, cModule *sender);
//        virtual BaseMessage* handleInvoice(BaseMessage *ttmsg, cModule *sender);
//        virtual BaseMessage* fulfillHTLC (BaseMessage *ttmsg, cModule *sender);
        virtual Invoice* generateInvoice(std::string srcName, double value);

    public:
        bool _isFirstSelfMessage;
        typedef std::map<std::string, int> RoutingTable;  // nodeName to gateIndex
        RoutingTable rtable;
        std::map<std::string, PaymentChannel> _paymentChannels;
        std::map<std::string, int> _signals;
        cTopology *_localTopology;
        int localCommitCounter;

        Json::Value paymentChannelstoJson();
        void printPaymentChannels();
        bool updatePaymentChannel(std::string nodeName, double value, bool increase);
};

// Define module and initialize random number generator
Define_Module(FullNode);

std::string myName;

// Util functions
void FullNode::printPaymentChannels() {

    EV << "Printing payment channels of node " << getIndex() << ":\n { ";
    for (auto& it : _paymentChannels) {
        EV << "{ '" << it.first << "': " << it.second.toJson().toStyledString();
    }
    EV << " }\n";
}


Json::Value FullNode::paymentChannelstoJson() {

    Json::Value json;
    std::map<std::string, PaymentChannel>::const_iterator it = _paymentChannels.begin(), end = _paymentChannels.end();
    for ( ; it != end; it++) {
        json[it->first] = it->second.toJson();
    }
    return json;
}


//Omnet++ functions

void FullNode::initialize() {

    // Get name (id) and initialize local topology based on the global topology created by netBuilder
    _localTopology = globalTopology;
    this->localCommitCounter = 0;
    myName = getName();
    std::map<std::string, std::vector<std::tuple<std::string, double, simtime_t>>> localPendingPayments = pendingPayments;

    // Initialize payment channels
    for (auto& neighborToPCs : nameToPCs[myName]) {
        std::string neighborName = neighborToPCs.first;
        std::tuple<double, double, double, int, double, double, cGate*> pcTuple = neighborToPCs.second;
        double capacity = std::get<0>(pcTuple);
        double fee = std::get<1>(pcTuple);
        double quality = std::get<2>(pcTuple);
        int maxAcceptedHTLCs = std::get<3>(pcTuple);
        int numHTLCs = 0;
        double HTLCMinimumMsat = std::get<4>(pcTuple);
        double channelReserveSatoshis = std::get<5>(pcTuple);
        cGate* neighborGate = std::get<6>(pcTuple);


        PaymentChannel pc = PaymentChannel(capacity, fee, quality, maxAcceptedHTLCs, numHTLCs, HTLCMinimumMsat, channelReserveSatoshis, neighborGate);
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

    BaseMessage *baseMsg = check_and_cast<BaseMessage *>(msg);
//    Payment *ttmsg = check_and_cast<Payment *>(msg);

    // Treat messages according to their message types
    switch(baseMsg->getMessageType()) {

        // Initialize payment
        case TRANSACTION_INIT: {
            Payment *trMsg = check_and_cast<Payment *> (baseMsg->decapsulate());
            EV << "TRANSACTION_INIT received. Starting payment "<< trMsg->getName() << "\n";

            // Create ephemeral communication channel with the payment source
            std::string srcName = trMsg->getSource();
            std::string srcPath = "PCN." + srcName;
            double value = trMsg->getValue();
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
            send(baseMsg, myGate);


            // Close ephemeral connection
            myGate->disconnect();
            break;
        }

        case INVOICE: {
            Invoice *invMsg = check_and_cast<Invoice *> (baseMsg->decapsulate());
            EV << "INVOICE received. Payment hash: " << invMsg->getPaymentHash() << "\n";

            //std::string dstName = invMsg->getSenderModule()->getName();
            std::string dstName = invMsg->getDestination();
            std::string srcName = getName();

            // Find route to destination
            std::vector<std::string> path = this->dijkstraWeightedShortestPath(srcName, dstName, adjMatrix);
            std::string firstHop = path[1];

            // Print route
            std::string printPath = "Full route to destination: ";
            for (auto hop: path)
                printPath = printPath + hop + ", ";
            printPath += "\n";
            EV << printPath;

            //Creating HTLC
            EV << "Creating HTLC to kick off the payment process \n";
            BaseMessage *newMessage = new BaseMessage();
            newMessage->setDestination(dstName.c_str());
            newMessage->setMessageType(UPDATE_ADD_HTLC);
            newMessage->setHopCount(1);
            newMessage->setHops(path);

            UpdateAddHTLC *firstHTLC = new UpdateAddHTLC();
            firstHTLC->setSource(srcName.c_str());
            firstHTLC->setPaymentHash(invMsg->getPaymentHash());
            firstHTLC->setValue(invMsg->getValue());

            _paymentChannels[firstHop].setPendingHTLC(invMsg->getPaymentHash(), firstHTLC);
            _paymentChannels[firstHop].setPendingHTLCFIFO(invMsg->getPaymentHash());

            //Decreasing channel capacity
            //_paymentChannels[firstHop].decreaseCapacity(invMsg->getValue());

            newMessage->encapsulate(firstHTLC);
            int gateIndex;
            gateIndex = rtable[firstHop];

            //Sending HTLC out
            EV << "Sending HTLC to " + firstHop + " through gate " + std::to_string(gateIndex) + " with payment hash " + firstHTLC->getPaymentHash() + "\n";
            send(newMessage,"out", gateIndex);

            break;
        }
        case UPDATE_ADD_HTLC: {

            EV << "UPDATE_ADD_HTLC received at " + std::string(getName()) + " from " + std::string(baseMsg->getSenderModule()->getName()) + ".\n";
            std::string srcName = getName();

            if (!baseMsg->isSelfMessage()){
                //node checks whether if it is the destination
                UpdateAddHTLC *HTLCMsg = check_and_cast<UpdateAddHTLC *> (baseMsg->decapsulate());
                std::string dstName = baseMsg->getDestination();


                //Geting the path of the message
                std::vector<std::string> path = baseMsg->getHops();

                std::string sender = baseMsg->getSenderModule()->getName();
                EV << "Storing HTLC from node " + sender + " as pending.\n";

                _paymentChannels[sender].setPendingHTLC(HTLCMsg->getPaymentHash(), HTLCMsg);
                _paymentChannels[sender].setPendingHTLCFIFO(HTLCMsg->getPaymentHash());
                _paymentChannels[sender].setPreviousHop(HTLCMsg->getPaymentHash(), sender);

                if (dstName == this->getName()){
                    EV << "Payment reached its destination. Releasing pre image \n";

                    _myTransactions[HTLCMsg->getPaymentHash()] = baseMsg;

                    //Getting the stored pre image
                    //std::string preImage = _myPreImages[HTLCMsg->getPaymentHash()];

                    //Generating a fulfill htlc message
                    //BaseMessage *fulfillMsg = new BaseMessage();
                    //fulfillMsg->setDestination(path[0].c_str());
                    //fulfillMsg->setMessageType(UPDATE_ADD_HTLC);
                    //fulfillMsg->setHopCount(baseMsg->getHopCount() + 1);
                    //fulfillMsg->setHops(path);

                    //UpdateFulfillHTLC *fulfillHTLC = new UpdateFulfillHTLC();
                    //fulfillHTLC->setPaymentHash(HTLCMsg->getPaymentHash());
                    //fulfillHTLC->setPreImage(preImage.c_str());

                    //_paymentChannels[path[baseMsg->getHopCount()]].increaseCapacity(HTLCMsg->getValue());


                    //fulfillMsg->encapsulate(fulfillHTLC);
                    //int gateIndex;
                    //gateIndex = rtable[path[fulfillMsg->getHopCount()]];

                    //Sending HTLC out
                    //EV << "Sending pre image " + preImage + " to " + path[(fulfillMsg->getHopCount())] + " through gate " + std::to_string(gateIndex) + "for payment hash " + HTLCMsg->getPaymentHash() + "\n";
                    //send(fulfillMsg,"out", gateIndex);
                    if (!tryCommitTxOrFail(sender, false)){
                        EV << "Setting timeout for node " + srcName + "\n";
                        scheduleAt((simTime() + SimTime(500,SIMTIME_MS)),baseMsg);
                    }

                    break;
                }


                std::string nextHop = path[baseMsg->getHopCount() + 1];
                std::string previousHop = path[baseMsg->getHopCount()-1];

                //Creating HTLC
                EV << "Creating HTLC to kick off the payment process \n";
                BaseMessage *newMessage = new BaseMessage();
                newMessage->setDestination(dstName.c_str());
                newMessage->setMessageType(UPDATE_ADD_HTLC);
                newMessage->setHopCount(baseMsg->getHopCount() + 1);
                newMessage->setHops(path);
                newMessage->setDisplayString("b=15,15,red,o=white,blue,1");
                newMessage->setName("UPDATE_ADD_HTLC");
                //newMessage->setUpstreamDirection(true);

                UpdateAddHTLC *HTLC = new UpdateAddHTLC();
                HTLC->setSource(srcName.c_str());
                HTLC->setPaymentHash(HTLCMsg->getPaymentHash());
                HTLC->setValue(HTLCMsg->getValue());

                //Decreasing channel capacity and storing in flight
                _paymentChannels[nextHop].setPendingHTLC(HTLC->getPaymentHash(),HTLC);
                _paymentChannels[nextHop].setPendingHTLCFIFO(HTLC->getPaymentHash());
                _paymentChannels[nextHop].setPreviousHop(HTLCMsg->getPaymentHash(), sender);
                _paymentChannels[previousHop].setInFlight(HTLCMsg->getPaymentHash(),HTLCMsg->getValue());

                newMessage->encapsulate(HTLC);
                int gateIndex;
                gateIndex = rtable[path[newMessage->getHopCount()]];

                //Sending HTLC out
                EV << "Sending HTLC to " + path[(newMessage->getHopCount())] + " through gate " + std::to_string(gateIndex) + " with payment hash " + HTLC->getPaymentHash() + "\n";
                send(newMessage,"out", gateIndex);

                if (!tryCommitTxOrFail(sender, false)){
                    EV << "Setting timeout for node " + srcName + "\n";
                    scheduleAt((simTime() + SimTime(500,SIMTIME_MS)),baseMsg);
                }
            }else{
                EV << srcName + " timeout expired. Creating commit\n";
                std::vector<std::string> path = baseMsg->getHops();
                std::string previousHop = path[baseMsg->getHopCount()-1];
                std::string dstName = baseMsg->getDestination();

                tryCommitTxOrFail(previousHop, true);
            }

            break;
        }
        case UPDATE_FAIL_HTLC:
            EV << "UPDATE_FAIL_HTLC received.\n";
            break;
        case UPDATE_FULFILL_HTLC:{
            UpdateFulfillHTLC *fulfillHTLC = check_and_cast<UpdateFulfillHTLC *>(baseMsg->decapsulate());
            EV << "UPDATE_FULFILL_HTLC received at " + std::string(getName()) + " from " + std::string(baseMsg->getSenderModule()->getName()) + ".\n";

            std::string preImage = fulfillHTLC->getPreImage();
            std::string dstName = baseMsg->getDestination();
            std::vector<std::string> path = baseMsg->getHops();

            if (sha256(preImage) != fulfillHTLC->getPaymentHash()){
                EV << "Failed to fulfill HTLC. Different hash value.\n";
                break;
            }
            if (getName() == dstName){
                EV << "Payment successfully achieved\n";
                break;
            }


            //Passing pre image across HTLC chain
            BaseMessage *newMessage = new BaseMessage();
            newMessage->setDestination(dstName.c_str());
            newMessage->setMessageType(UPDATE_FULFILL_HTLC);
            newMessage->setHopCount(baseMsg->getHopCount() - 1);
            newMessage->setHops(path);
            newMessage->setDisplayString("b=15,15,rect,o=white,blue,1");
            newMessage->setName("UPDATE_FULFILL_HTLC");

            EV << "Current path[getHopCount()]: " + path[baseMsg->getHopCount()] + "\n";
            UpdateFulfillHTLC *fulfillMsg = new UpdateFulfillHTLC();
            fulfillMsg->setPreImage(preImage.c_str());
            fulfillMsg->setPaymentHash(fulfillHTLC->getPaymentHash());

            //Updating channel capacity
            EV << "Increasing channel capacity\n";
            EV << "Hop count: " + std::to_string(baseMsg->getHopCount()) + "\n";
            std::string nextHop = path[baseMsg->getHopCount()-1];
            double value = _paymentChannels[nextHop].getInFlight(fulfillHTLC->getPaymentHash());
            //_paymentChannels[nextHop].increaseCapacity(value);
            updatePaymentChannel(nextHop, value, true);
            _paymentChannels[nextHop].removeInFlight(fulfillHTLC->getPaymentHash());
            std::string myName = getName();
            std::string signalName = myName +"-to-" + nextHop + ":capacity";
            emit(_signals[signalName], _paymentChannels[nextHop]._capacity);

            newMessage->encapsulate(fulfillMsg);
            int gateIndex;
            gateIndex =  rtable[path[baseMsg->getHopCount()-1]];
            //Sending pre image out
            EV << "Sending pre image to " + path[(baseMsg->getHopCount())-1] + "through gate " + std::to_string(gateIndex) + "with payment hash " + fulfillHTLC->getPaymentHash() + "\n";
            send(newMessage,"out", gateIndex);

            break;
        }
        case REVOKE_AND_ACK:{
            EV << "REVOKE_AND_ACK received at " + std::string(getName()) + " from " + std::string(baseMsg->getSenderModule()->getName()) + ".\n";
            revokeAndAck *ackMsg = check_and_cast<revokeAndAck *> (baseMsg->decapsulate());

            int ackId = ackMsg->getAckId();
            std::vector<UpdateAddHTLC *> HTLCs = _paymentChannels[baseMsg->getSenderModule()->getName()].getHTLCsWaitingForAck(ackId);
            //std::vector<UpdateAddHTLC *> HTLCs = sentCommitTx->getHTLCs();
            UpdateAddHTLC *HTLCIterator;
            std::string paymentHashIterator;
            int index2 = 0;
            int index = 0;
            int batchSize = _paymentChannels[baseMsg->getSenderModule()->getName()].getPendingBatchSize();

            for (index = 0; index < HTLCs.size(); index++){
                HTLCIterator = HTLCs[index];
                _paymentChannels[baseMsg->getSenderModule()->getName()].removePendingHTLC(HTLCIterator->getPaymentHash());
                for (index2 = 0; index2 < batchSize; index2++){
                    paymentHashIterator.assign(_paymentChannels[baseMsg->getSenderModule()->getName()].getPendingHTLCFIFO());
                    _paymentChannels[baseMsg->getSenderModule()->getName()].removePendingHTLCFIFO();
                    if (paymentHashIterator != HTLCIterator->getPaymentHash()){
                        _paymentChannels[baseMsg->getSenderModule()->getName()].setPendingHTLCFIFO(paymentHashIterator);
                    }
                }
                if (_myPreImages[HTLCIterator->getPaymentHash()] != ""){
                    EV << "Payment reached its destination. Releasing pre image \n";

                    //Getting the stored pre image
                    std::string preImage = _myPreImages[HTLCIterator->getPaymentHash()];
                    BaseMessage *storedBaseMsg = _myTransactions[HTLCIterator->getPaymentHash()];
                    std::vector<std::string> path = storedBaseMsg->getHops();

                    //Generating a fulfill htlc message
                    BaseMessage *fulfillMsg = new BaseMessage();
                    fulfillMsg->setDestination(path[0].c_str());
                    fulfillMsg->setMessageType(UPDATE_FULFILL_HTLC);
                    fulfillMsg->setHopCount(storedBaseMsg->getHopCount() - 1);
                    fulfillMsg->setHops(storedBaseMsg->getHops());

                    UpdateFulfillHTLC *fulfillHTLC = new UpdateFulfillHTLC();
                    fulfillHTLC->setPaymentHash(HTLCIterator->getPaymentHash());
                    fulfillHTLC->setPreImage(preImage.c_str());

                    //_paymentChannels[path[storedBaseMsg->getHopCount()]].increaseCapacity(HTLCIterator->getValue());
                    updatePaymentChannel(path[storedBaseMsg->getHopCount()-1], HTLCIterator->getValue(), true);
                    std::string myName = getName();
                    std::string signalName = myName +"-to-" + path[storedBaseMsg->getHopCount()-1] + ":capacity";
                    emit(_signals[signalName], _paymentChannels[path[storedBaseMsg->getHopCount()-1]]._capacity);

                    fulfillMsg->encapsulate(fulfillHTLC);
                    int gateIndex;
                    gateIndex = rtable[path[storedBaseMsg->getHopCount()-1]];

                    _myPreImages.erase(HTLCIterator->getPaymentHash());
                    _myTransactions.erase(HTLCIterator->getPaymentHash());

                    //Sending HTLC out
                    EV << "Sending pre image " + preImage + " to " + path[(fulfillMsg->getHopCount()-1)] + " through gate " + std::to_string(gateIndex) + "for payment hash " + HTLCIterator->getPaymentHash() + "\n";
                    send(fulfillMsg,"out", gateIndex);

                }
            }
            _paymentChannels[baseMsg->getSenderModule()->getName()].removeHTLCsWaitingForAck(ackId);

            break;
        }
        case COMMITMENT_SIGNED:{
            EV << "COMMITMENT_SIGNED received at " + std::string(getName()) + " from " + std::string(baseMsg->getSenderModule()->getName()) + ".\n";
            commitmentSigned *commitMsg = check_and_cast<commitmentSigned *>(baseMsg->decapsulate());

            UpdateAddHTLC *HTLCIterator = NULL;
            std::string paymentHashIterator;
            std::string paymentHashIterator2;
            unsigned short index = 0;
            std::vector<UpdateAddHTLC *> HTLCs = commitMsg->getHTLCs();
            size_t numberHTLCs = HTLCs.size();
            std::string sender = baseMsg->getSenderModule()->getName();

            for (index = 0; index < numberHTLCs; index++){
                HTLCIterator = HTLCs[index];
                paymentHashIterator = HTLCIterator->getPaymentHash();
                if (!_paymentChannels[sender].getPendingHTLC(paymentHashIterator)){
                    EV << "Error. Unknown updateAddHTLC received\n";
                    break;
                }

                if (_paymentChannels[sender].getPreviousHop(paymentHashIterator) != sender){
                    EV << "Decreasing " + std::to_string(HTLCIterator->getValue()) + "at node " + getName() + "for payment hash " + HTLCIterator->getPaymentHash() + "\n";

                    _paymentChannels[sender].setInFlight(paymentHashIterator, HTLCIterator->getValue());
                    //_paymentChannels[sender].decreaseCapacity(HTLCIterator->getValue());
                    if (!updatePaymentChannel(sender, HTLCIterator->getValue(), false)) {
                        bubble("Channel depleted. Failing payment... \n");
                        EV << "Channel depleted. Failing payment... \n";
                    }

                    _paymentChannels[sender].removePreviousHop(paymentHashIterator);
                }
                _paymentChannels[sender].removePendingHTLC(HTLCIterator->getPaymentHash());
                for (int index2 = 0; index < _paymentChannels[sender].getPendingBatchSize(); index2++){
                    paymentHashIterator2.assign(_paymentChannels[sender].getPendingHTLCFIFO());
                    _paymentChannels[sender].removePendingHTLCFIFO();
                    if (paymentHashIterator2 != HTLCIterator->getPaymentHash()){
                        _paymentChannels[sender].setPendingHTLCFIFO(paymentHashIterator);
                    }
                }
            }

            std::string myName = getName();
            std::string signalName = myName +"-to-" + sender + ":capacity";
            emit(_signals[signalName], _paymentChannels[sender]._capacity);

            revokeAndAck *ack = new revokeAndAck();
            ack->setAckId(commitMsg->getId());

            BaseMessage *newMsg = new BaseMessage();
            newMsg->setDestination(sender.c_str());
            newMsg->setMessageType(REVOKE_AND_ACK);
            newMsg->setHopCount(0);
            //newMsg->setUpstreamDirection(!baseMsg->getUpstreamDirection());

            newMsg->encapsulate(ack);
            int gateIndex;
            gateIndex =  rtable[sender];
            //Sending pre image out
            EV << "Sending ack to " + sender + "through gate " + std::to_string(gateIndex) + "with id " + std::to_string(commitMsg->getId()) + "\n";
            send(newMsg,"out", gateIndex);

            break;
        }
    }

}


// Helper function to update payment channels. If increase = true, the function attemps to increase
// the capacity of the channel. Else, check whether the channel has enough capacity to process the payment.
bool FullNode::updatePaymentChannel (std::string nodeName, double value, bool increase) {
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

void FullNode::forwardMessage(BaseMessage *baseMsg) {

//    std::string dstName = msg->getDestination();
//    std::string prevName = msg->getSenderModule()->getName();
//    myName = getName();
//
//    // Check if this node is the destination
//    if (dstName == myName){
//        EV << "Message reached destinaton at " << myName.c_str() << " after "
//                << msg->getHopCount() << " hops. Finishing...\n";
//        updatePaymentChannel(prevName, msg->getValue(), true);
//        std::string signalName = myName + "-to-" + prevName + ":capacity";
//        emit(_signals[signalName], _paymentChannels[prevName]._capacity);
//        if (hasGUI()) {
//            char text[64];
//            sprintf(text, "Payment reached destination!");
//            bubble(text);
//        }
//        delete msg;
//        return;
//    }
//
//    // Check if there's a route to the destination
//    RoutingTable::iterator it = rtable.find(dstName);
//    if (it == rtable.end()) {
//        EV << dstName << " unreachable, discarding packet " << msg->getName() << endl;
//        delete msg;
//        //return;
//    }
//    int outGateIndex = (*it).second;
//    std::string nextName = gate("out",outGateIndex)->getPathEndGate()->getOwnerModule()->getName();
//
//    // Update channel capacities and emit signals
//    if (prevName != myName) { // Prevent self messages from interfering in channel capacities
//        EV << "forwarding packet " << msg->getName() << " on gate index " << outGateIndex << endl;
//        updatePaymentChannel(prevName, msg->getValue(), true);
//        std::string signalName = myName + "-to-" + prevName + ":capacity";
//        emit(_signals[signalName], _paymentChannels[prevName]._capacity);
//
//        if(updatePaymentChannel(nextName, msg->getValue(), false)) {
//            signalName = myName + "-to-" + nextName + ":capacity";
//            emit(_signals[signalName],_paymentChannels[nextName]._capacity);
//            msg->setHopCount(msg->getHopCount()+1);
//            send(msg, "out", outGateIndex);
//        } else { // Not enough capacity to forward. Fail the payment.
//            EV << "Not enough capacity to forward payment. Failing the payment...\n";
//            delete msg;
//        }
//
//    } else if(_isFirstSelfMessage == true) {
//
//        if(updatePaymentChannel(nextName, msg->getValue(), false)) {
//             std::string signalName = myName + "-to-" + nextName + ":capacity";
//             emit(_signals[signalName],_paymentChannels[nextName]._capacity);
//             _isFirstSelfMessage = false;
//             msg->setHopCount(msg->getHopCount()+1);
//             send(msg, "out", outGateIndex);
//        } else {
//            EV << "Not enough capacity to initialize payment. Failing the payment...\n";
//            delete msg;
//        }
//    } else {
//
//    }
}

void FullNode::refreshDisplay() const {

    char pcText[100];
    sprintf(pcText, " ");
    for(auto& it : _paymentChannels) {
        char buf[100];
        std::string neighborName = it.first;
        float capacity = it.second.getCapacity();
        sprintf(buf, "%s: %0.1f,\n ", neighborName.c_str(), capacity);
        strcat(pcText,buf);
    }
    getDisplayString().setTagArg("t", 0, pcText);
}

/***********************************************************************************************************************/
/* tryCommitOrFail verifies whether the pending transactions queue has reached the defined commitment batch size       */
/* or not. If the batch is full, tryCommitOrFail creates a commitment_signed message and sends it to the node that     */
/* shares the payment channel where the batch is full. If the batch has not yet reached its limit, tryCommitOrFail     */
/* does nothing.                                                                                                       */
/***********************************************************************************************************************/
bool FullNode::tryCommitTxOrFail(std::string sender, bool timeoutFlag) {
    unsigned short int index = 0;
    UpdateAddHTLC *htlc = NULL;
    std::vector<UpdateAddHTLC *> HTLCVector;
    std::string paymentHashIterator;
    bool through = false;

    EV << "Entered tryCommitTxOrFail. Current batch size: " + std::to_string(_paymentChannels[sender].getPendingBatchSize()) + "\n";

    if (_paymentChannels[sender].getPendingBatchSize() >= COMMITMENT_BATCH_SIZE || timeoutFlag == true){
        for (index = 0; index < (_paymentChannels[sender].getPendingBatchSize()); index++){
            paymentHashIterator.assign(_paymentChannels[sender].getPendingHTLCFIFO());
            htlc = _paymentChannels[sender].getPendingHTLC(paymentHashIterator);
            //_paymentChannels[sender].removePendingHTLC(paymentHashIterator);
            _paymentChannels[sender].removePendingHTLCFIFO();
            _paymentChannels[sender].setPendingHTLCFIFO(paymentHashIterator);
            HTLCVector.push_back(htlc);

        }

        EV << "Setting through to true\n";
        through = true;

        commitmentSigned *commitTx = new commitmentSigned();
        commitTx->setHTLCs(HTLCVector);
        commitTx->setId(localCommitCounter);

        _paymentChannels[sender].setHTLCsWaitingForAck(localCommitCounter, HTLCVector);

        localCommitCounter = localCommitCounter + 1;
        int gateIndex = rtable[sender];

        BaseMessage *baseMsg = new BaseMessage();
        baseMsg->setDestination(sender.c_str());
        baseMsg->setMessageType(COMMITMENT_SIGNED);
        baseMsg->setHopCount(0);


        baseMsg->encapsulate(commitTx);

        EV << "Sending Commitment Signed from node " + std::string(getName()) + "to " + sender + ". localCommitCounter: " + std::to_string(localCommitCounter) + "\n";

        send (baseMsg,"out", gateIndex);
    }
    return through;
}


/***********************************************************************************************************************/
/* generateHTLC creates a hashed time lock contract between two nodes that share a payment channel. The function       */
/* receives as inputs the payment hash that will lock the payment, the amount to be paid, and the next hop to forward  */
/* the message. The function saves the new created htlc as in-flight, which is a key-value map that indexes every htlc */
/* by the payment hash.                                                                                                */
/***********************************************************************************************************************/
//BaseMessage* FullNode::generateHTLC(BaseMessage *ttmsg, cModule *sender) { // aka Handle add HTLC
//    updateAddHTLC *message = check_and_cast<updateAddHTLC *> (ttmsg->getEncapsulatedPacket());
//
//    std::string paymentHash;
//    std::string source = getName();
//    paymentHash.assign(message->getPaymentHash());
//    double amount = message->getAmount();
//    //int *hops = ttmsg->getHops();
//    int hopCount = ttmsg->getHopCount();
//    int destination = ttmsg->getHops(hopCount+1);
//
//    double current_balance = _paymentChannels[destination].getCapacity();
//    double htlc_minimum_msat = _paymentChannels[destination].getHTLCMinimumMSAT();
//    int htlc_number = _paymentChannels[destination].getHTLCNumber();
//    int max_acceptd_htlcs = _paymentChannels[destination].getMaxAcceptedHTLCs();
//    double channelReserveSatoshis = _paymentChannels[destination].getChannelReserveSatoshis();
//
//    //verifies if the channel follows all the requirements to support the new htlc
//    if (current_balance == 0 || htlc_number + 1 > max_acceptd_htlcs || amount < htlc_minimum_msat || current_balance - amount < channelReserveSatoshis){
//        return NULL;
//    }
//
//    // Decrease channel capacity and increase number of HTLCs in the channel
//    _paymentChannels[destination].decreaseCapacity(amount); // Substitute for updatePaymentChannel
//    _paymentChannels[destination].increaseHTLCNumber();
//
//    //store the new created htlc
//    _paymentChannels[destination].setInFlight(paymentHash, amount);
//    //_myInFlights[paymentHash] = destination;
//    //_inFlightsPath[paymentHash] = hops;
//    _senderModules[paymentHash] = sender;
//
//    updateAddHTLC *htlc = new updateAddHTLC();
//    htlc->setSource(source);
//    //htlc->setDestination(destination);
//    htlc->setPaymentHash(paymentHash.c_str());
//    htlc->setAmount(amount);
//    //htlc->setHops(&hops);
//    //htlc->setHopCount(hopCount+1);
//
//    BaseMessage *newMessage = new BaseMessage();
//    newMessage->setDestination(destination);
//    newMessage->setMessageType(UPDATE_ADD_HTLC);
//    newMessage->setHopCount(hopCount+1);
//
//    newMessage.encapsute(htlc);
//
//    return newMessage;
//
//}
//
//BaseMessage* FullNode::generateFailHTLC(BaseMessage *ttmsg, cModule *sender){
//    std::string reason;
//
//    reason.assign("Could not create HTLC");
//    FailHTLC *failHTLC = new FailHTLC();
//    failHTLC->setErrorReason(reason.c_str());
//
//    BaseMessage *newMessage = new BaseMessage();
//    newMessage->setDestination(sender->getIndex());
//    newMessage->setMessageType(UPDATE_FAIL_HTLC);
//    newMessage->setHopCount(0);
//
//    newMessage.encapsulate(failHTLC);
//
//    return newMessage;
//
//}
//
////modify fulfillHTLC to analyse the preImage, update own channel, and forward message
/*BaseMessage* FullNode::fulfillHTLC(BaseMessage *ttmsg, cModule *sender){

    update_fulfill_htlc *message = check_and_cast<update_fulfill_htlc *> (ttmsg->getEncapsulatedPacket());
    int hopCount = ttmsg->getHopCount();

    std::string paymentHash;
    std::string preImage;

    paymentHash.assign(message->getPaymentHash());
    preImage.assign(message->getPreImage());
    std::string preImageHash = sha256(preImage);

    if (preImageHash != paymentHash){
        return NULL;
    }

    std::string source = getName();
    std::string senderName = sender->getName();

    if (_myInFlights[paymentHash] != senderName){
        return NULL;
    }

    double amount = _paymentChannels[senderName].getInFlight(paymentHash);
    cModule *prevNodeHTLC = _senderModules[paymentHash];
    std::string prevNodeName = prevNodeHTLC->getName();
    _paymentChannels[prevNodeName].increaseCapacity(amount);
    sender->_paymentChannels[source].increaseCapacity(amount);

    _paymentChannels[senderName].removeInFlight(paymentHash);
    _myInFlights.erase(paymentHash);

    update_fulfill_htlc *htlcPreImage = new update_fulfill_htlc();
    htlcPreImage->setPaymentHash(paymentHash.c_str());
    htlcPreImage->setPreImage(preImage.c_str());
    htlcPreImage->setHopCount(hopCount+1);

    BaseMessage *fulfillMessage = new BaseMessage();
    fulfillMessage->setHopCount(hopCount+1);

    BaseMessage.encapsulate(htlcPreImage);

    return fulfillMessage;

}*/
//
//BaseMessage* FullNode::handleInvoice (BaseMessage *ttmsg, cModule *sender){
//    Invoice *invoice = check_and_cast<Invoice *> (ttmsg->getEncapsulatedPacket());
//
//    double value = invoice->getValue();
//    std::string paymentHash;
//    paymentHash.assign(invoice->getPaymentHash());
//
//
//    updateAddHTLC *first_htlc = new updateAddHTLC();
//    first_htlc->setSource(getName());
//    first_htlc->setPaymentHash(paymentHash.c_str());
//    first_htlc->setValue(value);
//
//    BaseMessage *newMessage = new BaseMessage();
//    newMessage->setDestination(sender->getName());
//    newMessage->setHopCount(0);
//    newMessage->setMessageType(UPDATE_ADD_HTLC);
//    newMessage->setHops();
//
//    newMessage.encapsulate(first_htlc);
//    return newMessage;
//
//}
//


Invoice* FullNode::generateInvoice(std::string srcName, double value) {

    //std::string srcName = getName();
    std::string preImage;
    std::string preImageHash;

    //preImage = genRandom();
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

// Selects the node with minimum distance out of a distances list while disregarding visited nodes
std::string FullNode::minDistanceNode (std::map<std::string, double> distances, std::map<std::string, bool> visited) {

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

// Return the path to source given a target and its parent nodes
std::vector<std::string> FullNode::getPath (std::map<std::string, std::string> parents, std::string target) {

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

// This function returns the Dijkstra's shortest path from a source to some target given an adjacency matrix
std::vector<std::string> FullNode::dijkstraWeightedShortestPath (std::string src, std::string target, std::map<std::string, std::vector<std::pair<std::string, double> > > graph) {

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


//
//
//// getDestinationGate returns the gate associated with the destination received as argument
//int FullNode::getDestinationGate (int destination){
//    for (cModule::GateIterator i(this); !i.end(); i++) {
//        cGate *gate = *i;
//        owner = gate("out")->getOwnerModule()->getIndex();
//        if (owner == destination){
//            break;
//        }
//    }
//    return owner;
//}
//