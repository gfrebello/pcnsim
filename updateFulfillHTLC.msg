cplusplus {{
    #include <string>
    #include "messages.h"
}};


packet update_fulfill_htlc
{
    string paymentHash;
    string preImage;
}