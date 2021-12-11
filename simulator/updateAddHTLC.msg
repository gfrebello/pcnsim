cplusplus {{
    #include <string>
    #include "messages.h"
}};

packet UpdateAddHTLC {
    string source;
    string htlcId;
    string paymentHash;
    simtime_t timeout; 
    double value;
}