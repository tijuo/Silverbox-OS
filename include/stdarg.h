#ifndef STDARG_H
#define STDARG_H

#define STACK_WIDTH		4

//typedef char * va_list;

#define va_list char *

#define va_round( type ) ( (sizeof(type) + sizeof(void *) - 1) & ~(sizeof(void *) - 1) )

#define va_arg( ap, type ) ( (ap) += (va_round(type)), ( *(type*) ( (ap) - (va_round(type)) ) ) )
#define va_copy( dest, src ) ( (dest) = (src), (void)0 )
#define va_end( ap ) ( (ap) = (void *)0, (void)0 )
#define va_start( ap, parmN ) ( (ap) = (char *) &parmN + ( va_round(parmN) ), (void)0 )

#endif /* STDARG_H */
