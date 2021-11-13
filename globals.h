#include <stdio.h>
#include <string.h>
#include <omnetpp.h>
#include <iostream>
#include <fstream>
#include <random>
#include <string>
#include <unordered_map>
#include <map>
#include <chrono>
#include <jsoncpp/json/value.h>

using namespace omnetpp;

#define PREIMAGE_SIZE 32

extern cTopology *globalTopology;
extern std::map<std::string, std::vector<std::tuple<std::string, double, simtime_t>>> pendingTransactions;
extern std::map< std::string, std::map<std::string, std::tuple <double, double, double, int, double, double, cGate*> > > globalPaymentChannels;
