#include "globals.h"
#include <stdio.h>
#include <omnetpp.h>

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
        std::map<std::string, double> _inFlights;

        cGate *_gate;

        // Constructors for polymorphism
        PaymentChannel() {};
        PaymentChannel(cGate *gate);
        //PaymentChannel(double capacity, double fee, double quality, cGate *gate);
        PaymentChannel(double capacity, double balance, double quality, int maxAcceptedHTLCs, int numHTLCs, double HTLCMinimumMsat, double channelReserveSatoshis, cGate *gate);


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
         virtual void setInFlight (std::string paymentHash, double amount) { this->_inFlights[paymentHash] = amount; };
         virtual double getInFlight (std::string paymentHash) { return this->_inFlights[paymentHash]; };
         virtual void removeInFlight (std::string paymentHash) { this->_inFlights.erase(paymentHash); };
         virtual cGate* getGate() const { return this->_gate; };
         virtual void setGate(cGate* gate) { this->_gate = gate; };

        // Auxiliary functions
        Json::Value toJson() const;

    private:
        void copy(const PaymentChannel& other);

};




