cplusplus {{
    #include <string>
    #include "messages.h"
}};

packet UpdateFulfillHTLC {
    string paymentHash;
    string preImage;
    string displayString = "b=15,15,rect,white,kind,5";
}