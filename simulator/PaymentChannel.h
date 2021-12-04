#include <stdio.h>
#include <omnetpp.h>
#include <vector>
#include <queue>
#include <algorithm>
#include <jsoncpp/json/value.h>
#include "HTLC.h"

using namespace omnetpp;

class PaymentChannel {

    public:
        double _capacity;
        double _fee;
        double _quality;

        //some channel parameters defined in BOLT#2
        int _maxAcceptedHTLCs;
        double _HTLCMinimumMsat;
        int _numHTLCs;
        double _channelReserveSatoshis;

        std::map<std::string, HTLC *> _inFlights; //paymentHash to value
        std::map<std::string, HTLC *> _pendingHTLCs; //paymentHash to pending HTLC
        std::deque<std::string> _pendingHTLCsFIFO; //determines the order that the HTLCs were added to the pending list
        std::map<int, std::vector<HTLC *>> _HTLCsWaitingForAck; //ackId to HTLCs waiting for ack to arrive
        std::map<std::string, std::string> _previousHop; //paymentHash to previous Hop

        cGate *_localGate;
        cGate *_neighborGate;

        // Constructors for polymorphism
        PaymentChannel() {};
        PaymentChannel(double capacity, double balance, double quality, int maxAcceptedHTLCs, int numHTLCs, double HTLCMinimumMsat, double channelReserveSatoshis, cGate* localGate, cGate *neighborGate);

        // Getters and setters
         virtual double getCapacity() const { return this->_capacity; };
         virtual void setCapacity(double capacity) { this->_capacity = capacity; };
         virtual void increaseCapacity(double value) { this->_capacity = this->_capacity + value; };
         virtual void decreaseCapacity(double value) { this->_capacity = this->_capacity - value; };
         virtual double getFee() const { return this->_fee; };
         virtual void setFee(double fee) { this->_fee = fee; };
         virtual double getQuality() const { return this->_quality; };
         virtual void setQuality(double quality) { this->_quality = quality; };
         virtual int getMaxAcceptedHTLCs() const { return this->_maxAcceptedHTLCs; };
         virtual void setMaxAcceptedHTLCs(int maxAcceptedHTLCs) { this->_maxAcceptedHTLCs = maxAcceptedHTLCs; };
         virtual double getHTLCMinimumMSAT() const { return this->_HTLCMinimumMsat; };
         virtual void setHTLCMinimumMSAT(double HTLCMinimumMsat) { this->_HTLCMinimumMsat = HTLCMinimumMsat; };
         virtual int getnumHTLCs() const { return this->_numHTLCs; };
         virtual void setnumHTLCs (int numHTLCs) { this->_numHTLCs = numHTLCs; };
         virtual void increasenumHTLCs () { this->_numHTLCs = _numHTLCs + 1; };
         virtual void decreasenumHTLCs () { this->_numHTLCs = _numHTLCs - 1; };
         virtual double getChannelReserveSatoshis () const { return this->_channelReserveSatoshis; };
         virtual void setChannelReserveSatothis (double channelReserveSatoshis) { this->_channelReserveSatoshis = channelReserveSatoshis; };
         virtual HTLC* getInFlight (std::string paymentHash) { return this->_inFlights[paymentHash]; };
         virtual void setInFlight (std::string paymentHash, HTLC *htlc) { this->_inFlights[paymentHash] = htlc; };
         virtual bool isInFlight (std::string paymentHash) { return (this->_inFlights[paymentHash] != NULL ? true : false); };
         virtual void removeInFlight (std::string paymentHash) { this->_inFlights.erase(paymentHash); };
         virtual HTLC* getPendingHTLC (std::string paymentHash) { return this->_pendingHTLCs[paymentHash]; };
         virtual void setPendingHTLC (std::string paymentHash, HTLC *htlc) { this->_pendingHTLCs[paymentHash] = htlc; };
         virtual bool isPendingHTLC (std::string paymentHash) { return (this->_pendingHTLCs[paymentHash] != NULL ? true : false); };
         virtual std::map<std::string, HTLC*> getPendingHTLCs () { return this->_pendingHTLCs; };
         virtual void removePendingHTLC (std::string paymentHash) { this->_pendingHTLCs.erase(paymentHash); };
         virtual void setFirstPendingHTLCFIFO (std::string paymentHash) { this->_pendingHTLCsFIFO.push_front(paymentHash); };
         virtual void setLastPendingHTLCFIFO (std::string paymentHash) { this->_pendingHTLCsFIFO.push_back(paymentHash); };
         virtual std::string getFirstPendingHTLCFIFO () { return _pendingHTLCsFIFO.front(); };
         virtual std::string getLastPendingHTLCFIFO () { return _pendingHTLCsFIFO.back(); };
         virtual void removeFirstPendingHTLCFIFO () { this->_pendingHTLCsFIFO.pop_front(); };
         virtual void removeLastPendingHTLCFIFO () { this->_pendingHTLCsFIFO.pop_back(); };
         virtual void removePendingHTLCFIFOByValue (std::string paymentHash) { this->_pendingHTLCsFIFO.erase(std::remove(_pendingHTLCsFIFO.begin(), _pendingHTLCsFIFO.end(), paymentHash), _pendingHTLCsFIFO.end()); };
         virtual size_t getPendingBatchSize () { return this->_pendingHTLCsFIFO.size(); };
         virtual std::vector<HTLC *> getHTLCsWaitingForAck (int id) { return this->_HTLCsWaitingForAck[id]; };
         virtual void setHTLCsWaitingForAck (int id, std::vector<HTLC *> vector) { this->_HTLCsWaitingForAck[id] = vector;};
         virtual void removeHTLCsWaitingForAck (int id) { this->_HTLCsWaitingForAck.erase(id); };
         virtual void setPreviousHop (std::string paymentHash, std::string previousHop) { this->_previousHop[paymentHash] = previousHop; };
         virtual std::string getPreviousHop (std::string paymentHash) { return this->_previousHop[paymentHash]; };
         virtual void removePreviousHop (std::string paymentHash) { this->_previousHop.erase(paymentHash); };
         virtual cGate* getLocalGate() const { return this->_localGate; };
         virtual void setLocalGate(cGate* gate) { this->_localGate = gate; };
         virtual cGate* getNeighborGate() const { return this->_neighborGate; };
         virtual void setNeighborGate(cGate* gate) { this->_neighborGate = gate; };

        // Auxiliary functions
        //Json::Value toJson() const;

    private:
        void copy(const PaymentChannel& other);

};

PaymentChannel::PaymentChannel(double capacity, double fee, double quality, int maxAcceptedHTLCs, int numHTLCs, double HTLCMinimumMsat, double channelReserveSatoshis, cGate *localGate, cGate *neighborGate) {
    this->_capacity = capacity;
    this->_fee = fee;
    this->_quality = quality;
    this->_maxAcceptedHTLCs = maxAcceptedHTLCs;
    this->_HTLCMinimumMsat = HTLCMinimumMsat;
    this->_numHTLCs = numHTLCs;
    this->_channelReserveSatoshis = channelReserveSatoshis;
    this->_localGate = localGate;
    this->_neighborGate = neighborGate;
}


void PaymentChannel::copy(const PaymentChannel& other) {
    this->_capacity = other._capacity;
    this->_fee = other._fee;
    this->_quality = other._quality;
    this->_maxAcceptedHTLCs = other._maxAcceptedHTLCs;
    this->_HTLCMinimumMsat = other._HTLCMinimumMsat;
    this->_numHTLCs = other._numHTLCs;
    this->_channelReserveSatoshis = other._channelReserveSatoshis;
    this->_localGate = other._localGate;
    this->_neighborGate = other._neighborGate;
}

//
//Json::Value PaymentChannel::toJson() const {
//    Json::Value json(Json::objectValue);
//    json["capacity"] = std::to_string(this->_capacity);
//    json["fee"] = std::to_string(this->_fee);
//    json["quality"] = std::to_string(this->_quality);
//    json["maxAcceptedHLTCs"] = std::to_string(this->_maxAcceptedHTLCs);
//    json["HTLCMinimumMsat"] = std::to_string(this->_HTLCMinimumMsat);
//    return json;
//}


