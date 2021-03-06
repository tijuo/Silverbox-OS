#include <os/mutex.h>
#include <os/syscalls.h>
#include <oslib.h>

extern void print(const char *);
extern void printN(const char *, int);
extern void printInt(int);
extern void printHex(int);
extern void printC(char);

/*
void printC( char c )
{
  char volatile *vidmem = (char *)(0xB8000 + 160 * 4);
  static int i=0;

  while(mutex_lock(&print_lock))
    sys_wait(0);

  if( c == '\n' )
    i += 160 - (i % 160);
  else
  {
    vidmem[i++] = c;
    vidmem[i++] = 7;
  }

  mutex_unlock(&print_lock);
}

void printN( const char *str, int n )
{
  for( int i=0; i < n; i++, str++ )
    printC(*str);
}

void print( const char *str )
{
  for(; *str; str++ )
    printC(*str);
}

void printInt( int n )
{
  print(toIntString(n));
}

void printHex( int n )
{
  print(toHexString(n));
}
*/

int main(int argc, char *argv[])
{
  msg_t msg =
  {
    .sender = ANY_SENDER,
  };

  while(1)
  {
    if(sys_receive(&msg, 0) == ESYS_OK)
    {
      print("receiver: Received a message with subject "), printInt(msg.subject), print(" from "), printInt(msg.sender), print("\n");

      if(msg.sender == 1026)
      {
        for(int i=0; i < 5; i++)
        {
          print("0x");
          printHex(msg.data.i32[i]);
          print(" ");
        }
        print("\n");
        break;
      }
      else
        msg.sender = ANY_SENDER;
    }
    else
      sys_wait(300);
/*
    else
    {
      print("Error. Unable to receive message");
      return 1;
    }
*/
  }

  return 0;
}
