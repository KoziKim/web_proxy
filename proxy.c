#include "csapp.h"
#include <stdbool.h>
#include <stdio.h>

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

// 클라이언트한테 받아서, 서버로 보냄
// 서버 응답 받아서, 클라이언트한테 보냄
// 캐싱 해야됨.

typedef struct cache_node
{
  char request[MAXLINE];
  char response[MAX_OBJECT_SIZE];
  struct cache_node *next;
} cache_node_t;

cache_node_t *cache_head = NULL; // 캐시의 헤드 포인터
int cache_size = 0;              // 현재 캐시 사이즈

// 캐시에서 해당 요청에 대한 응답을 찾아 반환하는 함수
cache_node_t *find_cache(const char *request)
{
  cache_node_t *ptr = cache_head;

  // 캐시 리스트를 순회하며 해당 요청과 일치하는 캐시를 찾음
  while (ptr != NULL)
  {
    if (strcmp(ptr->request, request) == 0)
    {
      return ptr;
    }
    ptr = ptr->next;
  }
  return NULL;
}

bool is_in_cache(const char *request)
{
  cache_node_t *cache = find_cache(request); // 캐시에서 해당 요청에 대한 응답을 찾음

  if (cache != NULL)
  {
    return true; // 캐시에 존재하는 경우 true 반환
  }
  else
  {
    return false; // 캐시에 존재하지 않는 경우 false 반환
  }
}

// 캐시에 새로운 요청-응답 쌍을 추가하는 함수
void add_cache(const char *request, const char *response)
{
  // 현재 캐시 사이즈가 최대 캐시 사이즈보다 작을 경우에만 추가
  if (cache_size + strlen(request) + strlen(response) <= MAX_CACHE_SIZE)
  {
    cache_node_t *new_cache = (cache_node_t *)Malloc(sizeof(cache_node_t));
    strcpy(new_cache->request, request);
    strcpy(new_cache->response, response);
    new_cache->next = cache_head;
    cache_head = new_cache;
    cache_size += strlen(request) + strlen(response);
  }
}

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr =
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 "
    "Firefox/10.0.3\r\n";

int main(int argc, char **argv)
{
  int listenfd, connfd;
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;

  if (argc != 2)
  {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(EXIT_FAILURE);
  }

  // 소켓 생성
  listenfd = socket(AF_INET, SOCK_STREAM, 0);
  if (listenfd < 0)
  {
    perror("socket creation failed");
    exit(EXIT_FAILURE);
  }

  // 소켓에 IP 주소와 포트 번호 할당
  struct sockaddr_in proxy_addr;
  memset(&proxy_addr, 0, sizeof(proxy_addr));
  proxy_addr.sin_family = AF_INET;
  proxy_addr.sin_addr.s_addr = htonl(INADDR_ANY); // 모든 네트워크 인터페이스에 바인딩

  // 프록시 포트 번호 설정
  int proxy_port = atoi(argv[1]);          // argv[1]을 정수로 변환
  proxy_addr.sin_port = htons(proxy_port); // 프록시의 포트 번호 설정

  if (bind(listenfd, (struct sockaddr *)&proxy_addr, sizeof(proxy_addr)) < 0)
  {
    perror("bind failed");
    exit(EXIT_FAILURE); // EXIT_FAILURE는 stdlib.h 헤더 파일에 정의된 매크로로, 프로그램의 비정상적인 종료를 나타내는 값
  }

  // 소켓을 수신 대기 상태로 전환
  if (listen(listenfd, LISTENQ) < 0)
  {
    perror("listen failed");
    exit(EXIT_FAILURE);
  }

  // 클라이언트와의 연결 수락
  while (1)
  {
    clientlen = sizeof(clientaddr);
    connfd = accept(listenfd, (struct sockaddr *)&clientaddr, &clientlen);
    if (connfd < 0)
    {
      perror("accept failed");
      exit(EXIT_FAILURE);
    }

    // 클라이언트로부터 HTTP 요청 받기
    rio_t rio_client;
    rio_readinitb(&rio_client, connfd);
    char buf[MAXLINE], response[MAXLINE];
    rio_readlineb(&rio_client, buf, MAXLINE);
    printf("Received request: %s", buf);

    // 요청 처리
    if (is_in_cache(buf)) // 캐시에 있으면
    {
      // 캐시에서 해당 요청에 대한 응답을 찾음
      *response = find_cache(buf);
      // 클라이언트에게 캐시된 응답을 전송
      Rio_writen(connfd, response, strlen(response));
    }
    else
    {
      // 목적지 서버에게 HTTP 요청 전달
      // 클라이언트로부터 받은 HTTP 요청을 목적지 서버로 전달
      int serverfd;
      rio_t rio_server;
      char request[MAXLINE];
      char method[MAXLINE], uri[MAXLINE], version[MAXLINE];
      sscanf(buf, "%s %s %s", method, uri, version);
      sprintf(request, "%s %s %s\r\n", method, uri, version);
      while (rio_readlineb(&rio_client, buf, MAXLINE) > 0)
      {
        if (strcmp(buf, "\r\n") == 0)
        {
          break;
        }
        // Host 헤더를 재작성하여 목적지 서버의 호스트 정보를 포함시킴
        if (strstr(buf, "Host:") != NULL)
        {
          strcat(request, "Host: ");
          strcat(request, uri);
        }
        else
        {
          strcat(request, buf);
        }
      }
      strcat(request, user_agent_hdr);

      // 목적지 서버에 연결
      serverfd = Open_clientfd(uri, 80);
      if (serverfd < 0)
      {
        perror("Failed to connect to server");
        continue;
      }

      // 목적지 서버로 HTTP 요청 전송
      rio_readinitb(&rio_server, serverfd);
      rio_writen(serverfd, request, strlen(request));

      // 목적지 서버로부터 HTTP 응답 받기
      while (rio_readlineb(&rio_server, response, MAXLINE) > 0)
      {
        // HTTP 응답을 클라이언트로 전송
        rio_writen(connfd, response, strlen(response));

        // 화면에 출력
        printf("Forwarding response: %s", response);

        // 목적지 서버로부터 받은 HTTP 응답을 클라이언트로 전달
        if (strcmp(response, "\r\n") == 0)
        {
          break;
        }
        // 캐시에 응답을 저장
        // 캐시에 새로운 요청-응답 쌍을 추가
        add_cache(request, response);
      }
      // 연결 종료
      Close(serverfd);
    }
    Close(connfd);
  }
  // printf("%s", user_agent_hdr);
  return 0;
}