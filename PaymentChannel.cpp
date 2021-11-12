#include "PaymentChannel.h"

PaymentChannel::PaymentChannel(double capacity, double fee, double quality, int maxAcceptedHTLCs, double HTLCMinimumMsat, int HTLCNumber, double channelReserveSatoshis, cGate *gate) {
    this->_capacity = capacity;
    this->_fee = fee;
    this->_quality = quality;
    this->_maxAcceptedHTLCs = maxAcceptedHTLCs;
    this->_HTLCMinimumMsat = HTLCMinimumMsat;
    this->_HTLCNumber = HTLCNumber;
    this->_channelReserveSatoshis = channelReserveSatoshis;
    this->_gate = gate;
}



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
    this->_maxAcceptedHTLCs = 5;
    this->_HTLCMinimumMsat = 0.1;
    this->_HTLCNumber = 0;
    this->_channelReserveSatoshis = 0.1;
    this->_gate = gate;
}


void PaymentChannel::copy(const PaymentChannel& other) {
    this->_capacity = other._capacity;
    this->_fee = other._fee;
    this->_quality = other._quality;
    this->_maxAcceptedHTLCs = other._maxAcceptedHTLCs;
    this->_HTLCMinimumMsat = other._HTLCMinimumMsat;
    this->_HTLCNumber = other._HTLCNumber;
    this->_channelReserveSatoshis = other._channelReserveSatoshis;
    this->_gate = other._gate;
}



Json::Value PaymentChannel::toJson() const {
    Json::Value json(Json::objectValue);
    json["capacity"] = std::to_string(this->_capacity);
    json["fee"] = std::to_string(this->_fee);
    json["quality"] = std::to_string(this->_quality);
    json["maxAcceptedHLTCs"] = std::to_string(this->_maxAcceptedHTLCs);
    json["HTLCMinimumMsat"] = std::to_string(this->_HTLCMinimumMsat);
    return json;
}
