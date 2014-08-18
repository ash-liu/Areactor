
#include <stdio.h>
#include <errno.h>
#include "network.h"
#include "ae.h"
#include "anet.h"
#include "areactor.h"
#include "command.h"
#include "client.h"


//----------------------------
// handles 

// handle for socket connect
void acceptTcpHandler(aeEventLoop *el, int fd, void *privdata, int mask) 
{
    int cport, cfd;
    char cip[128];
    NOTUSED(el);
    NOTUSED(mask);
    NOTUSED(privdata);

    // 连接
    cfd = anetTcpAccept(g_server.neterr, fd, cip, &cport);
    if (cfd == AE_ERR) {
        printf("Accepting client connection: %s", g_server.neterr);
        return;
    }
    printf("Accepted %s:%d\n", cip, cport);

    // 创建客户端
    acceptCommonHandler(cfd, 0);
    
}


// handle for socket firstly readable.
void acceptCommonHandler(int fd, int flags) 
{
    struct client *c;

    if ((c = create_client(fd)) == NULL) {
        printf("Error allocating resources for the client\n");
        close(fd); /* May be already closed, just ignore errors */
        return;
    }
}

// handle for socket readable
void readQueryFromClientHandle(aeEventLoop *el, int fd, void *privdata, int mask) 
{
    struct client *c = (struct client *)privdata;
    int nread, readlen;
	char *read_buf = c->input_buf;
	int i;
	
    NOTUSED(el);
    NOTUSED(mask);
    readlen = IOBUF_LEN;

 	for (i=0; i<IOBUF_LEN; i++) {
		read_buf[i] = 0;
	}
	
    nread = read(fd, read_buf, readlen);

    // 处理读错误值和 EOF （客户端已关闭）
    if (nread == -1) {
        if (errno == EAGAIN) {
            nread = 0;
        } else {
            printf("Reading from client: %s\n",strerror(errno));
            freeClient(c);
            return;
        }
    } else if (nread == 0) {
        printf("Client closed connection\n");
        freeClient(c);
        return;
    }

	//analysis and process cmd
    process_input(c);    
}


//-------------------------
// functions for reply to client
//-
void sendReplyToClient(aeEventLoop *el, int fd, void *privdata, int mask) 
{
	int nwritten = 0;
	struct client *c = (struct client *)privdata;
	
	nwritten = write(fd, c->buf, strlen(c->buf));
	
	// 写入出错
    if (nwritten == -1) {
        // 被中断
        if (errno == EAGAIN) {
            nwritten = 0;
        // 处理出错
        } else {
            printf("Error writing to client: %s\n", strerror(errno));
            freeClient(c);
            return;
        }
    }
	
	// delete the event which be pressed. 
	aeDeleteFileEvent(g_server.el, c->fd, AE_WRITABLE);
}

// str = NULL  => reply string is ready in c->buf, otherwise copy to c->buf 
void addReply(struct client *c, char *str) 
{
	if (str != NULL) {
		memcpy(c->buf, str, strlen(str)+1);
	}
	
	if (aeCreateFileEvent(g_server.el, c->fd, AE_WRITABLE, sendReplyToClient, c) == AE_ERR){
		printf ("create AE_WRITABLE error\n");
	}
}


//---------------------------------
// main process for socket date
//
void process_input(struct client *c)
{
	CommandProc *pro;
	char *name;
	int i;

	//it should be analysis here. i not do it ... ugly..
	for (i=0; i<get_commands_number(); i++) {
		printf ("%s & %s\n", get_command_from_index(i)->name, c->input_buf);
		if (memcmp(get_command_from_index(i)->name, c->input_buf, strlen(get_command_from_index(i)->name)) == 0) {
			(get_command_from_index(i)->pro)(c);
		}
	}
}

