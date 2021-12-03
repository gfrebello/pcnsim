cplusplus {{
    #include <string>
    #include "messages.h"
}};

packet HTLCRefused {
    string paymentHash;
    string errorReason;
    double value;
}