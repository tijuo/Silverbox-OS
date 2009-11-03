#ifdef __cplusplus
#include <cpp.h>

extern "C" 
{
  int __cxa_atexit(void (*f)(void *), void *p, void *d)
  {

          if (iObject >= 32) 
            return -1;
          object[iObject].f = f;
          object[iObject].p = p;
          object[iObject].d = d;
          ++iObject;

          return 0;
  }

  inline void __cxa_finalize(void *d)
  {
          unsigned int i = iObject;
          while(i--)
          {
            --iObject;
            object[iObject].f(object[iObject].p);
          }
  }


}

#endif /* __cplusplus */
