cplusplus {{
    #include <string>
    #include "messages.h"
}};

packet UpdateAddHTLC {
    string source;
    string paymentHash;
    //simtime_t timeout; 
    double value;
}