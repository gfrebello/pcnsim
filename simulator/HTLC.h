#pragma once

#include "updateAddHTLC_m.h"
#include "updateFulfillHTLC_m.h"
#include "updateFailHTLC_m.h"
#include <omnetpp.h>

using namespace omnetpp;

class HTLC {

public:
    int _type = 0;
    std::string _htlcId = "";
    std::string _source = "";
    std::string _paymentHash = "";
    std::string _preImage = "";
    std::string _errorReason = "";
    simtime_t _timeout = 0;
    double _value = 0;

    virtual std::string getHtlcId() { return _htlcId; };
    virtual void setHtlcId(std::string htlcId) { _htlcId = htlcId; };
    virtual int getType() { return _type; };
    virtual void setType(int type) { _type = type; };
    virtual std::string getSource() { return _source; };
    virtual void setSource(std::string source) { _source = source; };
    virtual std::string getPaymentHash() { return _paymentHash; };
    virtual void setPaymentHash(std::string paymentHash) { _paymentHash = paymentHash; };
    virtual std::string getPreImage() { return _preImage; };
    virtual void setPreImage(std::string preImage) { _preImage = preImage; };
    virtual std::string getErrorReason() { return _errorReason; };
    virtual void setErrorReason(std::string errorReason) { _errorReason = errorReason; };
    virtual double getValue() { return _value; };
    virtual void setValue(double value) { _value = value; };

    HTLC () {};
    HTLC (UpdateAddHTLC *htlc);
    HTLC (UpdateFulfillHTLC *htlc);
    HTLC (UpdateFailHTLC *htlc);

};


