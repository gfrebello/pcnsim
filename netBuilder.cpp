#include "globals.h"

cTopology *globalTopology = new cTopology("globalTopology");

class NetBuilder : public cSimpleModule {
    public:
        virtual void initialize() override;
        virtual void handleMessage(cMessage *msg) override;
        void buildNetwork(cModule *parent);
        void connect(cGate *src, cGate *dst, double delay);
        bool doesNodeExist(std::map<long, cModule*> nodeList, long nodeId);
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

void NetBuilder::connect(cGate *src, cGate *dst, double delay) {

    cDelayChannel *channel = cDelayChannel::create("channel");
    channel->setDelay(delay);
    src->connectTo(dst, channel);
}

bool NetBuilder::doesNodeExist(std::map<long, cModule*> nodeList, long nodeId) {
    std::map<long, cModule*>::iterator it = nodeList.find(nodeId);
    if (it != nodeList.end())
        return true;
    else
        return false;
}

void NetBuilder::buildNetwork(cModule *parent) {

    std::map<long, cModule *> nodeIdToMod;
    std::string line;
    std::string modClassName = "FullNode";
    std::ifstream topologyFile(par("topologyFile").stringValue(), std::ifstream::in);

    cModule *srcMod;
    cModule *dstMod;
    cTopology::Node *srcNode;
    cTopology::Node *dstNode;

    EV << "Building network from file: " << par("topologyFile").stringValue() << "\n";

    while (getline(topologyFile, line, '\n')) {

        // Skip headers and empty lines
        if (line.empty() || line[0] == '#')
            continue;

        // Check whether all fields are present
        std::vector<std::string> tokens = cStringTokenizer(line.c_str()).asVector();
        if (tokens.size() != 5)
            throw cRuntimeError("wrong line in topology file: 5 items required, line: \"%s\"", line.c_str());

        // Get fields from tokens
        long srcId = atol(tokens[0].c_str());
        long dstId = atol(tokens[1].c_str());
        double srcCapacity = atof(tokens[2].c_str());
        double dstCapacity = atof(tokens[3].c_str());
        double delay = atof(tokens[4].c_str());

        // Print found edges
        EV << "EDGE FOUND: (" << srcId << ", " << dstId << "); Delay = " << delay << "ms. Processing...\n";

        // Define module names
        std::string srcNodeName = "node" + std::to_string(srcId);
        std::string dstNodeName = "node" + std::to_string(dstId);

        // Check if the source module exists and if not, create a new source module
        if(!doesNodeExist(nodeIdToMod,srcId)) {
            cModuleType *srcModType = cModuleType::find(modClassName.c_str());
            if (!srcModType)
                throw cRuntimeError("Source module class `%s' not found", modClassName.c_str());
            srcMod = srcModType->create(srcNodeName.c_str(), parent);
            nodeIdToMod[srcId] = srcMod;
            srcMod->finalizeParameters();
            srcNode = new cTopology::Node(srcId);
            globalTopology->addNode(srcNode);
        } else {
            srcMod = nodeIdToMod.find(srcId)->second;
            srcNode = globalTopology->getNode(srcId);
        }

        // Check if the destination module exists and if not, create a new destination module
        if(!doesNodeExist(nodeIdToMod,dstId)) {
            cModuleType *dstModType = cModuleType::find(modClassName.c_str());
            if (!dstModType)
                throw cRuntimeError("Destination module class `%s' not found", modClassName.c_str());
            dstMod = dstModType->create(dstNodeName.c_str(), parent);
            nodeIdToMod[dstId] = dstMod;
            dstMod->finalizeParameters();
            dstNode = new cTopology::Node(dstId);
            globalTopology->addNode(dstNode);
        } else {
            dstMod = nodeIdToMod.find(dstId)->second;
            dstNode = globalTopology->getNode(dstId);
        }

        // Connect modules
        cGate *srcIn, *srcOut, *dstIn, *dstOut;
        srcIn = srcMod->getOrCreateFirstUnconnectedGate("in", 0, false, true);
        srcOut= srcMod->getOrCreateFirstUnconnectedGate("out", 0, false, true);
        dstIn = dstMod->getOrCreateFirstUnconnectedGate("in", 0, false, true);
        dstOut = dstMod->getOrCreateFirstUnconnectedGate("out", 0, false, true);
        connect(srcOut, dstIn, delay);
        connect(dstOut, srcIn, delay);

        // Add bidirectional link to the global topology
        cTopology::Link *srcToDstLink = new cTopology::Link(srcCapacity);
        cTopology::Link *dstToSrcLink = new cTopology::Link(dstCapacity);
        globalTopology->addLink(srcToDstLink, srcNode, dstNode);
        globalTopology->addLink(dstToSrcLink, dstNode, srcNode);
    }

    // Initialize all modules
    std::map<long, cModule*>::iterator it;
    for (it = nodeIdToMod.begin(); it != nodeIdToMod.end(); it++) {
        cModule *mod = it->second;
        mod->buildInside();
    }

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
