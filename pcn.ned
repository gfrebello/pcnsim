simple FullNode
{
    parameters:
		bool sendMsgOnInit = default(false);
		int initCapacity = default(10);
        @display("i=block/routing");
		@signal[node*-to-node*:balance](type="double");
		@statisticTemplate[pcBalances](title="Balance of payment channels from $namePart1"; record=vector);		         
    
	gates:
        input in[];
        output out[];
}


network PCN
{
    submodules:
        node[6]: FullNode;

    connections:
        node[0].out++ --> { delay = 100.0ms; } --> node[1].in++;
		node[0].out++ --> { delay = 100.0ms; } --> node[2].in++;
        node[0].in++ <-- { delay = 100.0ms; } <-- node[1].out++;
		node[0].in++ <-- { delay = 100.0ms; } <-- node[2].out++;

        node[1].out++ --> { delay = 100.0ms; } --> node[2].in++;
        node[1].in++ <-- { delay = 100.0ms; } <-- node[2].out++;

        node[1].out++ --> { delay = 100.0ms; } --> node[4].in++;
        node[1].in++ <-- { delay = 100.0ms; } <-- node[4].out++;

        node[3].out++ --> { delay = 100.0ms; } --> node[4].in++;
        node[3].in++ <-- { delay = 100.0ms; } <-- node[4].out++;

        node[4].out++ --> { delay = 100.0ms; } --> node[5].in++;
        node[4].in++ <-- { delay = 100.0ms; } <-- node[5].out++;
}
