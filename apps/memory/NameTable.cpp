#include "NameTable.h"

int NameNode::insert( char *name, mid_t mboxID )
{
    if( strncmp( mapping.getName(), name, 50 ) == 0 )
      return -1;
    else if( mapping.getID() == mboxID )
      return -1;
	else if ( this->next == NULL )
	{
		this->next = new NameNode( name, mboxID );
		             return 0;
	}
	else
		return this->next->insert( name, mboxID );
}

int NameNode::remove ( char *name )
{
	NameNode * node = next;

	if ( node == NULL )
		return -1;

	if ( strncmp( node->mapping.getName(), name, 50 ) == 0 )
	{
		this->next = node->next;
		delete node;
		return 0;
	}
	else
		return node->remove ( name )
		       ;
}

int NameNode::remove ( mid_t mboxID )
{
	NameNode * node = next;

	if ( node == NULL )
		return -1;

	if ( mboxID == mapping.getID() )
	{
		this->next = node->next;
		delete node;
		return 0;
	}
	else
		return node->remove ( mboxID )
		       ;
}

mid_t NameNode::find( char *name )
{
	if ( strncmp( mapping.getName(), name, 50 ) == 0 )
		return mapping.getID();
	else if ( this->next == NULL )
		return -1;
	else
		return this->next->find( name );
}

char *NameNode::find( mid_t mboxID )
{
	if ( mboxID == mapping.getID() )
		return mapping.getName();
	else if ( this->next == NULL )
		return NULL;
	else
		return this->next->find( mboxID );
}

void *NameNode::operator new( unsigned int size )
{
	void *ptr;

	if ( freeNameNodes != NULL )
	{
		ptr = static_cast<void *>( freeNameNodes );
		freeNameNodes = freeNameNodes->next;
		return ptr;
	}

	else if ( size > availBytes )
	{
		// print("name");
		mapPage( );
		availBytes += 4096;
	}

	availBytes -= size;
	ptr = allocEnd;
	allocEnd = reinterpret_cast<void *>( reinterpret_cast<size_t>( allocEnd ) + size );

	return ptr;
}

void NameNode::operator delete( void *node )
{
	NameNode * ptr = freeNameNodes;

	if ( node == NULL )
		return ;

	if ( freeNameNodes == NULL )
	{
		freeNameNodes = static_cast<NameNode *>( node );
		freeNameNodes->next = NULL;
		return ;
	}

	while ( ptr->next != NULL )
		ptr = ptr->next;

	ptr->next = static_cast<NameNode *>( node );
	ptr->next->next = NULL;
}

int NameTable::insert( char *name, mid_t mboxID )
{
	if ( nodes == NULL )
	{
		nodes = new NameNode( name, mboxID );
		return 0;
	}
	else
		return nodes->insert( name, mboxID );
}

int NameTable::remove ( char *name )
{
	NameNode * node;

	if ( nodes == NULL )
		return -1;
	else if ( strncmp( nodes->getMapping().getName(), name, 50 ) == 0 )
	{
		node = nodes;
		nodes = nodes->next;
		delete node;
		return 0;
	}
	else
		return nodes->remove ( name )
		       ;
}

int NameTable::remove ( mid_t mboxID )
{
	NameNode * node;

	if ( nodes == NULL )
		return -1;
	else if ( nodes->getMapping().getID() == mboxID )
	{
		node = nodes;
		nodes = nodes->next;
		delete node;
		return 0;
	}
	else
		return nodes->remove ( mboxID )
		       ;
}
