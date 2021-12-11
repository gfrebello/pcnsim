cplusplus {{
    #include <string>
    #include "messages.h"
}};

packet UpdateFulfillHTLC {
    string htlcId;
    string paymentHash;
    string preImage;
    double value;
}