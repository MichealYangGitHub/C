/* @Description: 
 * @Author: MichealYang
 * @Date: 2015/10/20
 */
#include <cstdlib>
#include <cstdio>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>  //file control function
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <exception>

#define VERSION 1
#define BUFSIZE 8096

enum LOGTYPE
{
    ERROR = 42,
    LOG = 44,
    FORBIDDEN = 403,
    NOTFOUND = 404,
};

struct {
    char *ext;
    char *fileType;
} extensions[] = {
    {"gif", "image/gif" },
    {"jpeg","image/jpeg"},
    {"jpg", "image/jpg" },
    {"png", "image/png" },
    {"zip", "image/zip" },
    {"htm", "text/html" },
    {"html","text/html" },
    {0,0} };

void logger(LOGTYPE type, char *s1, char *s2, int sockFd)
{
    int fd; //file descriptor of log file
    char logBuf[BUFSIZE * 2] = {0};
    switch(type)
    {
        case ERROR:
        {
            snprintf(logBuf, sizeof(logBuf), "[ERROR] %s:%s ERRNO:%d pid:%d", s1, s2, errno, getpid()); //getpid() in unistd.h
            break;
        }
        case LOG:
        {
            snprintf(logBuf, sizeof(logBuf), "[INFO] %s:%s, %d", s1, s2, sockFd);
            break;
        }
        case FORBIDDEN:
        {
            char httpBody[] = "<html><head>\n<title>403 Forbidden</title>\n</head><body>\n<h1>Forbidden</h1>\nThe requested URL, file type or operation is not allowed on this simple static file webserver.\n</body></html>";  //strlen() in string.h
            char httpResponse[BUFSIZE * 2] = {0};
            snprintf(httpResponse, sizeof(httpResponse), "HTTP/1.1 403 Forbidden\nContent-length:%lu\nConnection:close\nContent-type:text/html\n\n%s", strlen(httpBody), httpBody);
            write(sockFd, httpResponse, strlen(httpResponse));
            snprintf(logBuf, sizeof(logBuf), "[FORBIDDEN] %s:%s", s1, s2);
            break;
        }
        case NOTFOUND:
        {
            char httpBody[] = "<html><head>\n<title>404 Not Found</title>\n</head><body>\n<h1>Not Found</h1>\nThe requested URL was not found on this server.\n</body></html>";
            char httpResponse[BUFSIZE * 2] = {0};
            snprintf(httpResponse, sizeof(httpResponse), "HTTP/1.1 404 Not Found\nContent-length:%lu\nConnection:close\nContent-type:text/html\n\n%s", strlen(httpBody), httpBody);
            write(sockFd, httpResponse, strlen(httpResponse));
            snprintf(logBuf, sizeof(logBuf), "[NOTFOUND] %s:%s", s1, s2);
            break;
        }
    }
    if((fd = open("nweb.log", O_CREAT| O_WRONLY | O_APPEND,0644)) >= 0) //O_CREATE etc in fcntl.h
    {
        (void)write(fd, logBuf, strlen(logBuf));
        (void)write(fd, "\n", 1);
        (void)close(fd);
    }
    if(type == ERROR)
    {
        (void)write(2, logBuf, strlen(logBuf));
        (void)write(2, "\n", 1);
    }
    if(type == ERROR || type == FORBIDDEN || type == NOTFOUND) exit(3);
}

void web(int fd, int hit)
{
    int ret;
    static char buffer[BUFSIZE+1];
    ret = read(fd, buffer, BUFSIZE);
    if(ret == 0 || ret == -1)
    {
        logger(FORBIDDEN, "failed to read browser request", "", fd);
    }
    if(ret > 0 && ret < BUFSIZE){
        buffer[ret] = 0;
    }else{
        buffer[0] = 0;
    }
    for(long i=0; i<ret; i++)
    {
        if(buffer[i] == '\r' || buffer[i] == '\n')
            buffer[i] == '*';   //just for log
    }
    logger(LOG, "request", buffer, fd);
    if(strncmp(buffer, "GET ", 4) && strncmp(buffer, "get ", 4)){
        logger(FORBIDDEN, "Only simple GET operation supported", buffer, fd);
    }
    //Get request row
    for(long i=4; i<ret; i++)
    {
        if(buffer[i] == ' ')
        {
            buffer[i] = 0;
            break;
        }
    }
    //default index.html
    if(!strncmp(buffer, "GET /\0", 6) || !strncmp(buffer, "get /\0", 6))
    {
        (void)strcpy(buffer, "GET /index.html\0");
    }
    //supported file extensions
    int bufferLen = strlen(buffer);
    char *fstr;
    for(int i=0; extensions[i].ext != 0; i++)
    {
        int len = strlen(extensions[i].ext);
        if(!strncmp(&buffer[bufferLen - len], extensions[i].ext, len))
        {
            fstr = extensions[i].fileType;
            break;
        }
    }
    if(fstr == 0)
        logger(FORBIDDEN, "file extension type not supported", buffer, fd);
    //open file
    int fp;
    if((fp = open(&buffer[5], O_RDONLY)) == -1)
    {
        logger(NOTFOUND, "failed to open file", &buffer[5], fd);
    }
    logger(LOG, "SEND", &buffer[5], hit);
    //get file length
    long resLen = (long)lseek(fp, (off_t)0, SEEK_END);  //lseek to the file end to get the length
    (void)lseek(fp, (off_t)0, SEEK_SET);    //lseek to the file start ready for reading
    (void)sprintf(buffer,"HTTP/1.1 200 OK\nServer: nweb/%d.0\nContent-Length: %ld\nConnection: close\nContent-Type: %s\n\n", VERSION, resLen, fstr); /* Header + a blank line */
    logger(LOG,"Header",buffer,hit);
    (void)write(fd,buffer,strlen(buffer));
    /* send file in 8KB block - last block may be smaller */  
    while((ret = read(fp, buffer, BUFSIZE)) > 0)
    {
        (void)write(fd, buffer, ret);
    }
    sleep(1);   ///* allow socket to drain before signalling the socket is closed */ 
    close(fd);
    exit(1);
}

int main(int argc, char **argv)
{
    int listenFd, sockFd, port, hit, pid;
    socklen_t length;
    struct sockaddr_in serverAddr;
    struct sockaddr_in clienAddr;
    //resolve arguments
    //two parameters are needed. the first one is port, the last one is the web root
    if(argc != 3 || !strcmp(argv[1], "-h"))
    {
        char *Usage = "Usage:\n\t@MODE: ./server port webroot\n \
        \t@Description: This is a http web server\n \
        \t\tonly servers out file/web pages with extensions named below and \n \
        \t\tonly from the named directory or its sub-directories\n \
        \t\tOnly supported:";
        printf("%s", Usage);
        for(int i=0; extensions[i].ext != 0; i++)
        {
            (void)printf("\t%s", extensions[i].ext);
        }
        (void)printf("\n\t\tNot Supported: URLs including \"..\", Java, Javascript, CGI\n"
                "\t\tNot Supported: directories / /etc /bin /lib /tmp /usr /dev /sbin \n"
                "\t\tNo warranty given or implied\n\tNigel Griffiths nag@uk.ibm.com\n"  );  
        exit(0);
    }
    if(chdir(argv[2]) == -1)
    {
        printf("ERROR! Can't to change to directory: %s\n", argv[2]);
        exit(4);
    }
    //new socket
    if((listenFd = socket(AF_INET, SOCK_STREAM, 0)) < 0)    //socket() in socket.h
    {
        logger(ERROR, "system call", "socket", 0);
        return -1;
    }
    //bind
    port = atoi(argv[1]);
    if(port < 0 || port > 60000)
        logger(ERROR,"Invalid port number (try 1->60000)",argv[1],0);
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serverAddr.sin_port = htons(port);
    if(bind(listenFd, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0)
    {
        logger(ERROR, "system call", "bind", 0);
        return -1;
    }
    //listen
    if(listen(listenFd, 64) < 0)
    {
        logger(ERROR,"system call", "listen", 0);
        return -1;
    }
    logger(LOG, "server started", argv[1], getpid());
    printf("%s:%d\n", "web started", port);
    for(hit=1; ; hit++)
    {
        //accept
        length = sizeof(clienAddr);
        if((sockFd = accept(listenFd, (struct sockaddr *)&clienAddr, &length)) < 0)
        {
            logger(ERROR, "system call", "accept", 0);
            return -1;
        }
        if((pid = fork()) < 0)
        {
            logger(ERROR, "system call", "fork", 0);
            return -1;
        }else{
            if(pid == 0)    //child process
            {
                close(listenFd);
                web(sockFd, hit);
            }else{
                close(sockFd);
            }
        }
    }
}
