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

        // Some channel parameters defined in BOLT#2
        int _maxAcceptedHTLCs;
        double _HTLCMinimumMsat;
        int _numHTLCs;
        double _channelReserveSatoshis;

        bool _isWaitingForAck; // Auxiliary variable to check if we're waiting for an ACK in this channel

        std::map<std::string, HTLC *> _inFlights; //htlcId to value
        std::map<std::string, HTLC *> _pendingHTLCs; //htlcId to pending HTLC vector
        std::deque<HTLC *> _pendingHTLCsFIFO; //determines the order that the HTLCs were added to the pending list
        std::map<int, std::vector<HTLC *>> _HTLCsWaitingForAck; //ackId to HTLCs waiting for ack to arrive
        std::map<std::string, HTLC *> _committedHTLCs; //htlcId to committed HTLC vector
        std::deque<HTLC *> _committedHTLCsFIFO; //determines the order that the HTLCs were committed
        std::map<std::string, std::string> _previousHopUp; //htlcId to previous Hop in the upstream direction
        std::map<std::string, std::string> _previousHopDown; //htlcId to previous Hop in the downstream direction

        cGate *_localGate;
        cGate *_neighborGate;

        // Constructors for polymorphism
        PaymentChannel() {};
        PaymentChannel(double capacity, double balance, double quality, int maxAcceptedHTLCs, int numHTLCs, double HTLCMinimumMsat, double channelReserveSatoshis, cGate* localGate, cGate *neighborGate);

        // Generic getters and setters
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

         // In flight functions
         virtual HTLC* getInFlight (std::string htlcId) { return this->_inFlights[htlcId]; };
         virtual void setInFlight (std::string htlcId, HTLC *htlc) { this->_inFlights[htlcId] = htlc; };
         virtual bool isInFlight (HTLC *htlc);
         virtual void removeInFlight (std::string htlcId) { this->_inFlights.erase(htlcId); };

         // Pending HTLC functions
         virtual HTLC* getPendingHTLC (std::string htlcId) { return this->_pendingHTLCs[htlcId]; };
         virtual void setPendingHTLC (std::string htlcId, HTLC *htlc) { this->_pendingHTLCs[htlcId] = htlc; };
         virtual bool isPendingHTLC (HTLC *htlc);
         virtual std::map<std::string, HTLC*> getPendingHTLCs () { return this->_pendingHTLCs; };
         virtual void removePendingHTLC (std::string htlcId) { this->_committedHTLCs.erase(htlcId); };
         virtual std::deque<HTLC *> getPendingHTLCsFIFO () { return this->_pendingHTLCsFIFO; };
         virtual void setFirstPendingHTLCFIFO (HTLC *htlc) { this->_pendingHTLCsFIFO.push_front(htlc); };
         virtual void setLastPendingHTLCFIFO (HTLC *htlc) { this->_pendingHTLCsFIFO.push_back(htlc); };
         virtual HTLC * getFirstPendingHTLCFIFO () { return _pendingHTLCsFIFO.front(); };
         virtual HTLC * getLastPendingHTLCFIFO () { return _pendingHTLCsFIFO.back(); };
         virtual HTLC * getPendingHTLCByIndex (int index) { return _pendingHTLCsFIFO.at(index); };
         virtual void removeFirstPendingHTLCFIFO () { this->_pendingHTLCsFIFO.pop_front(); };
         virtual void removeLastPendingHTLCFIFO () { this->_pendingHTLCsFIFO.pop_back(); };
         virtual void removePendingHTLCFIFOByValue (HTLC *htlc);
         virtual size_t getPendingBatchSize () { return this->_pendingHTLCsFIFO.size(); };

         // Committed HTLC functions
         virtual HTLC* getCommittedHTLC (std::string htlcId) { return this->_committedHTLCs[htlcId]; };
         virtual void setCommittedHTLC (std::string htlcId, HTLC *htlc) { this->_committedHTLCs[htlcId] = htlc; };
         virtual bool isCommittedHTLC (HTLC *htlc);
         virtual std::map<std::string, HTLC*> getCommittedHTLCs () { return this->_committedHTLCs; };
         virtual void removeCommittedHTLC (std::string htlcId) { this->_pendingHTLCs.erase(htlcId); };
         virtual std::deque<HTLC *> getCommittedHTLCsFIFO () { return this->_committedHTLCsFIFO; };
         virtual void setFirstCommittedHTLCFIFO (HTLC * htlc) { this->_committedHTLCsFIFO.push_front(htlc); };
         virtual void setLastCommittedHTLCFIFO (HTLC * htlc) { this->_committedHTLCsFIFO.push_back(htlc); };
         virtual HTLC * getFirstCommittedHTLCFIFO () { return _committedHTLCsFIFO.front(); };
         virtual HTLC * getLastCommittedHTLCFIFO () { return _committedHTLCsFIFO.back(); };
         virtual HTLC * getCommittedHTLCByIndex (int index) { return _committedHTLCsFIFO.at(index); };
         virtual void removeFirstCommittedHTLCFIFO () { this->_committedHTLCsFIFO.pop_front(); };
         virtual void removeLastCommittedHTLCFIFO () { this->_committedHTLCsFIFO.pop_back(); };
         virtual void removeCommittedHTLCFIFOByValue (HTLC * htlc);
         virtual size_t getCommittedBatchSize () { return this->_committedHTLCsFIFO.size(); };

         // Previous hop functions
         virtual void setPreviousHopUp (std::string htlcId, std::string previousHop) { this->_previousHopUp[htlcId] = previousHop; };
         virtual std::string getPreviousHopUp (std::string htlcId) { return this->_previousHopUp[htlcId]; };
         virtual void removePreviousHopUp (std::string htlcId) { this->_previousHopUp.erase(htlcId); };
         virtual void setPreviousHopDown (std::string htlcId, std::string previousHop) { this->_previousHopDown[htlcId] = previousHop; };
         virtual std::string getPreviousHopDown (std::string htlcId) { return this->_previousHopDown[htlcId]; };
         virtual void removePreviousHopDown (std::string htlcId) { this->_previousHopDown.erase(htlcId); };

         // Gate functions
         virtual cGate* getLocalGate() const { return this->_localGate; };
         virtual void setLocalGate(cGate* gate) { this->_localGate = gate; };
         virtual cGate* getNeighborGate() const { return this->_neighborGate; };
         virtual void setNeighborGate(cGate* gate) { this->_neighborGate = gate; };

         // Ack functions
         virtual void setWaitingForAck (bool value) { this->_isWaitingForAck = value; };
         virtual bool isWaitingForAck() { return this->_isWaitingForAck; };
         virtual std::map<int, std::vector<HTLC *>> getAllHTLCsWaitingForAck () { return this->_HTLCsWaitingForAck; };
         virtual std::vector<HTLC *> getHTLCsWaitingForAck (int id) { return this->_HTLCsWaitingForAck[id]; };
         virtual void setHTLCsWaitingForAck (int id, std::vector<HTLC *> vector) { this->_HTLCsWaitingForAck[id] = vector;};
         virtual void removeHTLCsWaitingForAck (int id) { this->_HTLCsWaitingForAck.erase(id); };
         virtual void removeHTLCFromWaitingForAck (int id, HTLC *htlc) { this->_HTLCsWaitingForAck[id].erase(std::remove(_HTLCsWaitingForAck[id].begin(), _HTLCsWaitingForAck[id].end(), htlc), _HTLCsWaitingForAck[id].end()); };

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

bool PaymentChannel::isPendingHTLC (HTLC *htlc) {
    std::string htlcId = htlc->getPaymentHash() + ":" + std::to_string(htlc->getType());
    if(!_pendingHTLCs[htlcId]) {
        // Remove the null pointer we just added
        removePendingHTLC(htlcId);
        return false;
    } else {
        return true;
    }
}

bool PaymentChannel::isCommittedHTLC (HTLC *htlc) {
    std::string htlcId = htlc->getPaymentHash() + ":" + std::to_string(htlc->getType());
    if(!_committedHTLCs[htlcId]) {
        // Remove the null pointer we just added
        removeCommittedHTLC(htlcId);
        return false;
    } else {
        return true;
    }
}

bool PaymentChannel::isInFlight (HTLC *htlc) {
    std::string htlcId = htlc->getPaymentHash() + ":" + std::to_string(htlc->getType());
    if (!_inFlights[htlcId]) {
        // Remove the null pointer we just added
        removeInFlight(htlcId);
        return false;
    } else {
        return true;
    }
}

void PaymentChannel::removePendingHTLCFIFOByValue (HTLC *htlc) {
    for (auto it = _pendingHTLCsFIFO.begin(); it < _pendingHTLCsFIFO.end(); it++) {
        HTLC *pendingHTLC = *it;
        if (pendingHTLC->getHtlcId() == htlc->getHtlcId())
            _pendingHTLCsFIFO.erase(it);
    }
}

void PaymentChannel::removeCommittedHTLCFIFOByValue (HTLC *htlc) {
    for (auto it = _committedHTLCsFIFO.begin(); it < _committedHTLCsFIFO.end(); it++) {
        HTLC *committedHTLC = *it;
        if (committedHTLC->getHtlcId() == htlc->getHtlcId())
            _committedHTLCsFIFO.erase(it);
    }
}
