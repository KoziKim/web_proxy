#include "csapp.h"
#include <stdio.h>

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

void parse_uri(char *uri, char *host, char *port, char *path);
void modify_http_header(char *http_header, char *hostname, int port, char *path, rio_t *server_rio);
void doit(int fd);
void *thread(void *vargp);

// 클라이언트한테 받아서, 서버로 보냄
// 서버 응답 받아서, 클라이언트한테 보냄

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr =
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 "
    "Firefox/10.0.3\r\n";

int main(int argc, char **argv)
{
  int listenfd, *connfdp;
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t client_len;
  struct sockaddr_storage client_addr;
  pthread_t tid;

  // struct sockaddr_in proxy_addr;
  if (argc != 2)
  {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(EXIT_FAILURE);
  }

  // 소켓 생성
  listenfd = Open_listenfd(argv[1]);
  // listenfd = socket(AF_INET, SOCK_STREAM, 0);
  if (listenfd < 0)
  {
    printf("socket 생성\n");
    perror("socket creation failed");
    exit(EXIT_FAILURE);
  }
  while (1)
  {
    client_len = sizeof(client_addr);
    connfdp = Malloc(sizeof(int));
    *connfdp = Accept(listenfd, (SA *) &client_addr, &client_len); // line:netp:tiny:accept
    if (*connfdp < 0)
    {
      perror("accept failed");
      exit(EXIT_FAILURE);
    }
    Getnameinfo((SA *)&client_addr, client_len, hostname, MAXLINE, port, MAXLINE,
                0);
    printf("Accepted connection from (%s, %s)\n", hostname, port);

    Pthread_create(&tid, NULL, thread, connfdp);

    // doit(connfdp);  // line:netp:tiny:doit
    // Close(connfdp); // line:netp:tiny:close
  }
  printf("%s", user_agent_hdr);
  return 0;
}

/* Thread routine */
void *thread(void *vargp)
{
  int connfd = *((int *)vargp);
  Pthread_detach(pthread_self());
  Free(vargp);
  doit(connfd);
  Close(connfd);
  return NULL;
}
// // 소켓에 IP 주소와 포트 번호 할당
void doit(int fd)
{
  // 클라이언트로부터 HTTP 요청 받기 : client - proxy(server 역할)
  int serverfd;
  rio_t rio_client, rio_server;
  char client_buf[MAXLINE], server_buf[MAXLINE];
  char port[MAXLINE], path[MAXLINE];
  char host[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];

  Rio_readinitb(&rio_client, fd);
  Rio_readlineb(&rio_client, client_buf, MAXLINE); // 클라이언트 요청 읽고 파싱
  sscanf(client_buf, "%s %s %s", method, uri, version);
  printf("Received request:\n%s\n", client_buf);

  if (strcasecmp(method, "GET"))
  { // GET 요청 아닐 때,
    printf("Proxy does not implement the method\n");
    return;
  }
  // 요청 처리
  // 요청에서 목적지 서버의 호스트 및 포트 정보를 추출
  parse_uri(uri, host, port, path);
  // printf("parse 후\n");
  // proxy 서버가 client 서버 역할 함
  // 클라이언트로부터 받은 HTTP 요청을 목적지 서버로

  // printf("hostname: %s\n", host);
  serverfd = Open_clientfd(host, port); // serverfd = proxy가 client로 목적지 서버로 보낼 소켓 식별자
  // printf("서브fd 연 후\n");
  sprintf(server_buf, "%s %s %s\r\n", method, path, version);
  // printf("sprintf 다음 server_buf = %s\n", server_buf);
  printf("To server:\n%s\n", server_buf);

  // 목적지 서버로 HTTP 요청 전송
  Rio_readinitb(&rio_server, serverfd);
  // printf("목적지 연결?\n");
  if (serverfd < 0)
  {
    printf("목적지 연결 실패..\n");
    perror("Failed to connect to server");
    exit(EXIT_FAILURE);
  }
  // printf("rio_server = %s\n", rio_server);
  // printf("server_buf = %s\n", server_buf);
  // printf("buf = %s\n", buf);
  // printf("rio_server = %s\n", rio_server);
  // printf("&rio_client = %s\n", &rio_client);
  modify_http_header(server_buf, host, port, path, &rio_client); // 클라이언트로부터 받은 요청의 헤더를 수정하여 보냄
  // while (Rio_readlineb(&rio_client, buf, MAXLINE) > 0)
  // {
  //   printf("Rio_readlineb 하는 중\n");
  //   if (strcmp(buf, "\r\n") == 0) {
  //     break;
  //   }
  //   // Host 헤더를 재작성하여 목적지 서버의 호스트 정보를 포함시킴
  //   if (strstr(buf, "Host:") != NULL)
  //   {
  //     printf("들어오나?\n");
  //     strcat(host_hdr, "Host: ");
  //     strcat(host_hdr, uri);
  //   }
  //   else
  //   {
  //     printf("else?\n");
  //     strcat(other_hdr, buf);
  //   }
  // }
  // printf("여기에는 오나?\n");
  // strcat(other_hdr, user_agent_hdr);
  // printf("other_hdr: \n%s\n", other_hdr);

  // sprintf(&rio_client, "%s%s%s%s%s%s%s", \
    //         &rio_client, host_hdr, "Connection: close\r\n", \
    //         "Proxy-Connection: close\r\n", user_agent_hdr, \
    //         other_hdr, "\r\n");

  // modify_http_header(server_buf, hostname, port, path, &rio_client); // 요청헤더 수정
  Rio_writen(serverfd, server_buf, strlen(server_buf));
  size_t n;
  while ((n = Rio_readlineb(&rio_server, server_buf, MAXLINE)) != 0)
  {
    Rio_writen(fd, server_buf, n);
    printf("Forwarding response: %s", server_buf);
    // 목적지 서버로부터 HTTP 응답 받기
    // // size_t n;
    // while (Rio_readlineb(&rio_server, server_buf, MAXLINE) > 0)
    // {
    //   // HTTP 응답을 클라이언트로 전송
    //   Rio_writen(fd, server_buf, MAXLINE);

    //   // 화면에 출력
    //   printf("Forwarding response: %s", server_buf);

    //   if (strcmp(server_buf, "\r\n") == 0) {
    //     break;
    //   }
  }
  // 연결 종료
  Close(serverfd);
}

void parse_uri(char *uri, char *host, char *port, char *path)
{
  // http://hostname:port/path 형태
  // printf("port1: %s\n", port);
  // printf("%s\n", uri);
  char *ptr = strstr(uri, "//");
  // printf("ptr(uri): %s\n", ptr);
  ptr = ptr != NULL ? ptr + 2 : uri; // http:// 생략
  char *host_ptr = ptr;              // host 부분 찾기
  // printf("host_ptr: %s\n", host_ptr);
  char *port_ptr = strchr(ptr, ':'); // port 부분 찾기
  // printf("port_ptr: %s\n", port_ptr);
  char *path_ptr = strchr(ptr, '/'); // path 부분 찾기
  // printf("path_ptr: %s\n", path_ptr);

  strncpy(host, host_ptr, port_ptr - host_ptr); // 버퍼, 복사할 문자열, 복사할 길이
  // printf("host: %s\n", host);
  // 포트가 있는 경우
  if (port_ptr != NULL && (path_ptr == NULL || port_ptr < path_ptr))
  {
    strncpy(port, port_ptr + 1, path_ptr - port_ptr - 1);
    // printf("port: %s\n", port);
  }
  // 포트가 없는 경우
  else
  {
    strcpy(port, "80"); // 기본값
  }
  strcpy(path, path_ptr);
  // printf("port: %s\n", port);
  // host: %s port: %s path: %s, *host, *port, *path);
  return;
}

// 목적지 서버에 보낼 HTTP 요청 헤더로 수정하기
void modify_http_header(char *http_header, char *hostname, int port, char *path, rio_t *server_rio)
{

  char buf[MAXLINE], other_hdr[MAXLINE], host_hdr[MAXLINE];

  sprintf(http_header, "GET %s HTTP/1.0\r\n", path);

  while (Rio_readlineb(server_rio, buf, MAXLINE) > 0)
  {
    if (strcmp(buf, "\r\n") == 0)
      break;

    if (!strncasecmp(buf, "Host", strlen("Host"))) // Host:
    {
      strcpy(host_hdr, buf);
      continue;
    }

    if (strncasecmp(buf, "Connection", strlen("Connection")) && strncasecmp(buf, "Proxy-Connection", strlen("Proxy-Connection")) && strncasecmp(buf, "User-Agent", strlen("User-Agent")))
    {
      strcat(other_hdr, buf);
    }
  }

  if (strlen(host_hdr) == 0)
  {
    sprintf(host_hdr, "Host: %s:%d\r\n", hostname, port);
  }

  sprintf(http_header, "%s%s%s%s%s%s%s", http_header, host_hdr, "Connection: close\r\n", "Proxy-Connection: close\r\n", user_agent_hdr, other_hdr, "\r\n");
  return;
}