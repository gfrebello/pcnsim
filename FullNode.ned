simple FullNode
{
    parameters:
        @display("i=block/routing");
        @signal[node*-to-node*:balance](type="double");
        @statisticTemplate[pcBalances](title="Balance of payment channels from $namePart1"; record=vector);

    gates:
        input in[];
        output out[];
}