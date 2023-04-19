#include "csapp.h"
#include <stdio.h>

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr =
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 "
    "Firefox/10.0.3\r\n";

void modify_http_header(char *http_header, char *hostname, int port, char *path, rio_t *server_rio);
void parse_uri(char *uri, char *host, char* port, char *path);
void doit(int fd) ;

int main(int argc, char **argv) {
  //printf("%s", user_agent_hdr);
  //return 0;
  int listenfd, connfd;
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t clientlen;
  struct sockaddr_storage server_addr;
  struct sockaddr_storage client_addr;

  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  listenfd = Open_listenfd(argv[1]);
  while (1) {
    clientlen = sizeof(client_addr);
    connfd = Accept(listenfd, (SA *)&client_addr,
                    &clientlen);  // line:netp:tiny:accept
    Getnameinfo((SA *)&client_addr, clientlen, hostname, MAXLINE, port, MAXLINE,
                0);
    printf("Accepted connection from (%s, %s)\n", hostname, port);
    
    doit(connfd);   // line:netp:tiny:doit
    Close(connfd);  // line:netp:tiny:close
  }
  printf("%s", user_agent_hdr);
  return 0;
}

void doit(int fd) {
  int finalfd; // proxy 서버가 client로서 최종 서버에게 보내는 소켓 식별자
  char client_buf[MAXLINE], server_buf[MAXLINE];

  char method[MAXLINE], uri[MAXLINE], version[MAXLINE];
  char host[MAXLINE], port[MAXLINE], path[MAXLINE];
  rio_t client_rio, server_rio;

  // 1. 클라이언트로부터 요청을 수신한다 : client - proxy(server 역할)
  Rio_readinitb(&client_rio, fd);
  Rio_readlineb(&client_rio, client_buf, MAXLINE);      // 클라이언트 요청 읽고 파싱
  sscanf(client_buf, "%s %s %s", method, uri, version);
  printf("===FROM CLIENT===\n");
  printf("Request headers:\n");
  printf("%s", client_buf);
  
  if (strcasecmp(method, "GET")) {  // GET 요청만 처리한다
    printf("Proxy does not implement the method\n");
    return; 
  }

  // 2. 요청에서 목적지 서버의 호스트 및 포트 정보를 추출한다
  parse_uri(uri, host, port, path);
  
  // Proxy 서버: client 서버로서의 역할 수행
  // 3. 추출한 호스트 및 포트 정보를 사용하여 목적지 서버로 요청을 날린다
  finalfd = Open_clientfd(host, port);   // clientfd = proxy가 client로서 목적지 서버로 보낼 소켓 식별자
  sprintf(server_buf, "%s %s %s\r\n", method, path, version);
  printf("===TO SERVER===\n");
  printf("%s\n", server_buf);
  
  Rio_readinitb(&server_rio, finalfd);
  modify_http_header(server_buf, host, port, path, &client_rio);  // 클라이언트로부터 받은 요청의 헤더를 수정하여 보냄
  Rio_writen(finalfd, server_buf, strlen(server_buf));

  // 4. 서버로부터 응답을 읽어 클라이언트에 반환한다
  size_t n;
  while((n = Rio_readlineb(&server_rio, server_buf, MAXLINE)) != 0) {
    printf("Proxy received %d bytes from server and sent to client\n", n);
    Rio_writen(fd, server_buf, n);
  }
  Close(finalfd);
}

// 목적지 서버에 보낼 HTTP 요청 헤더로 수정하기
void modify_http_header(char *http_header, char *hostname, int port, char *path, rio_t *server_rio) {
  char buf[MAXLINE], other_hdr[MAXLINE], host_hdr[MAXLINE];

  sprintf(http_header, "GET %s HTTP/1.0\r\n", path);

  while(Rio_readlineb(server_rio, buf, MAXLINE) > 0) {
    if (strcmp(buf, "\r\n") == 0) break;

    if (!strncasecmp(buf, "Host", strlen("Host")))  // Host: 
    {
      strcpy(host_hdr, buf);
      continue;
    }

    if (strncasecmp(buf, "Connection", strlen("Connection"))
        && strncasecmp(buf, "Proxy-Connection", strlen("Proxy-Connection"))
        && strncasecmp(buf, "User-Agent", strlen("User-Agent")))
    {
      strcat(other_hdr, buf);
    }
  }

  if (strlen(host_hdr) == 0) {
    sprintf(host_hdr, "Host: %s:%d\r\n", hostname, port);
  }

  sprintf(http_header, "%s%s%s%s%s%s%s", http_header, host_hdr, "Connection: close\r\n", "Proxy-Connection: close\r\n", user_agent_hdr, other_hdr, "\r\n");
  return;
}

void parse_uri(char *uri, char *host, char* port, char *path) {
  // http://hostname:port/path 형태
  char *ptr = strstr(uri, "//");
  ptr = ptr != NULL ? ptr + 2 : uri;  // http:// 생략
  char *host_ptr = ptr;               // host 부분 찾기
  char *port_ptr = strchr(ptr, ':');  // port 부분 찾기
  char *path_ptr = strchr(ptr, '/');  // path 부분 찾기
  
  // 포트가 있는 경우
  if (port_ptr != NULL && (path_ptr == NULL || port_ptr < path_ptr)) {
    strncpy(host, host_ptr, port_ptr - host_ptr); // 버퍼, 복사할 문자열, 복사할 길이
    strncpy(port, port_ptr + 1, path_ptr - port_ptr - 1);
  }
  // 포트가 없는 경우 
  else {
    strcpy(port, "80"); // 기본값
    strncpy(host, host_ptr, path_ptr - host_ptr);
  }
  strcpy(path, path_ptr);
  return;
}