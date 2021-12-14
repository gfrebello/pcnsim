simple FullNode
{
    parameters:
        //@display("i=block/routing");
        @display("i=device/pc_s");

		// Signals
        @signal[node*-to-node*:capacity](type="double");
        @signal[completedPayments](type="int");
        @signal[failedPayments](type="int");
        @signal[canceledPayments](type="int");
        @signal[paymentGoodputSent](type="double");
        @signal[paymentGoodputAll](type="double");
        
		// Statistics        
        @statisticTemplate[pcCapacities](title="Capacity of payment channels from $namePart1"; record=vector,stats);
		@statistic[completedPayments](record=vector,stats);
		@statistic[failedPayments](record=vector,stats);
		@statistic[canceledPayments](record=vector,stats);
		@statistic[paymentGoodputSent](record=vector,stats);
		@statistic[paymentGoodputAll](record=vector,stats);
		
    gates:
        input in[];
        output out[];
}