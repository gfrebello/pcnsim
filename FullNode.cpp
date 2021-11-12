#include "globals.h"
#include "transaction_m.h"
#include "PaymentChannel.h"

class FullNode : public cSimpleModule {

    protected:
        virtual void initialize() override;
        virtual void handleMessage(cMessage *msg) override;
        virtual void forwardMessage(Transaction *msg);
        virtual void refreshDisplay() const;

    public:
        typedef std::map<std::string, int> RoutingTable;  // nodeName -> gateindex
        RoutingTable rtable;
        std::map<std::string, PaymentChannel> _paymentChannels;
        std::map<std::string, int> _signals;
        bool _isFirstSelfMessage;
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
        cGate* neighborGate = std::get<3>(pcTuple);
        PaymentChannel pc = PaymentChannel(capacity, fee, quality, neighborGate);
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
