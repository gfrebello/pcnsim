// Helper module that creates the network from prefedined topology and workload files

simple NetBuilder {
    parameters:
		@display("i=block/cogwheel_s");
		string topologyFile = default("../topologies/topology");
       	//string topologyFile = default("../topologies/scale-free.txt");
        //string topologyFile = default("topology.txt");
        string workloadFile = default("../workloads/random-workload.txt");
        //string workloadFile = default("workload.txt");
};
