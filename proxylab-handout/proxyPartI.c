#include <stdio.h>
#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

#define WEB_PREFIX "http://"

/* You won't lose style points for including this long line in your code */
static const char* user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";

void doit(int fd);
void read_requesthdrs(rio_t* rp);
void parse_uri(char* uri, char* host, char* port, char* filename);
void build_header(char* header, char* host, char* port, char* filename, rio_t* client_rio);
void serve_static(int fd, char* filename, int filesize);
void get_filetype(char* filename, char* filetype);
void serve_dynamic(int fd, char* filename, char* cgiargs);
void clienterror(int fd, char* cause, char* errnum, char* shortmsg, char* longmsg);

int main(int argc, char** argv) {
    printf("%s", user_agent_hdr);
    int listenfd, connfd;
    char hostname[MAXLINE], port[MAXLINE];
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;

    /* Check command line args */
    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }

    listenfd = Open_listenfd(argv[1]);
    while (1) {
        clientlen = sizeof(clientaddr);
        connfd = Accept(listenfd, (SA*)&clientaddr, &clientlen);  // line:netp:tiny:accept
        Getnameinfo((SA*)&clientaddr, clientlen, hostname, MAXLINE,
                    port, MAXLINE, 0);
        printf("Accepted connection from (%s, %s)\n", hostname, port);
        doit(connfd);   // line:netp:tiny:doit
        Close(connfd);  // line:netp:tiny:close
    }
}
/* $end tinymain */

/*
 * doit - handle one HTTP request/response transaction
 */
/* $begin doit */
void doit(int connfd) {
    // struct stat sbuf;
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

    // printf("uri = %s, host = %s, port = %s, filename = %s\n", uri, host, port, filename);

    /* build the http header which will send to the end server */
    build_header(header, host, port, filename, &rio);

    /* connet to the end server */
    int clientfd = Open_clientfd(host, port);

    printf("open success.\n");

    Rio_readinitb(&server_rio, clientfd);
    Rio_writen(clientfd, header, strlen(header));

    /*receive message from end server and send to the client*/
    int n;
    while ((n = Rio_readlineb(&server_rio, buf, MAXLINE)) != 0) {
        printf("proxy received %d bytes,then send\n", n);
        Rio_writen(connfd, buf, n);
    }
    Close(clientfd);
}
/* $end doit */

/*
 * read_requesthdrs - read HTTP request headers
 */
/* $begin read_requesthdrs */
void read_requesthdrs(rio_t* rp) {
    char buf[MAXLINE];

    Rio_readlineb(rp, buf, MAXLINE);
    printf("%s", buf);
    while (strcmp(buf, "\r\n")) {  // line:netp:readhdrs:checkterm
        Rio_readlineb(rp, buf, MAXLINE);
        printf("%s", buf);
    }
    return;
}
/* $end read_requesthdrs */

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