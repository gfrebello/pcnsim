cplusplus {{
    #include "messages.h"
}};

packet Invoice {
    string source;
    string destination;
    double value;
    string paymentHash;
}