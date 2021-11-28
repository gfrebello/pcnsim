#ifndef MESSAGES_H
#define MESSAGES_H

//message types follow BOLT#2
#define UPDATE_ADD_HTLC     128
#define UPDATE_FULFILL_HTLC 130
#define UPDATE_FAIL_HTLC    131
#define COMMITMENT_SIGNED   132
#define REVOKE_AND_ACK      133

//Invoice type not defined in BOLTs
#define INVOICE             100


//Other types of messages
#define TRANSACTION_INIT 0
#define TRANSACTION_ACK 1
#define ROUTE_REQ 2
#define ROUTE_REPLY 3
#define ROUTE_ESTABLISH 4


#endif
