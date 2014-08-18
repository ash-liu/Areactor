
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "client.h"
#include "anet.h"
#include "areactor.h"
#include "network.h"
#include "adlist.h"


struct client *create_client(int fd)
{
	struct client *c = (struct client *)malloc(sizeof(struct client));

	anetNonBlock(NULL,fd);
	anetTcpNoDelay(NULL,fd);
	if (aeCreateFileEvent(g_server.el, fd, AE_READABLE, readQueryFromClientHandle, c) == AE_ERR){
		close(fd);
		free(c);
		return NULL;
	}

	c->fd = fd;
	listAddNodeTail(g_server.clients, c);
}

void freeClient(struct client *c)
{
	listNode *ln;

	// Obvious cleanup 
    aeDeleteFileEvent(g_server.el, c->fd, AE_READABLE);
    aeDeleteFileEvent(g_server.el, c->fd, AE_WRITABLE);
	
	close(c->fd); 	// close fd

	// del node in list
	ln = listSearchKey(g_server.clients, c);
	listDelNode(g_server.clients, ln);

	free(c);
}



