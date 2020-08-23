#include <socket.h>
#include <os/rmpo.h>
#include <errno.h>

// XXX: convert index into a valid socket descriptor

int __fd_is_sockfd(int fd)
{
  if( fd < 0 )
    return -1;

  if( fd >= MAX_SOCKETS )
    return -1;
}

int __find_sockfd(void)
{
  for(int i=0; i < __SOCK_MAX; i++)
  {
    if( __sockets[i].state == NO_CONN )
      return i;
  }

  return -1;
}

int socket(int family, int type, int protocol)
{
  struct __Socket *sock;
  int desc;

  desc = __find_sockfd();

  if( desc < 0 )
    return -1;

  sock = &__socket[desc];

  if( family == PF_LOCAL )
  {
    if( type == SOCK_RDM || type == SOCK_SEQPACKET || type == SOCK_RAW )
    {
      // SOCK_RDM - reliable, unordered messages
      // SOCK_SEQPACKET - reliable, ordered messages
      // protocol = 0 -> let class and type determine protocol

      if( protocol != 0 )
      {
        errno = EPROTONOSUPPORT;
        return -1;
      }

      sock->family = family;
      sock->type = type;
      sock->protocol = protocol;

      return desc;
    }
    else
    {
      errno = ESOCKTNOSUPPORT;
      return -1;
    }
  }

  errno = EPFNOSUPPORT;
  return -1;  
}

int bind(int sockfd, const struct sockaddr *my_addr, socklen_t addrlen)
{
  struct __Socket *sock;

  if( sockfd < 0 )
  {
    errno = EBADF;
    return -1;
  }
  else if( !__fd_is_sockfd(sockfd) )
  {
    errno = ENOTSOCK;
    return -1;
  }

  sock = &__socket[sockfd];


}

int connect(int sockfd, const struct sockaddr *serv_addr, socklen_t addrlen)
{
  struct __Socket *sock;

  if( sockfd < 0 )
  {
    errno = EBADF;
    return -1;
  }
  else if( !__fd_is_sockfd(sockfd) )
  {
    errno = ENOTSOCK;
    return -1;
  }

  sock = &__socket[sockfd];

  if( sock->state == CONNECTED )
  {
    errno = EISCONN;
    return -1;
  }
  else if( sock->state == CONNECTING )
  {
    errno = EALREADY;
    return -1;
  }

  if( sock->family == PF_LOCAL )
  {
    if( sock->type == SOCK_RDM || sock->type == SOCK_SEQPACKET ||
        sock->type == SOCK_RAW )
    {
//      int rmpo_desc = create_rmpo_desc( ... );
    }
  }
}
