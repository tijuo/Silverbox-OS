#ifndef NAME_MAPPING_H
#define NAME_MAPPING_H

#include "InitServer.h"

class NameMapping
{
	public:
		NameMapping( char *name, mid_t mboxID ) : mid( mboxID )
		{
			strncpy( this->name, name, 50 );
			this->name[50] = '\0';
		}

		inline mid_t getMID() const
		{
			return mid;
		}

		inline char *getName()
		{
			return name;
		}

	private:
		char name[ 51 ];
		mid_t mid;
};

class NameNode
{
	public:
		friend class NameTable;
		NameNode( char *name, mid_t mboxID ) : mapping( name, mboxID )
		{ }

		NameMapping &getMapping()
		{
			return mapping;
		}

		int insert( char *name, mid_t mboxID );
		int remove ( char *name );
		int remove ( mid_t mboxID );
		mid_t find( char *name );
		char *find( mid_t mboxID );

		void *operator new( unsigned int size );
		void operator delete( void *node );

	private:
		NameMapping mapping;
		NameNode *next;
};

class NameTable
{
	public:
		NameTable() : nodes( NULL )
		{ }

		int insert( char *name, mid_t mboxID );

		int remove ( char *name );

		int remove ( mid_t mboxID );

		mid_t find( char *name )
		{
			return nodes->find( name );
		}
		char *find( mid_t mboxID )
		{
			return nodes->find( mboxID );
		}

	private:
		NameNode *nodes;
};

NameNode *freeNameNodes;
NameTable nameTable;

#endif /* NAME_MAPPING_H */
