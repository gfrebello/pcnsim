cplusplus {{
    #include <string>
    #include "messages.h"
}};

packet UpdateFailHTLC {
    string htlcId;
    string paymentHash;
    string errorReason;
    double value;
}