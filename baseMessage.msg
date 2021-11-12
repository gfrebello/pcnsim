cplusplus{{
    #include "messages.h"
}};
packet BaseMessage {
    int destination;
    int messageType;
    //int hops[20];
    int hopCount;
}