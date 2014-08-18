
#ifndef _NETWORK_H
#define _NETWORK_H

#include "ae.h"
#include "client.h"

#define NOTUSED(V) ((void) V)
#define IOBUF_LEN         (1024*16) 


void acceptTcpHandler(aeEventLoop *el, int fd, void *privdata, int mask);
void acceptCommonHandler(int fd, int flags);
void readQueryFromClientHandle(aeEventLoop *el, int fd, void *privdata, int mask) ;
void addReply(struct client *c, char *str) ;
void sendReplyToClient(aeEventLoop *el, int fd, void *privdata, int mask);
void process_input(struct client *c);


#endif

