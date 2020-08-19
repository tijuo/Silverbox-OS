#ifdef __cplusplus
#include <cpp.h>

extern "C"
{
  /* After a global object has been constructed, this function will be automatically
     called to set up a destructor. */

  int __cxa_atexit(void (*f)(void *), void *p, void *d)
  {

          if (iObject >= 32)
            return -1;

          object[iObject].f = f;
          object[iObject].p = p;
          object[iObject].d = d;
          iObject++;

          return 0;
  }

  /* This is called on program termination. The objects' destructors
     are called. */

  inline void __cxa_finalize(void *d)
  {
          unsigned int i = iObject;

          if( !d )
          {
            while(i--)
            {
              --iObject;

              if( object[iObject].f )
                object[iObject].f(object[iObject].p);
            }
            return;
          }

          while(i--)
          {
            --iObject;

            if( object[iObject].f == d )
            {
              object[iObject].f(object[iObject].p);
              object[iObject].f = 0;
            }
          }
  }

  // if GCC cannot call a virtual function(the virtual function is pure), then this will be called

  void __cxa_pure_virtual()
  {

  }

}

#endif /* __cplusplus */
