/*
 * adder.c - a minimal CGI program that adds two numbers together
 */
/* $begin adder */
#include "csapp.h"

int main(void) {
  char *buf, *p;
  char arg1[MAXLINE], arg2[MAXLINE], content[MAXLINE];
  int n1=0, n2=0;

  /* Extract the two arguments */
  if ((buf = getenv("QUERY_STRING")) != NULL) { /*num1=숫자&num2=숫자*/
    p = strchr(buf, '&');
    *p = '\0';
    // strcpy(arg1, buf+3); /* n1=을 뛰어넘음 */
    // strcpy(arg2, p+4); /* n2=을 뛰어넘음 */
    // n1 = atoi(arg1);
    // n2 = atoi(arg2);
    sscanf(buf, "n1=%d", &n1);
    sscanf(p+1, "n2=%d", &n2);
    *p = '&';
  }

  /* Make the response body */
  sprintf(content, "Welcome to add.com: ");
  sprintf(content, "%sThe Internet addition portal. \r\n<p>", content);
  sprintf(content, "%sThe answer is: %d + %d = %d\r\n<p>",
          content, n1, n2, n1 + n2);
  sprintf(content, "%sThanks for visiting!\r\n", content);
  
  /* Generate the HTTP response */
  printf("Connection: close\r\n");
  printf("Content-length: %d\r\n", (int)strlen(content));
  printf("Content-type: text/html\r\n\r\n");
  printf("%s", content);
  fflush(stdout);

  exit(0);
}

/* $end adder */
