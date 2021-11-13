cplusplus{{
    #include "messages.h"
}};
packet BaseMessage {
    string destination;
    int messageType;
    //int hops[20];
    int hopCount;
}