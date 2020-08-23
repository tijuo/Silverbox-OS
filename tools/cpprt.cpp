#ifdef __cplusplus
#include <cpp.h>

struct Object objects[NUM_OBJECTS];

unsigned int numObjects = 0;
void *__dso_handle = 0;

extern "C"
{
  /* After a global object has been constructed, this function will be automatically
     called to set up a destructor. */

  int __cxa_atexit(void (*func)(void *), void *arg, void *dsoHandle)
  {

          if (numObjects >= NUM_OBJECTS)
            return -1;

          objects[numObjects].destructor = func;
          objects[numObjects].arg = arg;
          objects[numObjects].dsoHandle = dsoHandle;
          numObjects++;

          return 0;
  }

  /* This is called on program termination. The objects' destructors
     are called. */

  inline void __cxa_finalize(void *dsoHandle)
  {
          unsigned int i = numObjects;

          if( !dsoHandle )
          {
            while(i--)
            {
              --numObjects;

              if( objects[numObjects].destructor )
                objects[numObjects].destructor(objects[numObjects].arg);
            }
            return;
          }

          while(i--)
          {
            --numObjects;

            if( objects[numObjects].destructor == dsoHandle )
            {
              objects[numObjects].destructor(objects[numObjects].arg);
              objects[numObjects].destructor = 0;
            }
          }
  }

  // if GCC cannot call a virtual function(the virtual function is pure), then this will be called

  void __cxa_pure_virtual()
  {

  }

}

#endif /* __cplusplus */
