#ifndef OSLIB_H
#define OSLIB_H

#include <os/mutex.h>
#include <types.h>

#define INIT_EXHANDLER 		    1
#define INIT_SERVER		        1

#define IRQ_ANY			        -1

#define KERNEL_TID		        (tid_t)0
#define NULL_SHMID		        (shmid_t)-1

typedef unsigned long shmid_t;

#define MAX_MSG_LEN	MSG_LEN
#define MSG_LEN         1024

#define TCB_STATUS_INACTIVE     0u
#define TCB_STATUS_PAUSED       1u  // Infinite blocking state
#define TCB_STATUS_ZOMBIE       2u  // Thread is waiting to be released
#define TCB_STATUS_READY        3u  // Thread is ready to be scheduled to a processor
#define TCB_STATUS_RUNNING      4u  // Thread is already scheduled to a processor
#define TCB_STATUS_WAIT_SEND    5u  // Thread is waiting for a sender to send a message
#define TCB_STATUS_WAIT_RECV    6u  // Thread is waiting for a receipient to receive a message

#define MAX(a,b)	            ({ __typeof__(a) _a=(a); __typeof__(b) _b=b; (_a > _b) ? _a : _b; })
#define MIN(a,b)                ({ __typeof__(a) _a=(a); __typeof__(b) _b=b; (_a < _b) ? _a : _b; })

#ifdef __cplusplus
extern "C" {
#endif

char *strdup(const char *str);
char *strndup(const char *str, size_t n);
char *strappend(const char *str, const char *add);

  /*
int fatGetAttributes( const char *path, unsigned short devNum, struct FileAttributes *attrib );
int fatGetDirList( const char *path, unsigned short devNum, struct FileAttributes *attrib,
                   size_t maxEntries );
int fatReadFile( const char *path, unsigned int offset, unsigned short devNum, char *fileBuffer, size_t length );
int fatCreateDir( const char *path, const char *name, unsigned short devNum );
int fatCreateFile( const char *path, const char *name, unsigned short devNum );
   */
#ifdef __cplusplus
}
#endif /*__cplusplus */
#endif /* OSLIB_H */
