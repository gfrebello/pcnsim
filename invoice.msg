cplusplus {{
    #include "messages.h"
}};

packet Invoice {
    string source;
    double value;
    string paymentHash;
}