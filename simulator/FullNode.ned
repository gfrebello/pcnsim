simple FullNode
{
    parameters:
        //@display("i=block/routing");
        @display("i=device/pc_s");
        @signal[node*-to-node*:capacity](type="double");
        @statisticTemplate[pcCapacities](title="Capacity of payment channels from $namePart1"; record=vector);

    gates:
        input in[];
        output out[];
}