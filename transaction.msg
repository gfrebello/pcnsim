cplusplus{{
#include "globals.h"
}};

packet Transaction {
    string source;
    string destination;
    int hopCount = 0;
    double value;
}