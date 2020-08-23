
#define PF_INET			0
#define PF_INET6		1
#define PF_UNIX			2
#define PF_LOCAL		3

#define SOCK_STREAM		0
#define SOCK_DGRAM		1
#define SOCK_SEQPACKET		2
#define SOCK_RAW		3

#define IPPROTO_TCP		0
#define IPPROTO_UDP		1

#define __SOCKFLG_NOBLOCK	1	

enum __SockState { NO_CONN, BOUND, ACCEPTING, CLOSING, LISTENING, CONNECTING,
                   CONNECTED };

struct sockaddr
{
  unsigned short 	sa_family;
  char			sa_data;
};

struct sockaddr_in
{
  unsigned char 	sin_family;
  unsigned short 	sin_port;
  struct in_addr 	sin_addr;
  char 			sin_zero[8];
};

struct in_addr
{
  unsigned long s_addr;
};

struct __Socket
{
  struct sockaddr bound_addr, remote_addr;
  unsigned sbuf_size, rbuf_size, timeout;
  int family, protocol, type;
  int proto_desc;
  int flags;
  enum __SockState state;
};

int socket(int family, int type, int protocol);
int bind(int sockfd, const struct sockaddr *my_addr, socklen_t addrlen);
int listen(int sockfd, int backlog);
int accept(int sockfd, struct sockaddr *cliaddr, socklen_t *addrlen);
int connect(int sockfd, const struct sockaddr *serv_addr, socklen_t addrlen);
ssize_t read(int sockfd, void *buf, size_t bytes);
ssize_t write(int sockfd, const void *buf, size_t bytes);
int close(int sockfd);

struct hostent *gethostbyname(const char *name);
struct hostent *gethostbyaddr(const void *addr, int len, int type);
