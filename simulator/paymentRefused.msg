cplusplus {{
    #include <string>
    #include "messages.h"
}};

packet PaymentRefused {
    string paymentHash;
    string errorReason;
    double value;
}