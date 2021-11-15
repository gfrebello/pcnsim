//
// Generated file, do not edit! Created by nedtool 5.7 from updateAddHTLC.msg.
//

#ifndef __UPDATEADDHTLC_M_H
#define __UPDATEADDHTLC_M_H

#if defined(__clang__)
#  pragma clang diagnostic ignored "-Wreserved-id-macro"
#endif
#include <omnetpp.h>

// nedtool version check
#define MSGC_VERSION 0x0507
#if (MSGC_VERSION!=OMNETPP_VERSION)
#    error Version mismatch! Probably this file was generated by an earlier version of nedtool: 'make clean' should help.
#endif



// cplusplus {{
    #include <string>
    #include "messages.h"
// }}

/**
 * Class generated from <tt>updateAddHTLC.msg:6</tt> by nedtool.
 * <pre>
 * packet UpdateAddHTLC
 * {
 *     string source;
 *     string paymentHash;
 *     //simtime_t timeout; 
 *     double value;
 * }
 * </pre>
 */
class UpdateAddHTLC : public ::omnetpp::cPacket
{
  protected:
    ::omnetpp::opp_string source;
    ::omnetpp::opp_string paymentHash;
    double value;

  private:
    void copy(const UpdateAddHTLC& other);

  protected:
    // protected and unimplemented operator==(), to prevent accidental usage
    bool operator==(const UpdateAddHTLC&);

  public:
    UpdateAddHTLC(const char *name=nullptr, short kind=0);
    UpdateAddHTLC(const UpdateAddHTLC& other);
    virtual ~UpdateAddHTLC();
    UpdateAddHTLC& operator=(const UpdateAddHTLC& other);
    virtual UpdateAddHTLC *dup() const override {return new UpdateAddHTLC(*this);}
    virtual void parsimPack(omnetpp::cCommBuffer *b) const override;
    virtual void parsimUnpack(omnetpp::cCommBuffer *b) override;

    // field getter/setter methods
    virtual const char * getSource() const;
    virtual void setSource(const char * source);
    virtual const char * getPaymentHash() const;
    virtual void setPaymentHash(const char * paymentHash);
    virtual double getValue() const;
    virtual void setValue(double value);
};

inline void doParsimPacking(omnetpp::cCommBuffer *b, const UpdateAddHTLC& obj) {obj.parsimPack(b);}
inline void doParsimUnpacking(omnetpp::cCommBuffer *b, UpdateAddHTLC& obj) {obj.parsimUnpack(b);}


#endif // ifndef __UPDATEADDHTLC_M_H

