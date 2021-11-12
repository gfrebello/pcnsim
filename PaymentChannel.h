#include "globals.h"
#include <stdio.h>
#include <omnetpp.h>

using namespace omnetpp;

class PaymentChannel {

    public:
        double _capacity;
        double _fee;
        double _quality;
        cGate *_gate;

        // Constructors for polymorphism
        PaymentChannel() {};
        PaymentChannel(cGate *gate);
        PaymentChannel(double capacity, double fee, double quality, cGate *gate);

        // Getters and setters
        virtual double getCapacity() const { return this->_capacity; };
        virtual void setCapacity(double capacity) { this->_capacity = capacity; };
        virtual void increaseCapacity(double value) { this->_capacity = this->_capacity + value; };
        virtual void decreaseCapacity(double value) { this->_capacity = this->_capacity - value; };
        virtual double getFee() const { return this->_fee; };
        virtual void setFee(double fee) { this->_fee = fee; };
        virtual double getQuality() const { return this->_quality; };
        virtual void setQuality(double quality) { this->_quality = quality; };
        virtual cGate* getGate() const { return this->_gate; };
        virtual void setGate(cGate* gate) { this->_gate = gate; };

        // Auxiliary functions
        Json::Value toJson() const;

    private:
        void copy(const PaymentChannel& other);

};
