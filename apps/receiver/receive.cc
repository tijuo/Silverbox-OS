#include <os/mutex.h>
#include <os/syscalls.h>
#include <oslib.h>
#include <vector>

mutex_t print_lock=0;

void print(const char *);
void printN(const char *, int);
void printInt(int);
void printHex(int);
void printC(char);

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

void printN( char *str, int n )
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

int main(int argc, char *argv[])
{
  msg_t msg =
  {
    .subject = 0,
    .sender = ANY_SENDER,
    .data = {}
  };

  std::vector v = std::vector();

  while(1)
  {
    if(sys_receive(&msg, 1) == ESYS_OK)
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
    {
      print("Error. Unable to receive message");
      return 1;
    }
  }

  return 0;
}
