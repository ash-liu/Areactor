
#ifndef _NETWORK_H
#define _NETWORK_H

#include "ae.h"
#include "client.h"

#define NOTUSED(V) ((void) V)

void acceptTcpHandler(aeEventLoop *el, int fd, void *privdata, int mask);
void acceptCommonHandler(int fd, int flags);
void readQueryFromClient(aeEventLoop *el, int fd, void *privdata, int mask) ;
void addReply(struct client *c, char *obj) ;
void sendReplyToClient(aeEventLoop *el, int fd, void *privdata, int mask);



#endif

