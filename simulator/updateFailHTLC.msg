cplusplus {{
    #include <string>
    #include "messages.h"
}};

packet UpdateFailHTLC {
    string paymentHash;
    string errorReason;
    double value;
}