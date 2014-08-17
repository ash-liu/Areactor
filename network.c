
#include <stdio.h>
#include <errno.h>
#include "network.h"
#include "ae.h"
#include "anet.h"
#include "areactor.h"


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


void acceptCommonHandler(int fd, int flags) 
{
    struct client *c;

    if ((c = create_client(fd)) == NULL) {
        printf("Error allocating resources for the client\n");
        close(fd); /* May be already closed, just ignore errors */
        return;
    }
}


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

void addReply(struct client *c, char *obj) 
{
	memcpy(c->buf, obj, strlen(obj)+1);
	if (aeCreateFileEvent(g_server.el, c->fd, AE_WRITABLE, sendReplyToClient, c) == AE_ERR){
		printf ("create AE_WRITABLE error\n");
	}
}



void readQueryFromClient(aeEventLoop *el, int fd, void *privdata, int mask) 
{
    struct client *c = (struct client *)privdata;
    int nread, readlen;
	char read_buf[IOBUF_LEN];
	char ret_buf[IOBUF_LEN];
	int i;
	
    NOTUSED(el);
    NOTUSED(mask);
    readlen = IOBUF_LEN;

 	for (i=0; i<IOBUF_LEN; i++) {
		read_buf[i] = 0;
		ret_buf[i] = 0;
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

    //processInputBuffer(c);
    sprintf(ret_buf, "OK");
	printf("%s\n",read_buf);
	if (strcmp(read_buf, "NUM") == 0){
		sprintf(ret_buf, "%d", listLength(g_server.clients));
	}
    
	addReply(c, ret_buf);
}



