cplusplus {{
    #include <string>
    #include "messages.h"
}};

packet UpdateFulfillHTLC {
    string paymentHash;
    string preImage;
    double value;
}