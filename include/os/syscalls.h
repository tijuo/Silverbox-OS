#ifndef OS_SYSCALLS
#define OS_SYSCALLS

#include <types.h>

#define SYSCALL_INT             0x40

#define SYS_SEND                0x00
#define SYS_RECEIVE             0x01
#define SYS_EXIT                0x02
#define SYS_MAP                 0x03
#define SYS_GRANT               0x04
#define SYS_GET_THREAD_INFO     0x05
#define SYS_CREATE_THREAD       0x06
#define SYS_START_THREAD        0x07
#define SYS_PAUSE               0x08
#define SYS_SLEEP               0x09
#define SYS_REGISTER_INT        0x0A
#define SYS_END_IRQ             0x0B
#define SYS_END_PAGE_FAULT      0x0C
#define SYS_MAP_PAGE_TABLE      0x0D
#define SYS_GRANT_PAGE_TABLE    0x0E
#define SYS_UNMAP               0x0F
#define SYS_UNMAP_PAGE_TABLE    0x10
#define SYS_RAISE               0x11
#define SYS_SET_SIG_HANDLER     0x12
#define SYS_SET_IO_PERM         0x13
#define SYS_DESTROY_THREAD      0x14
#define SYS_UNREGISTER_INT      0x15

int __send( tid_t recipient, void *msg, int timeout );
int __receive( tid_t sender, void *buf, int timeout );
int __pause( void );
int __pause_thread( tid_t tid );
int __start_thread( tid_t tid );
void __yield( void );
int __get_thread_info( tid_t tid, struct ThreadInfo *info );
int __register_int( int int_num );
int __unregister_int( int int_num );
tid_t __create_thread( void *entry, void *addr_space,
                       void *user_stack, tid_t exhandler );
int __map( void *virt, void *phys, size_t pages, int flags, void *addrSpace );
int __map_page_table( void *virt, void *phys, int flags, void *addrSpace );
int __grant( void *src_addr, void *dest_addr, void *addr_space, size_t pages );
int __grant_page_table( void *src_addr, void *dest_addr, void *addr_space, size_t pages );
void __exit( int status );
int __sleep( int msecs );
int __sleep_thread( int msecs, tid_t tid );
int __end_irq( int irqNum );
int __end_page_fault( tid_t tid );
void *__unmap( void *virt, void *addrSpace );
void *__unmap_page_table( void *virt, void *addrSpace );
int __raise( int signal, int arg );
int __set_sig_handler( void *handler );
int __set_io_perm( unsigned short start, unsigned short end, bool value, tid_t tid );
int __destroy_thread( tid_t tid );

#endif /* OS_SYSCALLS */
