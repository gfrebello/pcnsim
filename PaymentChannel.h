#include "globals.h"

class PaymentChannel {

    public:
        double _capacity;
        double _balance;
        double _quality;
        cGate *_gate;

        // Constructors for polymorphism
        PaymentChannel() {};
        PaymentChannel(cGate *gate);
        PaymentChannel(double capacity, double balance, double quality, cGate *gate);

        // Getters and setters
        virtual double getCapacity() const { return this->_capacity; };
        virtual void setCapacity(double capacity) { this->_capacity = capacity; };
        virtual void increaseCapacity(double value) { this->_capacity = this->_capacity + value; };
        virtual void decreaseCapacity(double value) { this->_capacity = this->_capacity - value; };
        virtual double getBalance() const { return this->_balance; };
        virtual void setBalance(double balance) { this->_balance = balance; };
        virtual double getQuality() const { return this->_quality; };
        virtual void setQuality(double quality) { this->_quality = quality; };
        virtual cGate* getGate() const { return this->_gate; };
        virtual void setGate(cGate* gate) { this->_gate = gate; };

        // Auxiliary functions
        Json::Value toJson() const;

    private:
        void copy(const PaymentChannel& other);

};

PaymentChannel::PaymentChannel(double capacity, double balance, double quality, cGate *gate) {
    this->_capacity = capacity;
    this->_balance = balance;
    this->_quality = quality;
    this->_gate = gate;
}

PaymentChannel::PaymentChannel(cGate *gate) {
    this->_capacity = 10;
    this->_balance = 0;
    this->_quality = 1;
    this->_gate = gate;
}


void PaymentChannel::copy(const PaymentChannel& other) {
    this->_capacity = other._capacity;
    this->_balance = other._balance;
    this->_quality = other._quality;
    this->_gate = other._gate;
}


Json::Value PaymentChannel::toJson() const {
    Json::Value json(Json::objectValue);
    json["capacity"] = std::to_string(this->_capacity);
    json["balance"] = std::to_string(this->_balance);
    json["quality"] = std::to_string(this->_quality);
    return json;
}
