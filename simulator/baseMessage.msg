cplusplus{{
    #include <vector>
    #include "messages.h"

	inline std::ostream & operator << (std::ostream & os, const std::string & s) {
        std::operator<<(os, s);        
        return os;
	}
	typedef std::vector<std::string> stringVector;
}};

class stringVector {
    @existingClass;
}

packet BaseMessage {
    string destination;
    int messageType;
    int hopCount;
    stringVector hops;
    //bool upstreamDirection;
     string displayString = "b=0,0,rect,o=white,white,0	";
}