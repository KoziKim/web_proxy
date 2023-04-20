#include "csapp.h"
#include <stdio.h>

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

struct cache
{ // 캐시 구조체
  char uri[MAXLINE];
  char object[MAX_OBJECT_SIZE];
  int size;
  struct cache *prev;
  struct cache *next;
  time_t timestamp;
};

struct cache *cache_list = NULL;
int cache_size = 0;

void parse_uri(char *uri, char *host, char *port, char *path);
void modify_http_header(char *http_header, char *hostname, int port, char *path, rio_t *server_rio);
void doit(int fd);
void *thread(void *vargp);
struct cache *get_cache_node_by_uri(char *uri);
void cache_insert(char *uri, char *object, int size);
void cache_remove();
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

  if (argc != 2)
  {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(EXIT_FAILURE);
  }

  // 소켓 생성
  listenfd = Open_listenfd(argv[1]);
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
    *connfdp = Accept(listenfd, (SA *)&client_addr, &client_len); // line:netp:tiny:accept
    if (*connfdp < 0)
    {
      perror("accept failed");
      exit(EXIT_FAILURE);
    }
    Getnameinfo((SA *)&client_addr, client_len, hostname, MAXLINE, port, MAXLINE,
                0);
    printf("Accepted connection from (%s, %s)\n", hostname, port);

    Pthread_create(&tid, NULL, thread, connfdp);
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

  // 캐시 체크해서 있으면 캐시 히트
  struct cache *cached_item = get_cache_node_by_uri(uri);
  if (cached_item != NULL)
  {
    printf("--------CACHE HIT--------\n");
    printf("Proxy read %d bytes from cache and sent to client\n", cached_item->size);
    Rio_writen(fd, cached_item->object, cached_item->size);
    return;
  }

  // 요청 처리
  // 요청에서 목적지 서버의 호스트 및 포트 정보를 추출
  parse_uri(uri, host, port, path);

  // proxy 서버가 client 서버 역할 함
  // 클라이언트로부터 받은 HTTP 요청을 목적지 서버로

  serverfd = Open_clientfd(host, port); // serverfd = proxy가 client로 목적지 서버로 보낼 소켓 식별자
  sprintf(server_buf, "%s %s %s\r\n", method, path, version);
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

  modify_http_header(server_buf, host, port, path, &rio_client); // 클라이언트로부터 받은 요청의 헤더를 수정하여 보냄
  Rio_writen(serverfd, server_buf, strlen(server_buf));

  // 서버로부터 응답 읽어서 클라이언트로
  char cache_buf[MAX_OBJECT_SIZE];
  int cache_buf_size = 0;
  size_t n;
  while((n = Rio_readlineb(&rio_server, server_buf, MAXLINE)) != 0) {
    printf("Proxy received %d bytes from server and sent to client\n", n);
    Rio_writen(fd, server_buf, n);

    cache_buf_size += n;
    if (cache_buf_size < MAX_OBJECT_SIZE) {
      strcat(cache_buf, server_buf);
    }
  }
  Close(serverfd);

  if (cache_size < MAX_OBJECT_SIZE) {
    printf("--------CACHE INSERT--------\n");
    cache_insert(uri, cache_buf, cache_buf_size);
  }
}

void parse_uri(char *uri, char *host, char *port, char *path)
{
  // http://hostname:port/path 형태
  char *ptr = strstr(uri, "//");

  ptr = ptr != NULL ? ptr + 2 : uri; // http:// 건너뛰기
  char *host_ptr = ptr;              // host 부분 찾기
  char *port_ptr = strchr(ptr, ':'); // port 부분 찾기
  char *path_ptr = strchr(ptr, '/'); // path 부분 찾기

  strncpy(host, host_ptr, port_ptr - host_ptr); // 버퍼, 복사할 문자열, 복사할 길이
  // 포트가 있는 경우
  if (port_ptr != NULL && (path_ptr == NULL || port_ptr < path_ptr))
  {
    strncpy(port, port_ptr + 1, path_ptr - port_ptr - 1);
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

struct cache *get_cache_node_by_uri(char *uri)
{
  struct cache *ptr = cache_list;
  while (ptr != NULL)
  {
    if (strcmp(ptr->uri, uri) == 0)
    {                              // 캐시 리스트에 uri와 일치하는 캐시 노드 있으면
      ptr->timestamp = time(NULL); // timestamp를 현재 시간으로(마지막으로 캐시 사용된 시간 기록)
      return ptr;
    }
    ptr = ptr->next;
  }
  return NULL;
}

void cache_insert(char *uri, char *object, int size)
{
  struct cache *new_cache = malloc(sizeof(struct cache));
  strcpy(new_cache->uri, uri);
  strcpy(new_cache->object, object);
  new_cache->size = size;
  // 맨앞에 넣기
  new_cache->prev = NULL;
  new_cache->next = cache_list;
  if (cache_list != NULL)
  {
    cache_list->prev = new_cache;
  }
  cache_list = new_cache;
  cache_size = cache_size + size;

  new_cache->timestamp = time(NULL);

  while (cache_size > MAX_CACHE_SIZE)
  {
    cache_remove();
  }
}

// LRU
void cache_remove()
{
  struct cache *ptr = cache_list;
  time_t oldest_time = time(NULL);
  struct cache *oldest = NULL;

  while (ptr != NULL)
  {
    if (ptr->timestamp < oldest_time)
    {
      oldest = ptr;
      oldest_time = oldest->timestamp;
    }
    ptr = ptr->next;
  }

  if (oldest != NULL)
  {
    // 맨 앞 지우기
    if (oldest->prev == NULL)
    {
      oldest->next->prev = NULL;
      cache_list = oldest->next;
    }
    // 맨 뒤 지우기
    else if (oldest->next == NULL)
    {
      oldest->prev->next = NULL;
      oldest->prev = NULL;
    }
    // 중간 부분 지우기
    else
    {
      oldest->prev->next = oldest->next;
      oldest->next->prev = oldest->prev;
    }
    cache_size -= oldest->size;
    free(oldest);
  }
}