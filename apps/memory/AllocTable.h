#ifndef ALLOC_TABLE_H
#define ALLOC_TABLE_H

#include "InitServer.h"

extern void *allocEnd;
extern unsigned availBytes;

typedef unsigned int memreg_addr_t;

class MemRegion
{
  public:
    MemRegion( memreg_addr_t addr, size_t len ) : 
               start( addr ), length( len ) { }

    int join( MemRegion &region );
    bool canJoin( MemRegion &region ) const;

    inline memreg_addr_t getStart() const { return start; }

    inline size_t getLength() const { return length; }
    bool isInRegion( memreg_addr_t addr ) const;

    inline void swap( MemRegion &region );
	
  private:
    memreg_addr_t start;
    size_t length;

    bool overlaps( MemRegion &region ) const;
    bool isAdjacent( MemRegion &region ) const;
};

class RegionListNode
{
  friend class RegionListTable;

  public:
    RegionListNode( memreg_addr_t addr, size_t len ) : 
       region( addr, len ), next( NULL ) { }
    ~RegionListNode() { }

    inline MemRegion &getMemRegion() 
       { return region; }

    RegionListNode *find( memreg_addr_t addr );
    void *operator new( unsigned int size );
    void operator delete( void *node );

  protected:
    MemRegion region;
    RegionListNode *next;
};

class RegionListHead : public RegionListNode
{
  friend class RegionListTable;

  public:
    RegionListHead( memreg_addr_t addr, size_t len, void *addrSpace ) : 
       RegionListNode::RegionListNode( addr, len ), addrSpace( addrSpace ),
       nextHead( NULL ) { }

    ~RegionListHead() { }

    inline void *getAddrSpace() const { return addrSpace; }

    void *operator new( unsigned int size );
    void operator delete( void *head );

    private:
      void *addrSpace;
      RegionListHead *nextHead;
};

class RegionListTable
{
  public:
    RegionListTable() : startHead( NULL ) { }
    ~RegionListTable() { }
		
    int insert( memreg_addr_t addr, size_t len, void *addrSpace );
    int remove ( memreg_addr_t addr, void *addrSpace );
    int removeList( void *addrSpace );
    MemRegion *find( memreg_addr_t addr, void *addrSpace ) const;

  private:
    RegionListNode *getPrev( RegionListHead *head, RegionListNode *node ) const;
    RegionListHead *getPrevHead( RegionListHead *head ) const;
    int deleteList( RegionListHead *head );
    RegionListHead *getASpaceList( void *addrSpace ) const;
    RegionListHead *startHead;
};

class MappingNode
{
  friend class MappingTable;

  public:
    MappingNode( tid_t newTID, MappingNode *mapHead ) : next(NULL), head(mapHead), tid(newTID) {}

    tid_t getTID() const { return tid; }
    void *operator new( unsigned int size );
    void operator delete( void *node );
    MappingNode *find( tid_t tid );

    MappingNode *getHead() const { return head; }

  protected:
    MappingNode *next;
    MappingNode *head;
    tid_t tid;
};

class MappingHead : public MappingNode
{
  friend class MappingTable;
  public:
    MappingHead( void *addrSpace, tid_t tid ) : MappingNode::MappingNode(tid, this), addrSpace(addrSpace),nextHead(NULL)
                                                { for(int i=0; i < 256; i++) pTableBitmap[i] = 0; }
    ~MappingHead() {}

    int add( tid_t tid );
    int remove( tid_t tid );

    void *operator new( unsigned int size );
    void operator delete( void *head );

    int setPTable( void *virt );
    int clearPTable( void *virt );
    bool pTableIsMapped( void *virt );

    void *getAddrSpace() const { return addrSpace; }

  private:
    void *addrSpace;
    MappingHead *nextHead;
    char pTableBitmap[256];
};

class MappingTable
{
  public:
    MappingTable() : startHead( NULL ) { }
    ~MappingTable() { }

    int map( tid_t tid, void *addrSpace );
    int unmap( tid_t tid, void *addrSpace );
    int removeAddrSpace( void *addrSpace );
    tid_t find( tid_t tid, void *addrSpace ) const;
/*
    int setPTable( tid_t tid, void *virt );
    int clearPTable( tid_t tid, void *virt );
    bool pTableIsMapped( tid_t tid, void *virt );
*/
    int setPTable( void *addrSpace, void *virt );
    int clearPTable( void *addrSpace, void *virt );
    bool pTableIsMapped( void *addrSpace, void *virt );

  private:
    MappingNode *getPrev( MappingHead *head, MappingNode *node ) const;
    MappingHead *getPrevHead( MappingHead *head ) const;
    int deleteList( MappingHead *head );
    MappingHead *getASpaceList( void *addrSpace ) const;
    MappingHead *startHead;
};

static MappingTable mappingTable;
static MappingNode *freeMapNodes;
static MappingHead *freeMapHeads;
static RegionListNode *freeNodes;
static RegionListHead *freeHeads;
static RegionListTable allocTable;

#endif /* ALLOC_TABLE_H */
