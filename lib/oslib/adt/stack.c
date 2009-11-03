//#include <stack.h>
#include <malloc.h>

struct Stack *createStack( void )
{
	struct Stack *newStack = ( struct Stack * ) malloc( sizeof( struct Stack ) );

	if ( newStack == NULL )
		return NULL;

	newStack->type = LIFO_STACK;
	newStack->head = newStack->tail = NULL;
	newStack->elements = 0;

	newStack->delete = ( void * ) deleteStack;
	newStack->getNumElements = ( void * ) getNumElements;
	newStack->setType = ( void * ) setStackType;
	newStack->push = ( void * ) pushStack;
	newStack->pop = ( void * ) popStack;

	return ( struct Stack * ) newStack;
}

void deleteStack( struct Stack *stack )
{
	if ( stack == NULL )
		return ;

	while ( stack->elements > 0 )
		free( popStack( stack ) );

	free( stack );
}

unsigned getNumElements( struct Stack *stack )
{
	if ( stack == NULL )
		return 0;
	else
		return stack->elements;
}

void setStackType( int type, struct Stack *stack )
{
	if ( stack == NULL )
		return ;

	switch ( type )
	{
			case LIFO_STACK:
			case FIFO_STACK:
			stack->type = type;
			default:
			break;
	}
}

bool pushStack( void *elem, struct Stack *stack )
{
	if ( stack == NULL )
		return false;

	struct StackNode *ptr;

	if ( stack->head == NULL )
	{
		stack->head = stack->tail =
		                  ( struct StackNode * ) malloc( sizeof( struct StackNode ) );

		if ( stack->head == NULL )
			return false;

		ptr = stack->head;

		ptr->next = ptr->prev = NULL;
		ptr->elem = elem;
		stack->elements++;

		return true;
	}

	ptr = ( struct StackNode * ) malloc( sizeof( struct StackNode ) );
	if ( ptr == NULL )
		return false;

	ptr->prev = stack->tail;
	ptr->next = NULL;
	ptr->elem = elem;
	stack->tail->next = ptr;
	stack->tail = ptr;
	stack->elements++;

	return true;
}

void *popStack( struct Stack *stack )
{
	if ( stack == NULL )
		return NULL;
	else if ( stack->head == NULL )
		return NULL;

	void *elem;
	struct StackNode *node;

	if ( stack->head == stack->tail )
	{
		elem = stack->head->elem;
		free( stack->head );
		stack->head = stack->tail = NULL;
		stack->elements = 0;
	}
	else
	{
		if ( stack->type == LIFO_STACK )
		{
			node = stack->tail;
			elem = node->elem;
			stack->tail = node->prev;
			free( node );
			stack->elements--;
		}
		else if ( stack->type == FIFO_STACK )
		{
			node = stack->head;
			elem = node->elem;
			stack->head = node->next;
			free( node );
			stack->elements--;
		}
	}
	return elem;
}
