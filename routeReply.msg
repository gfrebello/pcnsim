#include "messageTypes.h"
#include <vector>

packet RouteReply {
    int origin;
    double routingFee;
	std::vector<int> path;    
    int messageType;
    // TODO: maybe add some other infos
}