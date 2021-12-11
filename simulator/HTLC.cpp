#include "HTLC.h"
HTLC::HTLC(UpdateAddHTLC *htlc) {
    _htlcId = htlc->getHtlcId();
    _type = UPDATE_ADD_HTLC;
    _source = htlc->getSource();
    _paymentHash = htlc->getPaymentHash();
    _value = htlc->getValue();
    _timeout = htlc->getTimeout();
}

HTLC::HTLC(UpdateFulfillHTLC *htlc) {
    _htlcId = htlc->getHtlcId();
    _type = UPDATE_FULFILL_HTLC;
    _paymentHash = htlc->getPaymentHash();
    _preImage = htlc->getPreImage();
    _value = htlc->getValue();
}

HTLC::HTLC(UpdateFailHTLC *htlc) {
    _htlcId = htlc->getHtlcId();
    _type = UPDATE_FAIL_HTLC;
    _paymentHash = htlc->getPaymentHash();
    _errorReason = htlc->getErrorReason();
    _value = htlc->getValue();
}
