#include "globals.h"

cTopology *globalTopology = new cTopology("globalTopology");
std::map< std::string, std::vector< std::tuple<std::string, double, simtime_t> > > pendingPayments;
std::map< std::string, std::map<std::string, std::tuple<double, double, double, int, double, double, cGate*> > > nameToPCs;

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
        if (tokens.size() != 15)
            throw cRuntimeError("wrong line in topology file: 15 items required, line: \"%s\"", line.c_str());

        // Get fields from tokens
        int srcId = atoi(tokens[0].c_str());
        int dstId = atoi(tokens[1].c_str());
        double srcCapacity = atof(tokens[2].c_str());
        double dstCapacity = atof(tokens[3].c_str());
        double srcFee = atof(tokens[4].c_str());
        double dstFee = atof(tokens[5].c_str());
        double linkQualitySrcToDst = atof(tokens[6].c_str());
        double linkQualityDstToSrc = atof(tokens[7].c_str());
        int srcMaxAcceptedHTLCs = atoi(tokens[8].c_str());
        int dstMaxAcceptedHTLCs = atoi(tokens[9].c_str());
        double srcHTLCMinimumMsat = atof(tokens[10].c_str());
        double dstHTLCMinimumMsat = atof(tokens[11].c_str());
        double srcChannelReserveSatoshis = atof(tokens[12].c_str());
        double dstChannelReserveSatoshis = atof(tokens[13].c_str());
        double linkDelay = atof(tokens[14].c_str());

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
        cGate *srcIn, *srcOut, *dstIn, *dstOut;
        srcIn = srcMod->getOrCreateFirstUnconnectedGate("in", 0, false, true);
        srcOut = srcMod->getOrCreateFirstUnconnectedGate("out", 0, false, true);
        dstIn = dstMod->getOrCreateFirstUnconnectedGate("in", 0, false, true);
        dstOut = dstMod->getOrCreateFirstUnconnectedGate("out", 0, false, true);
        connect(srcOut, dstIn, linkDelay);
        connect(dstOut, srcIn, linkDelay);

        // Define link weights
        double srcToDstWeight = srcCapacity;
        double dstToSrcWeight = dstCapacity;

        // Add bidirectional link to links buffer (we use a buffer because
        // we can`t safely add links before all modules are built)
        cTopology::Link *srcToDstLink = new cTopology::Link(srcToDstWeight);
        cTopology::Link *dstToSrcLink = new cTopology::Link(dstToSrcWeight);
        auto linkTupleSrcToDst = std::make_tuple(srcToDstLink, srcOut, dstIn);
        auto linkTupleDstToSrc = std::make_tuple(dstToSrcLink, dstOut, srcIn);
        linksBuffer.push_back(linkTupleSrcToDst);
        linksBuffer.push_back(linkTupleDstToSrc);

        //Initialize payment channels
        auto pcSrcToDst = std::make_tuple(srcCapacity, srcFee, linkQualitySrcToDst, srcMaxAcceptedHTLCs, srcHTLCMinimumMsat, srcChannelReserveSatoshis, dstIn);
        auto pcDstToSrc = std::make_tuple(dstCapacity, dstFee, linkQualityDstToSrc, dstMaxAcceptedHTLCs, dstHTLCMinimumMsat, dstChannelReserveSatoshis, srcIn);
        nameToPCs[srcName][dstName] = pcSrcToDst;
        nameToPCs[dstName][srcName] = pcDstToSrc;
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
