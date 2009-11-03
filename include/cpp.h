#ifndef CPP_HEADER_H
#define CPP_HEADER_H

#ifdef __cplusplus

extern "C"
{
        int __cxa_atexit(void (*f)(void *), void *p, void *d);
        void __cxa_finalize(void *d);
};

struct object
{
        void (*f)(void*);
        void *p;
        void *d;
} object[32];

unsigned int iObject = 0;

void *__dso_handle = 0;

#endif /* __cplusplus */

#endif /* CPP_HEADER_H */
