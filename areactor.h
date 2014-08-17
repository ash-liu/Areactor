
#ifndef _AREACTOR_H
#define _AREACTOR_H

#include "dict.h"
#include "ae.h"
#include "adlist.h"
#include "anet.h"

#define DEFAULT_PORT	55555
#define IOBUF_LEN         (1024*16)  /* Generic I/O buffer size */

struct server{
	int port;		// socket port 
	int socket_fd;	// socket fd
	char *bindaddr;             //Bind address or NULL 
	char neterr[ANET_ERR_LEN];  //Error buffer for anet.c 

	dict *commands;	// commands

	// event loop 
	aeEventLoop *el;
	list *clients; 		//clients
};


extern struct server g_server;


#endif

