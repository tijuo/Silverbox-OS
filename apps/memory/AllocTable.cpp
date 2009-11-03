#include "AllocTable.h"

/* XXX What if the region wrapped around ? */

bool MemRegion::canJoin( MemRegion &region ) const
{
  if ( overlaps( region ) || isAdjacent( region ) )
    return true;
  else
    return false;
}

int MemRegion::join( MemRegion &region )
{
  memreg_addr_t newStart;
  size_t newLength;

  if ( !canJoin( region ) )
    return -1;

  if ( this->start <= region.start )
  {
    newStart = this->start;

    if ( this->start + this->length >= region.start + region.length )
      newLength = this->length;
    else
      newLength = ( region.start + region.length ) - newStart;
  }
  else
  {
    newStart = region.start;

    if ( region.start  + region.length >= this->start + this->length )
      newLength = region.length;
    else
      newLength = ( this->start + this->length ) - newStart;
  }

  this->start = newStart;
  this->length = newLength;

  return 0;
}

bool MemRegion::isInRegion( memreg_addr_t addr ) const
{
  if ( addr >= this->start )
  {
    if ( addr  <  this->start + this->length )
      return true;
  }

  return false;
}

bool MemRegion::overlaps( MemRegion &region ) const
{
  if ( isInRegion( region.start ) || region.isInRegion( this->start ) )
    return true;
  else
    return false;
}

bool MemRegion::isAdjacent( MemRegion &region ) const
{
  if ( this->start + this->length == region.start )
    return true;
  else if ( region.start + region.length == this->start )
    return true;
  else
    return false;
}

void MemRegion::swap( MemRegion &region )
{
  memreg_addr_t tStart;
  size_t tLen;

  tStart = region.start;
  tLen = region.length;

  region.start = this->start;
  region.length = this->length;

  this->start = tStart;
  this->length = tLen;
}

RegionListNode *RegionListNode::find( memreg_addr_t addr )
{
  RegionListNode * node = this;

  while ( node != NULL && !node->region.isInRegion( addr ) )
    node = node->next;

  if ( node == NULL )
    return NULL;
  else
    return node;
}

void *RegionListNode::operator new( unsigned int size )
{
  void *ptr;

  if ( freeNodes != NULL )
  {
    ptr = static_cast<void *>( freeNodes );
    freeNodes = freeNodes->next;
    return ptr;
  }

  else if ( size > availBytes )
  {
    mapPage( );
    availBytes += 4096;
  }

  availBytes -= size;
  ptr = allocEnd;
  allocEnd = reinterpret_cast<void *>( reinterpret_cast<size_t>( allocEnd ) + size );

  return ptr;
}

void RegionListNode::operator delete( void *node )
{
  RegionListNode * ptr = freeNodes;

  if ( node == NULL )
    return ;

  if ( freeNodes == NULL )
  {
    freeNodes = static_cast<RegionListNode *>( node );
    freeNodes->next = NULL;
    return ;
  }

  while ( ptr->next != NULL )
    ptr = ptr->next;

  ptr->next = static_cast<RegionListNode *>( node );
  ptr->next->next = NULL;
}

void *RegionListHead::operator new( unsigned int size )
{
  void *ptr;

  if ( freeHeads != NULL )
  {
    ptr = static_cast<void *>( freeHeads );
    freeHeads = freeHeads->nextHead;
    return ptr;
  }

  else if ( size > availBytes )
  {
    // print("head");
    mapPage( );
    availBytes += 4096;
  }

  availBytes -= size;
  ptr = allocEnd;
  allocEnd = reinterpret_cast<void *>( reinterpret_cast<size_t>( allocEnd ) + size );

  return ptr;
}

void RegionListHead::operator delete( void *head )
{
  RegionListHead * ptr = freeHeads;

  if ( head == NULL )
    return ;

  if ( freeHeads == NULL )
  {
    freeHeads = static_cast<RegionListHead *>( head );
    freeHeads->nextHead = NULL;
    return ;
  }

  while ( ptr->nextHead != NULL )
    ptr = ptr->nextHead;

  ptr->nextHead = static_cast<RegionListHead *>( head );
  ptr->nextHead->nextHead = NULL;
}

int RegionListTable::deleteList( RegionListHead *head )
{
  RegionListNode * ptr, * nextPtr;

  if ( head == NULL )
    return -1;

  ptr = head;
  nextPtr = ptr->next;
  delete head;

  while ( nextPtr != NULL )
  {
    nextPtr = ptr->next;
    delete ptr;
    ptr = nextPtr;
  }

  return 0;
}

int RegionListTable::remove ( memreg_addr_t addr, void *addrSpace )
{
  RegionListHead * ptr, *head = getASpaceList( addrSpace );
  RegionListNode *node, *ptr2;

  if ( head == NULL )
    return -1;

  node = head->find( addr );

  if ( node == NULL )
    return -1;

  if ( node == head )
  {
    if ( node->next == NULL )
    {
      removeList( addrSpace );
    }
    else
    {
      for ( ptr = startHead; ptr->nextHead != head; ptr = ptr->nextHead )
        ;

      ptr->nextHead = head->nextHead;
      delete head;
    }
    return 0;
  }

  ptr2 = getPrev( head, node );

  if ( ptr2 == NULL )
    return -1;
  else
    ptr2->next = node->next;

  delete node;

  return 0;
}

int RegionListTable::removeList( void *addrSpace )
{
  RegionListHead * ptr, *head = getASpaceList( addrSpace );

  if ( head == NULL )
    return -1;

  if ( head == startHead )
  {
    startHead = head->nextHead;
    delete startHead;
    return 0;
  }

  for ( ptr = startHead; ptr->nextHead != head; ptr = ptr->nextHead )
    ;

  ptr->nextHead = head->nextHead;

  return deleteList( head );
}

RegionListHead *RegionListTable::getASpaceList( void *addrSpace ) const
{
  RegionListHead * ptr = startHead;

  while ( ptr != NULL && ptr->getAddrSpace() != addrSpace )
    ptr = ptr->nextHead;

  return ptr;
}

int RegionListTable::insert( memreg_addr_t addr, size_t len, void *addrSpace )
{
  RegionListHead * ptr, *head;
  RegionListNode *node, *newNode;

  if ( len == 0 )
    return -1;

  if ( startHead == NULL )
  {
    startHead = new RegionListHead( addr, len, addrSpace );
    startHead->nextHead = NULL;

    return 0;
  }
  else
  {
    for ( ptr = startHead; ptr != NULL && ptr->getAddrSpace() != addrSpace;
            ptr = ptr->nextHead )
    {
      if ( ptr == startHead && addrSpace < ptr->getAddrSpace() )
      {
        head = new RegionListHead( addr, len, addrSpace );
        head->nextHead = startHead;
        startHead = head;

        return 0;
      }

      if ( addrSpace > ptr->getAddrSpace() )
      {
        if ( ptr->nextHead == NULL || addrSpace < ptr->nextHead->getAddrSpace() )
        {
          head = new RegionListHead( addr, len, addrSpace );
          head->nextHead = ptr->nextHead;
          ptr->nextHead = head;
          return 0;
        }
      }
    }

    node = head = ptr;
    newNode = new RegionListNode( addr, len );

    do
    {
      if ( node->getMemRegion().canJoin( newNode->getMemRegion() ) )
      {
        node->getMemRegion().join( newNode->getMemRegion() );
        delete newNode;

        while ( node->next != NULL && node->getMemRegion().join( node->next->getMemRegion() ) != -1 )
        {
          newNode = node->next;
          node->next = newNode->next;
          delete newNode;
        }
        return 0;
      }
      else if ( newNode->getMemRegion().getStart() < node->getMemRegion().getStart() )
      {
        if ( node == head )
        {
          node->getMemRegion().swap( newNode->getMemRegion() );
          newNode->next = node->next;
          node->next = newNode;
        }
        else
        {
          RegionListNode *ptr2;

          ptr2 = getPrev( head, node );

          if ( ptr2 == NULL )
          {
            delete newNode;
            return -1;
          }
          ptr2->next = newNode;
          newNode->next = node;
          return 0;
        }
        return 0;

      }
      else if ( node->next == NULL )
      {
        node->next = newNode;
        return 0;
      }
      node = node->next;

    }
    while ( node != NULL );
  }
  delete newNode;
  return -1;
}

MemRegion *RegionListTable::find( memreg_addr_t addr, void *addrSpace ) const
{
  RegionListHead * head = getASpaceList( addrSpace );

  if ( head == NULL )
    return NULL;

  return &head->find( addr )->getMemRegion();
}

RegionListNode *RegionListTable::getPrev( RegionListHead *head, RegionListNode *node ) const
{
  RegionListNode * ptr;

  if ( head == NULL || node == NULL || head == node )
    return NULL;

  for ( ptr = head; ptr->next != node && ptr->next != NULL; ptr = ptr->next )
    ;
  if ( ptr->next == NULL )
    return NULL;
  else
    return ptr;
}

void *MappingHead::operator new( unsigned int size )
{
  void *ptr;

  if ( freeMapHeads != NULL )
  {
    ptr = static_cast<void *>( freeMapHeads );
    freeMapHeads = freeMapHeads->nextHead;
    return ptr;
  }

  else if ( size > availBytes )
  {
    mapPage( );
    availBytes += 4096;
  }

  availBytes -= size;
  ptr = allocEnd;
  allocEnd = reinterpret_cast<void *>( reinterpret_cast<size_t>( allocEnd ) + size );

  return ptr;
}

void MappingHead::operator delete( void *head )
{
  MappingHead * ptr = freeMapHeads;

  if ( head == NULL )
    return ;

  if ( freeMapHeads == NULL )
  {
    freeMapHeads = static_cast<MappingHead *>( head );
    freeMapHeads->nextHead = NULL;
    return ;
  }

  while ( ptr->nextHead != NULL )
    ptr = ptr->nextHead;

  ptr->nextHead = static_cast<MappingHead *>( head );
  ptr->nextHead->nextHead = NULL;
}

void *MappingNode::operator new( unsigned int size )
{
  void *ptr;

  if ( freeMapNodes != NULL )
  {
    ptr = static_cast<void *>( freeMapNodes );
    freeMapNodes = freeMapNodes->next;
    return ptr;
  }

  else if ( size > availBytes )
  {
    mapPage( );
    availBytes += 4096;
  }

  availBytes -= size;
  ptr = allocEnd;
  allocEnd = reinterpret_cast<void *>( reinterpret_cast<size_t>( allocEnd ) + size );

  return ptr;
}

void MappingNode::operator delete( void *node )
{
  MappingNode * ptr = freeMapNodes;

  if ( node == NULL )
    return ;

  if ( freeMapNodes == NULL )
  {
    freeMapNodes = static_cast<MappingNode *>( node );
    freeMapNodes->next = NULL;
    return ;
  }

  while ( ptr->next != NULL )
    ptr = ptr->next;

  ptr->next = static_cast<MappingNode *>( node );
  ptr->next->next = NULL;
}

MappingNode *MappingNode::find( tid_t tid )
{
  MappingNode * node = this;

  while ( node != NULL && node->tid != tid )
    node = node->next;

  if ( node == NULL )
    return NULL;
  else
    return node;
}

int MappingHead::setPTable( void *virt )
{
  unsigned int tableNum = (unsigned)virt >> 22;

  if( tableNum >= 1024 )
    return -1;

  pTableBitmap[tableNum / 8] |= (1 << (tableNum % 8));

  return 0;
}

int MappingHead::clearPTable( void *virt )
{
  unsigned int tableNum = (unsigned)virt >> 22;

  if( tableNum >= 1024 )
    return -1;

  pTableBitmap[tableNum / 8] &= ~(1 << (tableNum % 8));

  return 0;
}

bool MappingHead::pTableIsMapped( void *virt )
{
  unsigned tableNum = (unsigned)virt >> 22;

  if( tableNum >= 1024 )
    return -1;

  return (((pTableBitmap[tableNum / 8] & (1 << (tableNum % 8))) == 0) ? false : true);
}

/*
int MappingTable::setPTable( tid_t tid, void *virt )
{
  find
}

int MappingTable::clearPTable( tid_t tid, void *virt )
{

}

bool MappingTable::pTableIsMapped( tid_t tid, void *virt )
{

}
*/
int MappingTable::setPTable( void *addrSpace, void *virt )
{
  MappingHead *head = getASpaceList( addrSpace );

  if( head == NULL )
    return -1;

  return head->setPTable( virt );
}

int MappingTable::clearPTable( void *addrSpace, void *virt )
{
  MappingHead *head = getASpaceList( addrSpace );

  if( head == NULL )
    return -1;

  return head->clearPTable( virt );
}

bool MappingTable::pTableIsMapped( void *addrSpace, void *virt )
{
  MappingHead *head = getASpaceList( addrSpace );

  if( head == NULL )
    return -1;

  return head->pTableIsMapped( virt );
}


int MappingTable::deleteList( MappingHead *head )
{
  MappingNode * ptr, * nextPtr;

  if ( head == NULL )
    return -1;

  ptr = head;
  nextPtr = ptr->next;
  delete head;

  while ( nextPtr != NULL )
  {
    nextPtr = ptr->next;
    delete ptr;
    ptr = nextPtr;
  }

  return 0;
}

int MappingTable::unmap( tid_t tid, void *addrSpace )
{
  MappingHead * ptr, *head = getASpaceList( addrSpace );
  MappingNode *node, *ptr2;

  if ( head == NULL )
    return -1;

  node = head->find( tid );

  if ( node == NULL )
    return -1;

  if ( node == head )
  {
    if ( node->next == NULL )
      removeAddrSpace( addrSpace );
    else
    {
      for ( ptr = startHead; ptr->nextHead != head; ptr = ptr->nextHead )
        ;

      ptr->nextHead = head->nextHead;
      delete head;
    }
    return 0;
  }

  ptr2 = getPrev( head, node );

  if ( ptr2 == NULL )
    return -1;
  else
    ptr2->next = node->next;

  delete node;

  return 0;
}

int MappingTable::removeAddrSpace( void *addrSpace )
{
  MappingHead * ptr, *head = getASpaceList( addrSpace );

  if ( head == NULL )
    return -1;

  if ( head == startHead )
  {
    startHead = head->nextHead;
    delete startHead;
    return 0;
  }

  for ( ptr = startHead; ptr->nextHead != head; ptr = ptr->nextHead )
    ;

  ptr->nextHead = head->nextHead;

  return deleteList( head );
}

MappingHead *MappingTable::getASpaceList( void *addrSpace ) const
{
  MappingHead * ptr = startHead;

  while ( ptr != NULL && ptr->getAddrSpace() != addrSpace )
    ptr = ptr->nextHead;

  return ptr;
}

int MappingTable::map( tid_t tid, void *addrSpace )
{
  MappingHead * ptr, *head;
  MappingNode *newNode;

  if ( tid == -1 )
    return -1;

  if ( startHead == NULL )
  {
    startHead = new MappingHead( addrSpace, tid );
    startHead->nextHead = NULL;
  }
  else
  {
    for ( ptr = startHead; ptr != NULL && ptr->getAddrSpace() != addrSpace;
          ptr = ptr->nextHead );
    if( ptr == NULL )
    {
      head = new MappingHead( addrSpace, tid );
      head->nextHead = startHead;
      startHead = head;
    }
    else
    {
      head = ptr;
      newNode = new MappingNode( tid, head );
      newNode->next = head->next;
      head->next = newNode;
    }
  }
  return 0;
}

tid_t MappingTable::find( tid_t tid, void *addrSpace ) const
{
  MappingHead *head = getASpaceList( addrSpace );

  if ( head == NULL )
    return -1;

  return head->find( tid )->getTID();
}

MappingNode *MappingTable::getPrev( MappingHead *head, MappingNode *node ) const
{
  MappingNode * ptr;

  if ( head == NULL || node == NULL || head == node )
    return NULL;

  for ( ptr = head; ptr->next != node && ptr->next != NULL; ptr = ptr->next )
    ;
  if ( ptr->next == NULL )
    return NULL;
  else
    return ptr;
}
