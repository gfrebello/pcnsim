#include "globals.h"
#include "crypto.h"
#include "update_add_htlc_m.h"
#include "update_fulfill_htlc_m.h"
#include "failHTLC_m.h"
#include "baseMessage_m.h"
#include "transaction_m.h"
#include "PaymentChannel.h"
#include "invoice_m.h"

class FullNode : public cSimpleModule {

    protected:
        virtual void initialize() override;
        virtual void handleMessage(cMessage *msg) override;
        virtual void forwardMessage(Transaction *msg);
        virtual void refreshDisplay() const;
//        virtual int getDestinationGate (int destination);

        std::map<std::string, std::string> _myPreImages; // paymentHash to preImage
        std::map<std::string, std::string> _myInFlights; // paymentHash to nodeName (who owes me)
        //std::map<std::string, int*> _inFlightsPath;
        std::map<std::string, cModule*> _senderModules; // PaymentHash to Module
        //virtual void setPreImage (std::string paymentHash, std::string preImage) { this->_myPreImages[paymentHash] = preImage; };
        //virtual std::string getPreImageByHash (std::string paymentHash) { return this->_myPreImages[paymentHash]; };
        //virtual void setMyInFlight (std::string paymentHash, int destination) { this->_myInFlights[paymentHash] = destination; };
        //virtual int getInFlightChannel (std::string paymentHash) { return this->_myInFlights[paymentHash]; };

        virtual BaseMessage* generateHTLC (BaseMessage *ttmsg, cModule *sender);
        virtual BaseMessage* handleInvoice(BaseMessage *ttmsg, cModule *sender);
        virtual BaseMessage* fulfillHTLC (BaseMessage *ttmsg, cModule *sender);

    public:
        bool _isFirstSelfMessage;
        typedef std::map<std::string, int> RoutingTable;  // nodeName to gateIndex
        RoutingTable rtable;
        std::map<std::string, PaymentChannel> _paymentChannels;
        std::map<std::string, int> _signals;
        cTopology *_localTopology;

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
    myName = getName();
    std::map<std::string, std::vector<std::tuple<std::string, double, simtime_t>>> localPendingTransactions = pendingTransactions;

    // Initialize payment channels
    for (auto& neighborToPC : globalPaymentChannels[myName]) {
        std::string neighborName = neighborToPC.first;
        std::tuple<double, double, double, cGate*> pcTuple = neighborToPC.second;
        double capacity = std::get<0>(pcTuple);
        double fee = std::get<1>(pcTuple);
        double quality = std::get<2>(pcTuple);
        int maxAcceptedHTLCs = std::get<3>(pcTuple);
        double HTLCMinimumMsat = std::get<4>(pcTuple);
        double channelReserveSatoshis = std::get<5>(pcTuple);
        cGate* neighborGate = std::get<3>(pcTuple);


        PaymentChannel pc = PaymentChannel(capacity, fee, quality, maxAcceptedHTLCs, HTLCMinimumMsat, channelReserveSatoshis, neighborGate);
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

    // Schedule transactions according to workload
    std::map<std::string, std::vector<std::tuple<std::string, double, simtime_t>>>::iterator it = pendingTransactions.find(myName);
    if (it != pendingTransactions.end()) {
        std::vector<std::tuple<std::string, double, simtime_t>> myWorkload = it->second;
        for (const auto& transactionTuple: myWorkload) {
             std::string dstName = std::get<0>(transactionTuple);
             double value = std::get<1>(transactionTuple);
             simtime_t time = std::get<2>(transactionTuple);
             char msgname[32];
             sprintf(msgname, "%s-to-%s;value:%0.1f", myName.c_str(), dstName.c_str(), value);
             Transaction *msg = new Transaction(msgname);
             msg->setSource(myName.c_str());
             msg->setDestination(dstName.c_str());
             msg->setValue(value);
             scheduleAt(simTime()+time, msg);
             _isFirstSelfMessage = true;
        }
    } else {
        EV << "No workload found for " << myName.c_str() << ".\n";
    }
}

void FullNode::handleMessage(cMessage *msg) {

    Transaction *ttmsg = check_and_cast<Transaction *>(msg);
    forwardMessage(ttmsg);
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

void FullNode::forwardMessage(Transaction *msg) {



    std::string dstName = msg->getDestination();
    std::string prevName = msg->getSenderModule()->getName();
    myName = getName();

    // Check if this node is the destination
    if (dstName == myName){
        EV << "Message reached destinaton at " << myName.c_str() << " after "
                << msg->getHopCount() << " hops. Finishing...\n";
        updatePaymentChannel(prevName, msg->getValue(), true);
        std::string signalName = myName + "-to-" + prevName + ":capacity";
        emit(_signals[signalName], _paymentChannels[prevName]._capacity);
        if (hasGUI()) {
            char text[64];
            sprintf(text, "Payment reached destination!");
            bubble(text);
        }
        delete msg;
        return;
    }

    // Check if there's a route to the destination
    RoutingTable::iterator it = rtable.find(dstName);
    if (it == rtable.end()) {
        EV << dstName << " unreachable, discarding packet " << msg->getName() << endl;
        delete msg;
        //return;
    }
    int outGateIndex = (*it).second;
    std::string nextName = gate("out",outGateIndex)->getPathEndGate()->getOwnerModule()->getName();

    // Update channel capacities and emit signals
    if (prevName != myName) { // Prevent self messages from interfering in channel capacities
        EV << "forwarding packet " << msg->getName() << " on gate index " << outGateIndex << endl;
        updatePaymentChannel(prevName, msg->getValue(), true);
        std::string signalName = myName + "-to-" + prevName + ":capacity";
        emit(_signals[signalName], _paymentChannels[prevName]._capacity);

        if(updatePaymentChannel(nextName, msg->getValue(), false)) {
            signalName = myName + "-to-" + nextName + ":capacity";
            emit(_signals[signalName],_paymentChannels[nextName]._capacity);
            msg->setHopCount(msg->getHopCount()+1);
            send(msg, "out", outGateIndex);
        } else { // Not enough capacity to forward. Fail the transaction.
            EV << "Not enough capacity to forward payment. Failing the transaction...\n";
            delete msg;
        }

    } else if(_isFirstSelfMessage == true) {

        if(updatePaymentChannel(nextName, msg->getValue(), false)) {
             std::string signalName = myName + "-to-" + nextName + ":capacity";
             emit(_signals[signalName],_paymentChannels[nextName]._capacity);
             _isFirstSelfMessage = false;
             msg->setHopCount(msg->getHopCount()+1);
             send(msg, "out", outGateIndex);
        } else {
            EV << "Not enough capacity to initialize payment. Failing the transaction...\n";
            delete msg;
        }
    } else {

    }
}

void FullNode::refreshDisplay() const {

    char pcText[100];
    sprintf(pcText, " ");
    for(auto& it : _paymentChannels) {
        char buf[20];
        std::string neighborName = it.first;
        float capacity = it.second.getCapacity();
        sprintf(buf, "%s: %0.1f,\n ", neighborName.c_str(), capacity);
        strcat(pcText,buf);
    }
    getDisplayString().setTagArg("t", 0, pcText);
}


/***********************************************************************************************************************/
/* generateHTLC creates a hashed time lock contract between two nodes that share a payment channel. The function       */
/* receives as inputs the payment hash that will lock the payment, the amount to be paid, and the next hop to forward  */
/* the message. The function saves the new created htlc as in-flight, which is a key-value map that indexes every htlc */
/* by the payment hash.                                                                                                */
/***********************************************************************************************************************/
BaseMessage* FullNode::generateHTLC(BaseMessage *ttmsg, cModule *sender) { // aka Handle add HTLC
    update_add_htlc *message = check_and_cast<update_add_htlc *> (ttmsg->getEncapsulatedPacket());

    std::string paymentHash;
    std::string source = getName();
    paymentHash.assign(message->getPaymentHash());
    double amount = message->getAmount();
    //int *hops = ttmsg->getHops();
    int hopCount = ttmsg->getHopCount();
    int destination = ttmsg->getHops(hopCount+1);

    double current_balance = _paymentChannels[destination].getCapacity();
    double htlc_minimum_msat = _paymentChannels[destination].getHTLCMinimumMSAT();
    int htlc_number = _paymentChannels[destination].getHTLCNumber();
    int max_acceptd_htlcs = _paymentChannels[destination].getMaxAcceptedHTLCs();
    double channelReserveSatoshis = _paymentChannels[destination].getChannelReserveSatoshis();

    //verifies if the channel follows all the requirements to support the new htlc
    if (current_balance == 0 || htlc_number + 1 > max_acceptd_htlcs || amount < htlc_minimum_msat || current_balance - amount < channelReserveSatoshis){
        return NULL;
    }

    // Decrease channel capacity and increase number of HTLCs in the channel
    _paymentChannels[destination].decreaseCapacity(amount); // Substitute for updatePaymentChannel
    _paymentChannels[destination].increaseHTLCNumber();

    //store the new created htlc
    _paymentChannels[destination].setInFlight(paymentHash, amount);
    //_myInFlights[paymentHash] = destination;
    //_inFlightsPath[paymentHash] = hops;
    _senderModules[paymentHash] = sender;

    update_add_htlc *htlc = new update_add_htlc();
    htlc->setSource(source);
    //htlc->setDestination(destination);
    htlc->setPaymentHash(paymentHash.c_str());
    htlc->setAmount(amount);
    //htlc->setHops(&hops);
    //htlc->setHopCount(hopCount+1);

    BaseMessage *newMessage = new BaseMessage();
    newMessage->setDestination(destination);
    newMessage->setMessageType(UPDATE_ADD_HTLC);
    newMessage->setHopCount(hopCount+1);

    newMessage.encapsute(htlc);

    return newMessage;

}

BaseMessage* FullNode::generateFailHTLC(BaseMessage *ttmsg, cModule *sender){
    std::string reason;

    reason.assign("Could not create HTLC");
    FailHTLC *failHTLC = new FailHTLC();
    failHTLC->setErrorReason(reason.c_str());

    BaseMessage *newMessage = new BaseMessage();
    newMessage->setDestination(sender->getIndex());
    newMessage->setMessageType(UPDATE_FAIL_HTLC);
    newMessage->setHopCount(0);

    newMessage.encapsulate(failHTLC);

    return newMessage;

}

//modify fulfillHTLC to analyse the preImage, update own channel, and forward message
BaseMessage* FullNode::fulfillHTLC(BaseMessage *ttmsg, cModule *sender){

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

//    if (_myInFlights[paymentHash] != senderName){
//        return NULL;
//    }

    double amount = _paymentChannels[senderName].getInFlight(paymentHash);
    cModule *prevNodeHTLC = _senderModules[paymentHash];
    std::string prevNodeName = prevNodeHTLC->getName();
    _paymentChannels[prevNodeName].increaseCapacity(amount);
    //sender->_paymentChannels[source].increaseCapacity(amount);

    _paymentChannels[senderName].removeInFlight(paymentHash);
    //_myInFlights.erase(paymentHash);

    update_fulfill_htlc *htlcPreImage = new update_fulfill_htlc();
    htlcPreImage->setPaymentHash(paymentHash.c_str());
    htlcPreImage->setPreImage(preImage.c_str());
    //htlcPreImage->setHopCount(hopCount+1);

    BaseMessage *fulfillMessage = new BaseMessage();
    fulfillMessage->setHopCount(hopCount+1);

    BaseMessage.encapsulate(htlcPreImage);

    return fulfillMessage;

}

BaseMessage* FullNode::handleInvoice (BaseMessage *ttmsg, cModule *sender){
    Invoice *invoice = check_and_cast<Invoice *> (ttmsg->getEncapsulatedPacket());

    double amount = invoice->getAmount();
    std::string paymentHash;
    paymentHash.assign(invoice->getPaymentHash());

    //add path finding function here

    update_add_htlc *first_htlc = new update_add_htlc();
    first_htlc->setSource(getName());
    first_htlc->setPaymentHash(paymentHash.c_str());
    first_htlc->setAmount(amount);

    BaseMessage *newMessage = new BaseMessage();
    newMessage->setDestination(ttmsg->getSource());
    newMessage->setHopCount(0);
    newMessage->setMessageType(UPDATE_ADD_HTLC);

    newMessage.encapsulate(first_htlc);
    return newMessage;

}

Invoice* FullNode::generateInvoice(std::string destination, double amount){
    // create channel with source
    std::string sourceName = getName();
    std::string preImage;
    std::string preImageHash;

    preImage = gen_random();
    preImageHash = sha256(preImage);

    _myPreImages[preImageHash] = preImage;

    Invoice *invoice = new Invoice();
    invoice->setDestination(destination);
    invoice->setAmount(amount);
    invoice->setPaymentHash(preImageHash.c_str());

    return invoice;

}


// getDestinationGate returns the gate associated with the destination received as argument
int FullNode::getDestinationGate (int destination){
    for (cModule::GateIterator i(this); !i.end(); i++) {
        cGate *gate = *i;
        owner = gate("out")->getOwnerModule()->getIndex();
        if (owner == destination){
            break;
        }
    }
    return owner;
}

