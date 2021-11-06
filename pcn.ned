import FullNode;
import NetBuilder;

network PCN {
    submodules:
        netBuilder: NetBuilder;
}

//network PCN
//{
//    @display("bgb=335,233");
//    submodules:
//        node[6]: FullNode {
//            @display("p=31,42");
//        }
//    connections:
//        node[0].out++ --> {  delay = 100.0ms; } --> node[1].in++;
//        node[0].out++ --> {  delay = 100.0ms; } --> node[2].in++;
//        node[0].in++ <-- {  delay = 100.0ms; } <-- node[1].out++;
//        node[0].in++ <-- {  delay = 100.0ms; } <-- node[2].out++;
//
//        node[1].out++ --> {  delay = 100.0ms; } --> node[2].in++;
//        node[1].in++ <-- {  delay = 100.0ms; } <-- node[2].out++;
//
//        node[1].out++ --> {  delay = 100.0ms; } --> node[4].in++;
//        node[1].in++ <-- {  delay = 100.0ms; } <-- node[4].out++;
//
//        node[3].out++ --> {  delay = 100.0ms; } --> node[4].in++;
//        node[3].in++ <-- {  delay = 100.0ms; } <-- node[4].out++;
//
//        node[4].out++ --> {  delay = 100.0ms; } --> node[5].in++;
//        node[4].in++ <-- {  delay = 100.0ms; } <-- node[5].out++;
//}
