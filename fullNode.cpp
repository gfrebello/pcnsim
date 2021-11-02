#include "globals.h"
#include "transaction_m.h"
#include "PaymentChannel.h"

class FullNode : public cSimpleModule {

    protected:
        virtual void initialize() override;
        virtual Transaction *generateMessage();
        virtual void handleMessage(cMessage *msg) override;
        virtual void forwardMessage(Transaction *msg);
        virtual void refreshDisplay() const;

    public:
        std::map<int, PaymentChannel> _paymentChannels;
        std::map<std::string, int> _signals;

        Json::Value paymentChannelstoJson();
        void printPaymentChannels();
};

// Define module and initialize random number generator
Define_Module(FullNode);
std::random_device rd;
std::mt19937 gen(rd()); // seed the generator

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
    std::map<int, PaymentChannel>::const_iterator it = _paymentChannels.begin(), end = _paymentChannels.end();
    for ( ; it != end; it++) {
        json[it->first] = it->second.toJson();
    }
    return json;
}


//Omnet++ functions

void FullNode::initialize() {

    // Create payment channels and register them as signals/statistics
    for (int i=0; i < gateSize("out"); i++) {

        // Initialize payment channels
        cGate *neighborGate = gate("out", i)->getPathEndGate();
        PaymentChannel pc = PaymentChannel(neighborGate);
        int neighborIndex = neighborGate->getOwnerModule()->getIndex();
        _paymentChannels[neighborIndex] = pc;

        // Register signals and statistics
        std::string signalName = "node" + std::to_string(getIndex()) + "-to-node" + std::to_string(neighborIndex) + ":balance";
        simsignal_t signal = registerSignal(signalName.c_str());
        _signals[signalName] = signal;
        emit(_signals[signalName], _paymentChannels[neighborIndex]._capacity);

        char statisticName[32];
        sprintf(statisticName, "node%d-to-node%d:balance", getIndex(), neighborIndex);
        cProperty *statisticTemplate = getProperties()->get("statisticTemplate", "pcBalances");
        getEnvir()->addResultRecorders(this, signal, statisticName, statisticTemplate);

    }

    // Send initial payment from node 0
    if( getIndex() == 0 ) {
        Transaction *msg = generateMessage();
        EV << "Sending initial payment\n";
        scheduleAt(0.0, msg);
    }
}

Transaction* FullNode::generateMessage() {

    // Produce source and destination addresses.
    int src = getIndex();
    int n = getVectorSize();
    //std::random_device rd;
    //std::mt19937 gen(rd()); // seed the generator
    std::uniform_int_distribution<> distr(0, n-2); // define the range
    int dest = distr(gen);
    if (dest == src) dest++;
    double value = 1;
    char msgname[20];
    sprintf(msgname, "%d-to-%d;value:%0.1f", src, dest,value);

    // Create message object and set source and destination field.
    Transaction *msg = new Transaction(msgname);
    msg->setSource(src);
    msg->setDestination(dest);
    msg->setValue(value);
    return msg;
}


void FullNode::handleMessage(cMessage *msg) {

    Transaction *ttmsg = check_and_cast<Transaction *>(msg);

    // Increase channel capacity and emit signal after receiving payment
    int prevNode = ttmsg->getSenderModule()->getIndex();
    if (prevNode != this->getIndex()) { // Prevent self messages from interfering in PCs
        _paymentChannels[prevNode].increaseCapacity(ttmsg->getValue());
        std::string signalName = "node" + std::to_string(getIndex()) + "-to-node" + std::to_string(prevNode) + ":balance";
        std::map<std::string, int> signals = _signals;
        std::map<std::string, int>::iterator it = _signals.find(signalName);
        emit(_signals[signalName],_paymentChannels[prevNode]._capacity);
    }

    if (ttmsg->getDestination() == getIndex()){
        EV << "Message reached destinaton at node " << getIndex() << " after " << ttmsg->getHopCount() << " hops. Finishing...\n";
        delete msg;
    }
    else {
        forwardMessage(ttmsg);
    }
}

void FullNode::forwardMessage(Transaction *msg) {

    // Forward message to random gate
    int prevNode = msg->getSenderModule()->getIndex();
    int n = gateSize("out");
    std::uniform_int_distribution<> distr(0, n-1); // define the range
    int k = distr(gen);
    int nextNode = gate("out",k)->getPathEndGate()->getOwnerModule()->getIndex();

    // Allow or deny returns to the last hop
    if(ALLOW_RETURNS == false) {
        while ((nextNode == getIndex())||(nextNode==prevNode)) { // Prevent returns and self-messages
            k = distr(gen);
            nextNode = gate("out",k)->getPathEndGate()->getOwnerModule()->getIndex();
        }
    } else {
        while (nextNode == getIndex()) { // Prevent only self-messages
            k = distr(gen);
            nextNode = gate("out",k)->getPathEndGate()->getOwnerModule()->getIndex();
        }
    }

    msg->setHopCount(msg->getHopCount()+1);
    send(msg, "out", k);

    // Decrease channel capacity and emit signal after forwarding payment
    _paymentChannels[nextNode].decreaseCapacity(msg->getValue());
    char signalName[32];
    sprintf(signalName, "node%d-to-node%d:balance", getIndex(), nextNode);
    emit(_signals[signalName], _paymentChannels[nextNode]._capacity);
}


void FullNode::refreshDisplay() const {
    char pcText[100];
    sprintf(pcText, "{ ");
    for(auto& i : _paymentChannels) {
        char buf[20];
        sprintf(buf, "[%d]: %0.1f, ", i.first, i.second.getCapacity());
        strcat(pcText,buf);
    }
    strcat(pcText," }");
    getDisplayString().setTagArg("t", 0, pcText);
}
