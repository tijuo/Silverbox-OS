#include <ctype.h>

int isalnum(int c)
{
  return isalpha(c) || isdigit(c);
}

int isalpha(int c)
{
  return isupper(c) || islower(c);
}

int iscntrl(int c)
{
  return ((c >= 0x00 && c <= 0x1F) || c == 0x7F);
}

int isdigit(int c)
{
  return (c >= '0' && c <= '9');
}

int islower(int c)
{
  return (c >= 'a' && c <= 'z');
}

int isspace(int c)
{
  return (c == 0x20 || (c >= 0x09 && c <= 0x0D));
}

int isupper(int c)
{
  return (c >= 'A' && c <= 'Z');
}

int isxdigit(int c)
{
  return (isdigit(c) || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f'));
}

int isprint(int c)
{
  return !iscntrl(c);
}

int ispunct(int c)
{
  return ((c >= 0x21 && c <= 0x2F) || (c >= 0x3a && c <= 0x40) ||
          (c >= 0x5b && c <= 0x60) || (c >= 0x7b && c <= 0x7e));
}

int isgraph(int c)
{
  return isprint(c) && c != ' ';
}

int tolower(int c)
{
  return (isupper(c) ? c - ('A' - 'a') : c);
}

int toupper(int c)
{
  return (islower(c) ? c + ('A' - 'a') : c);
}
