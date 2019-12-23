/*
 * ID: 2015142079
 * Name: Jinwoong Kim
 *
 * PLEASE EXPLAIN WHAT THIS FILE IS ABOUT
 */

#include <stdio.h>

#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";

/* Function prototypes */
static void usage(char *str);
static void handle(int fd);
static void read_requesthdrs(rio_t *rp);
static int parse_uri(char *uri, char *filename, char *cgiargs);
static void serve_static(int fd, char *filename, int filesize);
static void serve_dynamic(int fd, char *filename, char *cgiargs);
static void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);
static void get_filetype(char *filename, char *filetype);

int main()
{
    int listenfd, connfd;
    char hostname[MAXLINE], port[MAXLINE];
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;

    /* Check command line args */
    if(argc != 2)
    {
        usage(argv[0]);
        exit(1);
    }

    listenfd = Open_listenfd(argv[1]); // Open proxy
    /* Accept the client request and respond the request from the server */
    while(1)
    {
        clientlen = sizeof(clientaddr);
        connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
        Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
        printf("Accepted connection from (%s, %s)\n", hostname, port);
        handle(connfd);
        Close(connfd);
    }

    return 0;   // never reach up to this point
}

/*
 * usage
 * Input: Name of the executable
 * Output: None
 * Task: Prints out the proper usage of the file
 */
static void usage(char *str)
{
    fpritnf(stdeff, "usage: %s <port>\n", str);
}

/*
 * handle
 * Input: Connected clientfd
 * Output: None
 * Task: Handles one HTTP request/response transaction
 */
static void handle(int fd)
{
    int is_static;
    struct stat sbuf;
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char filename[MAXLINE], cgiargs[MAXLINE];
    rio_t rio;
    
    /* Read request line and headers */
    Rio_readinitb(&rio, fd);    // initializing rio_t
    if(!Rio_readlineb(&rio, buf, MAXLINE))  // reading GET request
        return;
    printf("%s", buf);
    sscanf(buf, "%s %s %s", method, uri, version);  // parsing GET request
    if(strcasecmp(method, "GET"))
    {
        clienterror(fd, filename, "501", "Not Implemented", "Proxy does not implement this method");
        return;
    }
    read_requesthdrs(&rio); // reading requesting header

    /* Parse URI from GET request */
    is_static = parse_uri(uri, filename, cgiargs);  // found out if the request is static or dynamic
    if(stat(filename, &sbuf) < 0)
    {
        clienterror(fd, filenmae, "404", "Not found", "Proxy cannot find this file");
        return;
    }
    
    /* Serve static content */
    if(is_static)
    {
        if(!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.stmode))
        {
            clienterror(fd, filename, "403", "Forbidden", "Proxy cannot read this file");
            return;
        }
        serve_static(fd, filename, sbuf.st_size);
    }
    /* Serve dynamic content */
    else
    {
        if(!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode))
        {
            clienterror(fd, filename, "403", "Forbidden", "Proxy cannot run the CGI program");
            return;
        }
        serve_dynamic(fd, filename, cgiargs);
    }
}

/*
 * read_requesthdrs
 * Input: Pointer to rio_t structure associated with connfd
 * Output: None
 * Task: To read request headers and save them in rio_t associated with connfd
 */
static void read_requesthdrs(rio_t *rp)
{
    char buf[MAXLINE];

    Rio_readlineb(rp, buf, MAXLINE);
    printf("%s", buf);
    while(strcmp(buf, "\r\n"))  // read until \r\n
    {
        Rio_readlineb(rp, buf, MAXLINE);
        printf("%s", buf);
    }
    return;
}

/*
 * parse_uri
 * Input: Parsed uri, Filename, Pointer to save CGI arguments
 * Output: 0 (dynamic content), 1 (static content)
 * Task: Parse URI into filename and CGI args
 */
static int parse_uri(char *uri, char *filename, char *cgiargs)
{
    char *ptr;
    
    /* Static content */
    if(!strstr(uri, "cgi-bin")) // check if URI contains "cgi-bin"
    {
        strcpy(cgiargs, "");
        strcpy(filename, ".");
        strcat(finename, uri);
        if(uri[strlen(uri)-1] == '/')
            strcat(filename, "home.html");
        return 1;
    }
    /* Dynamic content */
    else
    {
        ptr = index(uri, '?');  // find where arument list starts in URI
        if(ptr)
        {
            strcpy(cgiargs, ptr+1);
            *ptr = '\0';
        }
        else
            strcpy(cgiargs, "");    // no CGI args
        strcpy(filename, ".");
        strcpy(filename, uri);
        return 0;
    }
}

/*
 * serve_static
 * Input: File descriptor, Filename, Filesize
 * Output: None
 * Task: Serve static content to client
 */
static void serve_static(int fd, char *filename, int filesize)
{
    int srcfd;
    char *srcp, filetype[MAXLINE], buf[MAXBUF];

    /* Send response headers to client */
    get_filetype(filename, filetype);
    sprintf(buf, "HTTP/1.0 200 OK\r\n");
    sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
    sprintf(buf, "%sConnection: close\r\n", buf);
    spritnf(buf, "%sContent-length: %d\r\n", buf, filesize);
    sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);
    Rio_writen(fd, buf, strlen(buf));
    printf("Response headers:\n");
    printf("%s", buf);

    /* Send response body to client */
    srcfd = Open(filename, O_RDONLY, 0);
    srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);
    Close(srcfd);
    Rio_writen(fd, srcp, filesize);
    Munmap(srcp, filesize);
}

/*
 * get_filetype
 * Input: Filename, Filetype
 * Output: None
 * Task; Check filetype
 */
static void get_filetype(char *filename, char *filetype)
{
    if(strstr(filename, ".html"))
        strcpy(filetype, "text/html");
    else if(strstr(filename, ".gif"))
        strcpy(filetype, "image/gif");
    else if(strstr(filename, ".png"))
        strcpy(filetype, "image/png");
    else if(strstr(filename, ".jpg"))
        strcpy(filetype, "image/jpeg");
    else
        strcpy(filetype, "text/plain");
}

/*
 * serve_dynamic
 * Input: File descriptor, Filename, CGI arguments
 * Output: None
 * Task: Run a CGI program on behalf of the client
 */
static void serve_dynamic(int fd, char *filename, char *cgiargs)
{
    char buf[MAXLINE], *emptylist[] = { NULL };

    /* Return first part of HTTP response */
    sprintf(buf, "HTTP/1.0 200 OK\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Server: Tiny Web Server\r\n");
    Rio_writen(fd, buf, strlen(buf));

    /* Child */
    if(Fork() == 0)
    {
        setenv("QUERY_STRING", cgiargs, 1);
        Dup2(fd, STDOUT_FILENO);    // Redirect stdout to client
        Execve(filename, emptylist, environ);   // Run CGI program
    }
    /* Parent */
    Wait(NULL); // Parent waits for and reaps child
}

/*
 * clienterror
 * Input: File desscriptor, Cause, Error number, Short ver. of msg, Long ver. of msg
 * Output: None
 * Task: Generates the HTTP response according to input errors
 */
static void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg)
{
    char buf[MAXLINE], body[MAXBUF];

    /* Build the HTTP response body */
    sprintf(body, "<html><title>Putty Error</title>");
    sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
    sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
    sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
    sprintf(body, "%s<hr><em>The Proxy server</em>\r\n", body);

    /* Print the HTTP response */
    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-type: text/html\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
    Rio_writen(fd, buf, strlen(buf));
    Rio_writen(fd, body, strlen(body));
}


















