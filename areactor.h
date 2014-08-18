
#ifndef _AREACTOR_H
#define _AREACTOR_H

#include "ae.h"
#include "adlist.h"
#include "anet.h"
#include "command.h"

#define DEFAULT_PORT	5555

struct server{
	int port;		// socket port 
	int socket_fd;	// socket fd
	char *bindaddr;             //Bind address or NULL 
	char neterr[ANET_ERR_LEN];  //Error buffer for anet.c 

	struct command *commands;	// commands

	// event loop 
	aeEventLoop *el;
	list *clients; 		//clients
};


extern struct server g_server;


#endif

