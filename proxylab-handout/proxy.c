#include <stdio.h>
#include "cache.h"
#include "csapp.h"
#include "sbuf.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

#define READER_SAME_TIME 3

/* http protocol */
#define WEB_PREFIX "http://"

/* Concurrent Program */
#define NTHREADS 4
#define SBUFSIZE 16

/* You won't lose style points for including this long line in your code */
static const char* user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";

static sbuf_t sbuf; /* Shared buffer of connected descriptors */
static cache_t cache;
static int readcnt;
static sem_t mutex, W;

void* thread(void* vargp);
void doit(int fd);
void parse_uri(char* uri, char* host, char* port, char* filename);
void build_header(char* header, char* host, char* port, char* filename, rio_t* client_rio);
cache_node* readItem(char* targetURI, int clientfd);
void readAndWriteResponse(int connfd, rio_t* server_rio, char* uri);
void clienterror(int fd, char* cause, char* errnum, char* shortmsg, char* longmsg);

int main(int argc, char** argv) {
    printf("%s", user_agent_hdr);
    int listenfd, connfd;
    char hostname[MAXLINE], port[MAXLINE];
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;
    pthread_t tid;

    /* Check command line args */
    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }
    listenfd = Open_listenfd(argv[1]);

    sbuf_init(&sbuf, SBUFSIZE);
    for (int i = 0; i < NTHREADS; i++) { /* create worker threads */
        Pthread_create(&tid, NULL, thread, NULL);
    }

    /* initialize the cache */
    initializeCache(&cache);

    /* initialize the read / write lock*/
    readcnt = 0;
    Sem_init(&mutex, 0, READER_SAME_TIME);
    Sem_init(&W, 0, 1);

    while (1) {
        clientlen = sizeof(clientaddr);
        connfd = Accept(listenfd, (SA*)&clientaddr, &clientlen);  // line:netp:tiny:accept
        Getnameinfo((SA*)&clientaddr, clientlen, hostname, MAXLINE,
                    port, MAXLINE, 0);
        printf("Accepted connection from (%s, %s)\n", hostname, port);
        sbuf_insert(&sbuf, connfd);
    }
}
/* $end tinymain */

void* thread(void* vargp) {
    Pthread_detach(pthread_self());
    while (1) {
        int connfd = sbuf_remove(&sbuf);
        doit(connfd);
        Close(connfd);
    }
}

/*
 * doit - handle one HTTP request/response transaction
 */
/* $begin doit */
void doit(int connfd) {
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char host[MAXLINE], port[MAXLINE], filename[MAXLINE];
    char header[MAXLINE];
    rio_t rio, server_rio;

    /* Read request line and headers */
    Rio_readinitb(&rio, connfd);
    if (!Rio_readlineb(&rio, buf, MAXLINE))  // line:netp:doit:readrequest
        return;
    printf("%s", buf);
    sscanf(buf, "%s %s %s", method, uri, version);  // line:netp:doit:parserequest
    if (strcasecmp(method, "GET")) {                // line:netp:doit:beginrequesterr
        clienterror(connfd, method, "501", "Not Implemented",
                    "Proxy does not implement this method");
        return;
    }  // line:netp:doit:endrequesterr

    /* Parse URI from GET request to host:port/filename */
    parse_uri(uri, host, port, filename);

    /* build the http header which will send to the end server */
    build_header(header, host, port, filename, &rio);

    cache_node* node = readItem(uri, connfd);
    if (node != NULL) {
        printf("using cache to send back.\n");
        Rio_writen(connfd, node->respHeader, node->respHeaderLen);
        Rio_writen(connfd, "\r\n", strlen("\r\n"));
        Rio_writen(connfd, node->respBody, node->respBodyLen);
        return;
    }

    /* connet to the end server */
    int clientfd = Open_clientfd(host, port);

    printf("open success.\n");

    Rio_readinitb(&server_rio, clientfd);
    Rio_writen(clientfd, header, strlen(header));

    /*receive message from end server and send to the client*/
    // int n;
    // while ((n = Rio_readlineb(&server_rio, buf, MAXLINE)) != 0) {
    //     printf("proxy received %d bytes,then send\n", n);
    //     Rio_writen(connfd, buf, n);
    // }
    // Close(clientfd);
    readAndWriteResponse(connfd, &server_rio, uri);
    Close(clientfd);
}
/* $end doit */

/*
 * parse_uri - parse URI into filename and CGI args
 *             return 0 if dynamic content, 1 if static
 */
/* $begin parse_uri */
void parse_uri(char* uri, char* host, char* port, char* filename) {
    char* hostp = strstr(uri, WEB_PREFIX) + strlen(WEB_PREFIX);
    char* slash = strstr(hostp, "/");
    char* colon = strstr(hostp, ":");
    // get host name
    strncpy(host, hostp, colon - hostp);
    // get port number
    strncpy(port, colon + 1, slash - colon - 1);
    // get file name
    strcpy(filename, slash);
}
/* $end parse_uri */

void build_header(char* header, char* host, char* port, char* filename, rio_t* rp) {
    int Host = 0, UserAgent = 0, Connection = 0, ProxyConnection = 0;
    char buf[MAXLINE];
    char* findp;

    sprintf(header, "GET %s HTTP/1.0\r\n", filename);

    Rio_readlineb(rp, buf, MAXLINE);
    printf("%s", buf);
    while (strcmp(buf, "\r\n")) {
        strcat(header, buf);
        if ((findp = strstr(buf, "User-Agent:")) != NULL) {
            UserAgent = 1;
        } else if ((findp = strstr(buf, "Proxy-Connection:")) != NULL) {
            ProxyConnection = 1;
        } else if ((findp = strstr(buf, "Connection:")) != NULL) {
            Connection = 1;
        } else if ((findp = strstr(buf, "Host:")) != NULL) {
            Host = 1;
        }
        Rio_readlineb(rp, buf, MAXLINE);
        printf("%s", buf);
    }

    if (Host == 0) {
        sprintf(buf, "Host: %s\r\n", host);
        strcat(header, buf);
    }
    /** append User-Agent */
    if (UserAgent == 0) {
        strcat(header, user_agent_hdr);
    }
    /** append Connection */
    if (Connection == 0) {
        sprintf(buf, "Connection: close\r\n");
        strcat(header, buf);
    }
    /** append Proxy-Connection */
    if (ProxyConnection == 0) {
        sprintf(buf, "Proxy-Connection: close\r\n");
        strcat(header, buf);
    }
    /* add terminator for request */
    strcat(header, "\r\n");
}

cache_node* readItem(char* targetURI, int clientfd) {
    P(&mutex);
    readcnt++;
    if (readcnt == 1)
        P(&W);
    V(&mutex);

    /***** reading section starts *****/
    cache_node *cur = cache.head->next, *res = NULL;
    rio_t rio;
    Rio_readinitb(&rio, clientfd);
    while (cur->flag != '@') {
        if (strcmp(targetURI, cur->uri) == 0) {
            res = cur;
            break;
        }
        cur = cur->next;
    }
    /***** reading section ends *****/

    P(&mutex);
    readcnt--;
    if (readcnt == 0)
        V(&W);
    V(&mutex);

    return res;
}

void readAndWriteResponse(int connfd, rio_t* server_rio, char* uri) {
    char buf[MAXLINE];
    int n, totalBytes = 0;

    cache_node* node = Malloc(sizeof(*node));
    node->flag = '0';
    strcpy(node->uri, uri);
    *node->respHeader = 0;
    *node->respBody = 0;

    printf("node->uri == %s\n", node->uri);

    while ((n = rio_readlineb(server_rio, buf, MAXLINE)) != 0) {
        Rio_writen(connfd, buf, n);

        if (strcmp(buf, "\r\n") == 0)  // prepare for body part
            break;

        strcat(node->respHeader, buf);
        totalBytes += n;
    }
    node->respHeaderLen = totalBytes;
    totalBytes = 0;

    while ((n = Rio_readlineb(server_rio, buf, MAXLINE)) != 0) {
        Rio_writen(connfd, buf, n);
        strcat(node->respBody, buf);
        totalBytes += n;
    }
    node->respBodyLen = totalBytes;

    printf("===== node->respHeader =====\n%s===== node->respBody =====\n%s\n", node->respHeader, node->respBody);
    printf("proxy received %d bytes,then send\n", totalBytes);

    if (totalBytes >= MAX_OBJECT_SIZE) {
        Free(node);
        return;
    }

    /* try to read current capacity and write into it */
    P(&W);
    writeToCache(&cache, node);
    printf("======= writng this node into cache =========\n");
    V(&W);
}

/*
 * clienterror - returns an error message to the client
 */
/* $begin clienterror */
void clienterror(int fd, char* cause, char* errnum, char* shortmsg, char* longmsg) {
    char buf[MAXLINE];

    /* Print the HTTP response headers */
    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-type: text/html\r\n\r\n");
    Rio_writen(fd, buf, strlen(buf));

    /* Print the HTTP response body */
    sprintf(buf, "<html><title>Tiny Error</title>");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf,
            "<body bgcolor="
            "ffffff"
            ">\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "%s: %s\r\n", errnum, shortmsg);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "<p>%s: %s\r\n", longmsg, cause);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "<hr><em>The Proxy server</em>\r\n");
    Rio_writen(fd, buf, strlen(buf));
}