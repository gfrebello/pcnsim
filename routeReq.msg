cplusplus{{
#include "messageTypes.h"
}};

packet RouteReq {
    int src;
    int dest;
    double transactionValue;
    double maxRoutingFee;
    int messageType;
}