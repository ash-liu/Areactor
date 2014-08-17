
#ifndef _CLIENT_H
#define _CLIENT_H

#include "client.h"

#define REPLY_CHUNK_BYTES	(16*1024)

struct client{
	int fd;		// socket fd

	int argc;	//
	char **argv;

	
	char buf[REPLY_CHUNK_BYTES];
};



struct client *create_client(int fd);
void freeClient(struct client *c);

#endif

