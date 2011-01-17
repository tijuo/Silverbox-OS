#ifndef _OS_SIG
#define _OS_SIG

#define SIGTERM		0
#define SIGINT		1
#define SIGSTOP		2
#define SIGBRK		3
#define SIGMSG		4
#define SIGEXP		5
#define SIGALRM		6
#define SIGEXIT		7
#define SIGTMOUT	8

extern void set_signal_handler( void (*handler)(int, int) );

void (*__default_sig_handler)(int, int);

extern void _default_sig_handler(void);

#endif /* _OS_SIG */
