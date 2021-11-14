cplusplus{{
#include "globals.h"
}};

packet Payment {
    string source;
    string destination;
    int hopCount = 0;
    double value;
}