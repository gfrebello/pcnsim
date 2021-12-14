#include "globals.h"
#include <algorithm>

cTopology *globalTopology = new cTopology("globalTopology");
std::map< std::string, std::vector< std::tuple<std::string, double, simtime_t> > > pendingPayments;
std::map< std::string, std::map<std::string, std::tuple<double, double, double, int, double, double, cGate*, cGate*> > > nameToPCs;
std::map< std::string, std::vector< std::pair<std::string, std::vector<double> > > > adjMatrix;

class NetBuilder : public cSimpleModule {
    public:
        virtual void initialize() override;
        virtual void handleMessage(cMessage *msg) override;
        void buildNetwork(cModule *parent);
        void initWorkload();
        void connect(cGate *src, cGate *dst, double linkDelay);
        bool nodeExists(std::map<int, cModule*> nodeList, int nodeId);
};

Define_Module(NetBuilder);

void NetBuilder::initialize() {
    // build the network in event 1, because it is undefined whether the simkernel
    // will implicitly initialize modules created *during* initialization, or this needs
    // to be done manually.
    scheduleAt(0, new cMessage());
}

void NetBuilder::handleMessage(cMessage *msg) {
    if (!msg->isSelfMessage())
        throw cRuntimeError("This module does not process messages.");

    delete msg;
    buildNetwork(getParentModule());
}

void NetBuilder::connect(cGate *srcGate, cGate *dstGate, double linkDelay) {

    cDelayChannel *channel = cDelayChannel::create("channel");
    channel->setDelay(linkDelay);
    srcGate->connectTo(dstGate, channel);
}

bool NetBuilder::nodeExists(std::map<int, cModule*> nodeList, int nodeId) {
    std::map<int, cModule*>::iterator it = nodeList.find(nodeId);
    if (it != nodeList.end())
        return true;
    else
        return false;
}

void NetBuilder::initWorkload() {
    std::map<int, cMessage*> paymentList;
    std::string line;
    std::ifstream workloadFile(par("workloadFile").stringValue(), std::ifstream::in);
    pendingPayments.clear();

    EV << "Initializing workload from file: " << par("topologyFile").stringValue() << "\n";

    while (getline(workloadFile, line, '\n')) {

        // Skip headers and empty lines
        if (line.empty() || line[0] == '#')
            continue;

        // Check whether all fields are present
        std::vector<std::string> tokens = cStringTokenizer(line.c_str()).asVector();
        if (tokens.size() != 4)
            throw cRuntimeError("wrong line in topology file: 4 items required, line: \"%s\"", line.c_str());

        // Get fields from tokens
        int srcId = atoi(tokens[0].c_str());
        int dstId = atoi(tokens[1].c_str());
        double value = atof(tokens[2].c_str());
        simtime_t time = atoi(tokens[3].c_str());

        // Print found edges
        EV << "PAYMENT FOUND: (" << srcId << ", " << dstId << "); Value = " << value << ". Processing...\n";

        // Add payments to global map (index by the destination because it facilitates sending the invoice later)
        std::string srcName = "node" + std::to_string(srcId);
        std::string dstName = "node" + std::to_string(dstId);
        auto paymentTuple = std::make_tuple(srcName, value, time);
        pendingPayments[dstName].push_back(paymentTuple);
    }

}

void NetBuilder::buildNetwork(cModule *parent) {

    // Initialize workload
    initWorkload();

    // Initialize variables and build network
    std::map<int, cModule *> nodeIdToMod;
    std::string line;
    std::string modClassName = "FullNode";
    std::ifstream topologyFile(par("topologyFile").stringValue(), std::ifstream::in);
    cModule *srcMod;
    cModule *dstMod;
    cTopology::Node *srcNode;
    cTopology::Node *dstNode;
    std::vector<std::tuple<cTopology::Link*, cGate*, cGate*>> linksBuffer;

    EV << "Building network from file: " << par("topologyFile").stringValue() << "\n";

    while (getline(topologyFile, line, '\n')) {

        // Skip headers and empty lines
        if (line.empty() || line[0] == '#')
            continue;

        // Check whether all fields are present
        std::vector<std::string> tokens = cStringTokenizer(line.c_str()).asVector();
        if (tokens.size() != 9)
            throw cRuntimeError("wrong line in topology file: 9 items required, line: \"%s\"", line.c_str());

        // Get fields from tokens
        int srcId = atoi(tokens[0].c_str());
        int dstId = atoi(tokens[1].c_str());
        double capacity = atof(tokens[2].c_str());
        double fee = atof(tokens[3].c_str());
        double linkQuality = atof(tokens[4].c_str());
        int maxAcceptedHTLCs = atoi(tokens[5].c_str());
        double HTLCMinimumMsat = atof(tokens[6].c_str());
        double channelReserveSatoshis = atof(tokens[7].c_str());
        double linkDelay = atof(tokens[8].c_str());

        // Print found edges
        EV << "EDGE FOUND: (" << srcId << ", " << dstId << "); linkDelay = " << linkDelay << "ms. Processing...\n";

        // Define module names
        std::string srcName = "node" + std::to_string(srcId);
        std::string dstName = "node" + std::to_string(dstId);

        // Check if the source module exists and if not, create a new source module
        if(!nodeExists(nodeIdToMod,srcId)) {
            cModuleType *srcModType = cModuleType::find(modClassName.c_str());
            if (!srcModType)
                throw cRuntimeError("Source module class `%s' not found", modClassName.c_str());
            srcMod = srcModType->create(srcName.c_str(), parent);
            nodeIdToMod[srcId] = srcMod;
            srcMod->finalizeParameters();
            srcNode = new cTopology::Node(srcMod->getId());
            globalTopology->addNode(srcNode);
        } else {
            srcMod = nodeIdToMod.find(srcId)->second;
            srcNode = globalTopology->getNodeFor(srcMod);
        }

        // Check if the destination module exists and if not, create a new destination module
        if(!nodeExists(nodeIdToMod,dstId)) {
            cModuleType *dstModType = cModuleType::find(modClassName.c_str());
            if (!dstModType)
                throw cRuntimeError("Destination module class `%s' not found", modClassName.c_str());
            dstMod = dstModType->create(dstName.c_str(), parent);
            nodeIdToMod[dstId] = dstMod;
            dstMod->finalizeParameters();
            dstNode = new cTopology::Node(dstMod->getId());
            globalTopology->addNode(dstNode);
        } else {
            dstMod = nodeIdToMod.find(dstId)->second;
            dstNode = globalTopology->getNodeFor(dstMod);
        }

        // Connect modules
        cGate *srcOut, *dstIn;
        srcOut = srcMod->getOrCreateFirstUnconnectedGate("out", 0, false, true);
        dstIn = dstMod->getOrCreateFirstUnconnectedGate("in", 0, false, true);
        connect(srcOut, dstIn, linkDelay);

        // Define link weights
        double weight = 1/capacity;
        std::vector<double> weightVector{capacity, fee, linkQuality};

        // Add link to links buffer (we use a buffer because we can`t safely add links before all modules are built)
        cTopology::Link *link = new cTopology::Link(weight);
        auto linkTuple = std::make_tuple(link, srcOut, dstIn);
        linksBuffer.push_back(linkTuple);

        //Initialize payment channels and add nodes to adjacency matrix
        auto pc = std::make_tuple(capacity, fee, linkQuality, maxAcceptedHTLCs, HTLCMinimumMsat, channelReserveSatoshis, srcOut, dstIn);
        nameToPCs[srcName][dstName] = pc;
        adjMatrix[srcName].push_back(std::make_pair(dstName, weightVector));

    }

    // Build modules
    std::map<int, cModule*>::iterator it;
    for (it = nodeIdToMod.begin(); it != nodeIdToMod.end(); it++) {
        cModule *mod = it->second;
        mod->buildInside();
    }

    // Add links to global topology
    for(const auto& linkTuple: linksBuffer) {
        cTopology::Link *link = std::get<0>(linkTuple);
        cGate *srcGate = std::get<1>(linkTuple);
        cGate *dstGate = std::get<2>(linkTuple);
        globalTopology->addLink(link, srcGate, dstGate);
    }

    // Initialize modules
    bool more = true;
    for (int stage = 0; more; stage++) {
        more = false;
        for (it = nodeIdToMod.begin(); it != nodeIdToMod.end(); ++it) {
            cModule *mod = it->second;
            if (mod->callInitialize(stage))
                more = true;
        }
    }

}
