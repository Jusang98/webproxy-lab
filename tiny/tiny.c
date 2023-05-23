/* $begin tinymain */
/*
 * tiny.c - A simple, iterative HTTP/1.0 Web server that uses the
 *     GET method to serve static and dynamic content.
 */
#include "csapp.h"

void doit(int fd);
void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, int filesize);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs);
void clienterror(int fd, char *cause, char *errnum,
                 char *shortmsg, char *longmsg);
// 여기서 변수 fd는 파일 디스크립터
// Unix OS에서 네트워크 소켓과 같은 파일이나 기타 입력/출력 리소스에 액세스하는 데 사용되는 추상표현이다. 즉, 시스템으로 부터 할당받은 파일이나 소켓을 대표하는 정수다
// 프로세스에서 특정 파일에 접근할 때 사용하는 추상적인 값이다
int main(int argc, char **argv)
{
  int listenfd, connfd;
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;

  /* Check command line args */
  if (argc != 2)
  {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  listenfd = Open_listenfd(argv[1]);
  while (1)
  {
    clientlen = sizeof(clientaddr);
    connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen); // 클라이언트 연결 요청 수신
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE,
                port, MAXLINE, 0);
    printf("Accepted connection from (%s, %s)\n", hostname, port); // 클라이언트의 호스트 이름과 포트 번호 파악
    doit(connfd);                                                  // 클라이언트의 요청 처리
    Close(connfd);                                                 // 연결종료
  }
}
/* $end tinymain */

/*
 * doit - handle one HTTP request/response transaction
 */
/* $begin doit */
void doit(int fd)
{
  int is_static;
  struct stat sbuf;
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
  char filename[MAXLINE], cgiargs[MAXLINE];
  rio_t rio;

  // 요청라인과 헤더라인을읽고 분석하는 코드
  Rio_readinitb(&rio, fd);
  if (!Rio_readlineb(&rio, buf, MAXLINE)) // line:netp:doit:readrequest
    return;
  printf("%s", buf);
  sscanf(buf, "%s %s %s", method, uri, version);
  // 만약 클라이언트가 GET이 아닌 다른 메소드를 호출하면 에러발생
  if (strcasecmp(method, "GET"))
  {
    clienterror(fd, method, "501", "Not Implemented",
                "Tiny does not implement this method");
    return;
  }
  read_requesthdrs(&rio);
  // 다른 요청헤더들 무시

  // URI로부터 GET request를 분석해 파일이름과 비어 있을 수도 있는 CGI인자로 분석해 정적 또는 동적 콘텐츠를 위한 것인지 나타내는 플래그 생성
  is_static = parse_uri(uri, filename, cgiargs);
  // 만약 이 파일이 디스크상에 없으면 에러메세지를 클라이언트한테 보냄
  if (stat(filename, &sbuf) < 0)
  {
    clienterror(fd, filename, "404", "Not found",
                "Tiny couldn't find this file");
    return;
  } // line:netp:doit:endnotfound

  if (is_static)
  { // 만약 요청이 정적 콘텐츠를 위한거라면
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode))
    { // line:netp:doit:readable
      clienterror(fd, filename, "403", "Forbidden",
                  "Tiny couldn't read the file");
      return;
    }
    serve_static(fd, filename, sbuf.st_size); // line:netp:doit:servestatic
  }
  else
  { // 동적 콘텐츠를 위한거면
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode))
    {
      clienterror(fd, filename, "403", "Forbidden",
                  "Tiny couldn't run the CGI program");
      return;
    }
    serve_dynamic(fd, filename, cgiargs); // line:netp:doit:servedynamic
  }
}

// Tiny는 요청 헤더 내의 어떤 정보도 사용하지 않는다. read_requestthdrs함수를 호출해서 이들을 읽고 무시한다.
void read_requesthdrs(rio_t *rp)
{
  char buf[MAXLINE];

  Rio_readlineb(rp, buf, MAXLINE);
  printf("%s", buf);
  while (strcmp(buf, "\r\n")) // 버퍼에서 읽은 줄이 '\r\n'이 아닐 때까지 반복 (strcmp: 두 인자가 같으면 0 반환)
  {
    Rio_readlineb(rp, buf, MAXLINE);
    printf("%s", buf);
  }
  return;
}

// uri 파싱 함수
int parse_uri(char *uri, char *filename, char *cgiargs)
{
  char *ptr;

  if (!strstr(uri, "cgi-bin"))
  { // 정적 콘텐츠일때 --> uri에 cgi-bin 이 없으면 정적 콘텐츠다.
    strcpy(cgiargs, "");
    strcpy(filename, ".");
    strcat(filename, uri); // 파일이름 뒤에 uri를 붙혀
    if (uri[strlen(uri) - 1] == '/')
      strcat(filename, "home.html"); // uri 제일 뒤가 /로 끝나면 home.html 붙혀
    return 1;
  }
  // 동적 콘텐츠일때
  else
  { // 모든 CGI 인자들을 추출하고
    ptr = index(uri, '?');
    if (ptr)
    {
      strcpy(cgiargs, ptr + 1);
      *ptr = '\0';
    }
    // 나머지 URI 부분을 상대 리눅스 파일 이름으로 변환
    else
      strcpy(cgiargs, "");
    strcpy(filename, ".");
    strcat(filename, uri);
    return 0;
  }
}

void serve_static(int fd, char *filename, int filesize)
{
  int srcfd;
  char *srcp, filetype[MAXLINE], buf[MAXBUF];
  rio_t rio;
  /* Send response headers to client */
  get_filetype(filename, filetype);    // line:netp:servestatic:getfiletype
  sprintf(buf, "HTTP/1.1 200 OK\r\n"); // line:netp:servestatic:beginserve
  sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
  sprintf(buf, "%sConnection: close\r\n", buf);
  sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
  sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);
  Rio_writen(fd, buf, strlen(buf));
  // 헤더 정보 전송
  printf("Response headers:\n");
  printf("%s", buf);
  // 응답 바디를 클라한테 보내

  srcfd = Open(filename, O_RDONLY, 0); // 파일 열기
  // srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0); // 파일을 가상메모리에 매핑
  srcp = malloc(filesize);          // 파일을 위한 메모리 할당
  Rio_readinitb(&rio, srcfd);       // 파일을 버퍼로 읽기위한 초기화
  Rio_readn(srcfd, srcp, filesize); // 읽기
  Close(srcfd);                     // 파일 디스크립터 닫기
  Rio_writen(fd, srcp, filesize);   // 파일 내용을 클라이언트에게 전송 (응답 바디 전송)
  free(srcp);                       // 매핑된 가상메모리 해제
}

void get_filetype(char *filename, char *filetype)
{
  if (strstr(filename, ".html"))
    strcpy(filetype, "text/html");
  else if (strstr(filename, ".gif"))
    strcpy(filetype, "image/gif");
  else if (strstr(filename, ".png"))
    strcpy(filetype, "image/png");
  else if (strstr(filename, ".jpg"))
    strcpy(filetype, "image/jpeg");
  else if (strstr(filename, ".mp4"))
    strcpy(filetype, "video/mp4");
  else
    strcpy(filetype, "text/plain");
}

// Tiny는 자식 프로세스를 fork하고 그 후에 CGI프로그램을 자식의 컨텍스트에서 실행한다.(모든 종류의 동적 컨텐츠를 제공)
// fork()? - 부모 프로세스에서 fork()하면 부모의 메모리를 그대로 복사한 자식 프로세스 생성된다. 그 이후 각자 메모리를 사용하여 실행된다.
// -> CGI 프로그램이 자식 컨텍스트에서 실행된다. -> 부모 프로세스의 어떠한 간섭도 x 자식 종료까지 wait 함수에서 블록됨
void serve_dynamic(int fd, char *filename, char *cgiargs)
{
  char buf[MAXLINE], *emptylist[] = {NULL};

  sprintf(buf, "HTTP/1.1 200 OK\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Server: Tiny Web Server\r\n");
  Rio_writen(fd, buf, strlen(buf));

  if (Fork() == 0)
  {
    setenv("QUERY_STRING", cgiargs, 1);   // QUERY_STRING 환경 변수를 URI에서 추출한 CGI 인수로 설정
    Dup2(fd, STDOUT_FILENO);              // 자식 프로세스의 표준 출력을 클라이언트 소켓에 연결된 파일 디스크립터로 변경
    Execve(filename, emptylist, environ); // 현재 프로세스의 이미지를 filename 프로그램으로 대체
  }
  Wait(NULL); // 자식 종료까지 부모는 wait
}

// 클라이언트에 에러를 전송하는 함수(cause: 오류 원인, errnum: 오류 번호, shortmsg: 짧은 오류 메시지, longmsg: 긴 오류 메시지)
void clienterror(int fd, char *cause, char *errnum,
                 char *shortmsg, char *longmsg)
{
  char buf[MAXLINE], body[MAXBUF]; // buf: HTTP 응답 헤더, body: HTML 응답의 본문인 문자열(오류 메시지와 함께 HTML 형태로 클라이언트에게 보여짐)

  // 응답 본문 생성하는 코드
  sprintf(body, "<html><title>Tiny Error</title>");
  sprintf(body, "%s<body bgcolor="
                "ffffff"
                ">\r\n",
          body);
  sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
  sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
  sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);

  // 응답 출력
  sprintf(buf, "HTTP/1.1 %s %s\r\n", errnum, shortmsg);
  Rio_writen(fd, buf, strlen(buf)); // 클라이언트에 전송 '버전 에러코드 에러메시지'
  sprintf(buf, "Content-type: text/html\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
  Rio_writen(fd, buf, strlen(buf)); // 컨텐츠 크기
  Rio_writen(fd, body, strlen(body));
}
