cplusplus {{
    #include <string>
    #include "messages.h"
}};

packet update_add_htlc
{
    int source;
    string paymentHash;
    //simtime_t timeout; 
    double amount;
}