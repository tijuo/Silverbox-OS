#ifndef CPP_HEADER_H
#define CPP_HEADER_H

#ifdef __cplusplus

#define NUM_OBJECTS		128

extern "C"
{
  int __cxa_atexit(void (*func)(void *), void *arg, void *dsoHandle);
  void __cxa_finalize(void *dsoHandle);
};

struct Object
{
  void (*destructor)(void*);
  void *arg;
  void *dsoHandle;
};

extern unsigned int numObjects;
extern void *__dso_handle;

#endif /* __cplusplus */

#endif /* CPP_HEADER_H */
