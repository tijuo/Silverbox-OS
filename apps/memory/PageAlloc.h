#ifndef PAGE_ALLOCATOR
#define PAGE_ALLOCATOR

#define PAGE_SIZE 4096

class PageAllocator
{
  public:
    PageAllocator( unsigned int *new_stack, int numPages )
    { 
      if( new_stack != NULL )
        this->stack = this->top = stack;
      if(numPages > 0 )
        this->numPages = numPages;
    }
    ~PageAllocator() { }

    void setStack(unsigned int *new_stack)
    {
      if( new_stack != NULL )
      {
        this->stack = this->top = new_stack;
        this->numPages = 0;
      }
    }

    void addPage( unsigned int page )
    {
      if( this->top != NULL )
      {
        *this->top++ = page * PAGE_SIZE;

        numPages++;
      }
    }

    void *alloc( void )
    {
      if( this->top == this->stack)
        return NULL;

      return (void *)(*--top);
    }

    void free( void *address )
    {
      if( (unsigned int)(top - stack) >= numPages )
        return;

      *top++ = (unsigned int )address;
    }

    unsigned int *getStack() { return stack; }
    unsigned int *getTop() { return top; }
  private:
    unsigned int *stack, *top;
    unsigned int numPages;
};

PageAllocator *convAllocator, *dmaAllocator, *pageAllocator;

#endif
