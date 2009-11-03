#include <oslib.h>
#include <os/uthreads.h>

void _init_uthreads(void)
{
  int i;

  for(i=0; i < MAX_UTHREADS; i++)
  {
    __uthreads[i].status = DEAD_STATE;
    __uthreads[i].utid = NULL_UTID;
  }
}

static struct _UThread *get_free_uthread(void)
{
  struct _UThread *ptr;
  int i;
  
  for(i=0, ptr=__uthreads; i < MAX_UTHREADS; i++, ptr++)
  {
    if( ptr->utid == NULL_UTID )
    {
      ptr->utid = (ptr - __uthreads);
      return ptr;
    }
  }

  return NULL;
}

utid_t create_uthread( void *start )
{
  struct _UThread *new_uthread = get_free_uthread();
  dword *stack;
  
  if( new_uthread == NULL )
    return NULL_UTID;
  
  new_uthread->tid = -1; // XXX: This needs to be obtained by using 
                         // _get_thread_info(), I'm too lazy to use
                         //  the function right now
  new_uthread->status = PAUSED_STATE;
  
  new_uthread->stack = (void *)(CALC_STACK_TOP(new_uthread->utid) - PAGE_SIZE);
  
  allocatePages( new_uthread->stack, 1 );

  stack = (dword *)((unsigned int)new_uthread->stack + PAGE_SIZE);
  
  *--stack = (dword)start;
  *--stack = ((dword)new_uthread->stack + PAGE_SIZE);
  new_uthread->state.esp = new_uthread->state.ebp = (dword)stack;

  return new_uthread->utid;
}

int schedule( void )
{
  return -1;  
}

void yield_uthread(void)
{
  
  
}
