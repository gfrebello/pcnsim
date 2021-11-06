// Helper module that creates the network from prefedined topology and workload files

simple NetBuilder {
    parameters:
		@display("i=block/cogwheel_s");
        string topologyFile = default("topology.txt");
        string workloadFile = default("workload.txt");
};
