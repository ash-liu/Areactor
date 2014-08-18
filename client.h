
#ifndef _CLIENT_H
#define _CLIENT_H

#define LEN	(1024*16)

struct client{
	int fd;		// socket fd

	char input_buf[LEN];
	char buf[LEN];
};



struct client *create_client(int fd);
void freeClient(struct client *c);

#endif

