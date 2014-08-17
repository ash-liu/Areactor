
#include "areactor.h"
#include "stdlib.h"
#include "stdio.h"
#include "adlist.h"
#include "anet.h"
#include "network.h"

struct server g_server;

void init_server_config()
{
	g_server.port = DEFAULT_PORT;
	g_server.bindaddr = NULL;
	g_server.commands = NULL;
	g_server.clients = listCreate();
	g_server.el = aeCreateEventLoop(100 + 1024);
	if (g_server.el == NULL) {
		printf ("el error\n");
	}

	// create tcp server
	g_server.socket_fd = anetTcpServer(g_server.neterr, g_server.port, g_server.bindaddr);
	if (g_server.socket_fd == ANET_ERR) {
		printf ("socket error\n");
		exit(1);
	}

	// binding acceptTcpHandler to client connected. 
	if (g_server.socket_fd > 0) {
		if (aeCreateFileEvent(g_server.el, g_server.socket_fd, AE_READABLE, acceptTcpHandler, NULL) == AE_ERR){
			printf ("Unrecoverable error creating server.ipfd file event");
			exit(1);
		}
	}
}

int main(int argc, char **argv)
{
	init_server_config();

	aeMain(g_server.el);
	
	return 0;
}


