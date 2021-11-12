#include "PaymentChannel.h"

PaymentChannel::PaymentChannel(double capacity, double balance, double quality, cGate *gate) {
    this->_capacity = capacity;
    this->_fee = balance;
    this->_quality = quality;
    this->_gate = gate;
}

PaymentChannel::PaymentChannel(cGate *gate) {
    this->_capacity = 10;
    this->_fee = 0;
    this->_quality = 1;
    this->_gate = gate;
}


void PaymentChannel::copy(const PaymentChannel& other) {
    this->_capacity = other._capacity;
    this->_fee = other._fee;
    this->_quality = other._quality;
    this->_gate = other._gate;
}


Json::Value PaymentChannel::toJson() const {
    Json::Value json(Json::objectValue);
    json["capacity"] = std::to_string(this->_capacity);
    json["balance"] = std::to_string(this->_fee);
    json["quality"] = std::to_string(this->_quality);
    return json;
}