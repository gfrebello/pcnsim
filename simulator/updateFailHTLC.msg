cplusplus {{
    #include <string>
    #include "messages.h"
}};

packet UpdateFailHTLC {
    string paymentHash;
<<<<<<< Updated upstream
=======
    string errorReason;
    double value;
>>>>>>> Stashed changes
}