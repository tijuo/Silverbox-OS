#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <stdint.h>
#include <stddef.h>

#define STATE_NONE		0
#define STATE_FORMAT		1
#define STATE_FLAGS		2
#define STATE_WIDTH		3
#define STATE_DOT		4
#define STATE_PRECISION		5
#define STATE_LENGTH		6
#define STATE_LENGTH_H		7
#define STATE_LENGTH_L		8
#define STATE_TYPE		9

#define	FLAGS_FLAG		1
#define PRECISION_FLAG		2
#define LENGTH_FLAG		4
#define WIDTH_FLAG		8
#define LEFT_ALIGN_FLAG		16
#define ALT_FLAG		32
#define THOUSANDS_SEP_FLAG	64
#define WIDTH_ARG_FLAG		128
#define PRECISION_ARG_FLAG	256
#define STRING_FLAG		512
#define ITOA_FLAG		1024

#define LENGTH_CHAR		1
#define LENGTH_SHORT		2
#define LENGTH_LONG		3
#define LENGTH_LONG_LONG	4
#define LENGTH_DOUBLE_LONG	5
#define LENGTH_SIZE_T		6
#define LENGTH_INTMAX_T		7
#define LENGTH_PTRDIFF_T	8

extern char *itoa(int c, char *buf, int base);

static int isFlagChar(int c);
static int isLengthChar(int c);
static int isTypeChar(int c);
static int min(int a, int b);
static int max(int a, int b);

int do_printf(void *s, const char *format, va_list ap, int isStream);

static int min(int a, int b)
{
  return a < b ? a : b;
}

static int max(int a, int b)
{
  return a > b ? a : b;
}

static int add_char(int c, void **s, int isStream)
{
  if(!c)
    return 0;

  if( isStream )
  {
    FILE *stream = *(FILE **)s;
    return fputc( c, stream );
  }
  else
  {
    char **_s = (char **)s;

    **_s = c;
    (*_s)++;
  }
  return 0;
}

static int isFlagChar(int c)
{
  switch(c)
  {
    case '-':
    case '+':
    case '0':
    case ' ':
    case '#':
    case '\'':
      return 1;
    default:
      return 0;
  }
}

static int isLengthChar(int c)
{
  switch(c)
  {
    case 'h':
    case 'l':
    case 'L':
    case 'z':
    case 'j':
    case 't':
      return 1;
    default:
      return 0;
  }
}

static int isTypeChar(int c)
{
  switch(c)
  {
    case '%':
    case 'd':
    case 'u':
    case 'i':
    case 'f':
    case 'F':
    case 'e':
    case 'E':
    case 'g':
    case 'G':
    case 'x':
    case 'X':
    case 'O':
    case 's':
    case 'c':
    case 'p':
    case 'a':
    case 'A':
    case 'n':
      return 1;
    default:
      return 0;
  }
}

int do_printf(void *s, const char *format, va_list aplist, int isStream)
{
  int flags=0, length=0, base=0;
  int width=0, precision=0;
  int state=STATE_NONE;
  const char *ptr=format;
  char positiveChar='\0', spaceChar=' ';
  int writeCount=0;
  char buf[65];
  char *prefix="";

  if(!s)
    return -1;

  while(*ptr)
  {
    int c = *ptr;

    // First read formatted type substring (with associated flags, width, etc.)

    switch(state)
    {
      case STATE_NONE:
        if(c == '%')
        {
          state = STATE_FORMAT;
          flags = 0;
          width = 0;
          precision = 0;
          length = 0;
          base = 0;
          spaceChar = ' ';
          positiveChar = '\0';
          prefix = "";
          buf[0] = '\0';
        }
        else
        {
          add_char(c, &s, isStream);
          writeCount++;
        }
        ptr++;
        break;
      case STATE_FLAGS:
        if(isFlagChar(c))
        {
          switch(c)
          {
            case '-':
              flags |= LEFT_ALIGN_FLAG;
              break;
            case '+':
              positiveChar = '+';
              break;
            case '0':
              spaceChar = '0';
              break;
            case ' ':
              if(positiveChar != '+')
                positiveChar = ' ';
              break;
            case '#':
              flags |= ALT_FLAG;
              break;
            case '\'':
              flags |= THOUSANDS_SEP_FLAG;
              break;
            default:
              break;
          }
         ptr++;
        }
        else
          goto check_width;
        break;
      case STATE_WIDTH:
        if(c == '*')
        {
          if(!(flags & WIDTH_ARG_FLAG) && !(flags & WIDTH_FLAG))
          {
            flags |= WIDTH_ARG_FLAG;
            ptr++;
          }
          else
            goto invalid_format;
        }
        else if(isdigit(c))
        {
          if(flags & WIDTH_ARG_FLAG)
            goto invalid_format;

          width = width * 10 + (c - '0');
          flags |= WIDTH_FLAG;
          ptr++;
        }
        else
          goto check_dot;
        break;
      case STATE_DOT:
        if(isdigit(c) || c == '*')
          state = STATE_PRECISION;
        else
          goto invalid_format;
        break;
      case STATE_PRECISION:
        if(isdigit(c))
        {
          if(flags & PRECISION_ARG_FLAG)
            goto invalid_format;
          else
          {
            precision = precision * 10 + (c - '0');
            flags |= PRECISION_FLAG;
            ptr++;
          }
        }
        else if(c == '*')
        {
          if(!(flags & PRECISION_FLAG) && !(flags & PRECISION_ARG_FLAG))
          {
            flags |= PRECISION_ARG_FLAG;
            ptr++;
          }
          else
            goto invalid_format;
        }
        else
          goto check_length;
        break;
      case STATE_LENGTH:
        switch(c)
        {
          case 'h':
            state = STATE_LENGTH_H;
            break;
          case 'l':
            state = STATE_LENGTH_L;
            break;
          case 'L':
            length = LENGTH_LONG_LONG;
            state = STATE_TYPE;
            break;
          case 'z':
            length = LENGTH_SIZE_T;
            state = STATE_TYPE;
            break;
          case 'j':
            length = LENGTH_INTMAX_T;
            state = STATE_TYPE;
            break;
          case 't':
            length = LENGTH_PTRDIFF_T;
            state = STATE_TYPE;
            break;
          default:
            goto invalid_format;
        }

        flags |= LENGTH_FLAG;
        ptr++;
        break;
      case STATE_LENGTH_H:
        if(c == 'h')
        {
          length = LENGTH_CHAR;
          state = STATE_TYPE;
          ptr++;
        }
        else
        {
          length = LENGTH_SHORT;
          goto check_type;
        }
        break;
      case STATE_LENGTH_L:
        if(c == 'l')
        {
          length = LENGTH_LONG_LONG;
          state = STATE_TYPE;
          ptr++;
        }
        else
        {
          length = LENGTH_LONG;
          goto check_type;
        }
        break;
      case STATE_TYPE:
        if(isTypeChar(c))
        {
          if(flags & WIDTH_ARG_FLAG)
          {
            width = va_arg(aplist, int);
            flags |= WIDTH_FLAG;
          }

          if(flags & PRECISION_ARG_FLAG)
          {
            precision = va_arg(aplist, int);
            flags |= PRECISION_FLAG;
          }

          switch(c)
          {
            case '%':
              buf[0] = '%';
              buf[1] = '\0';
              add_char('%', &s, isStream);
              flags &= ~(WIDTH_ARG_FLAG | WIDTH_FLAG | PRECISION_FLAG | PRECISION_ARG_FLAG | ALT_FLAG | THOUSANDS_SEP_FLAG | LENGTH_FLAG);
              writeCount++;
              break;
            case 'd':
            case 'i':
              flags |= ITOA_FLAG;
              base = 10;
              break;
            case 'u':
              flags |= ITOA_FLAG;
              base = -10;
              break;
            case 'f':
              break;
            case 'F':
              break;
            case 'e':
              break;
            case 'E':
              break;
            case 'g':
              break;
            case 'G':
              break;
            case 'p':
              prefix = "0x";
              flags |= ITOA_FLAG;
              base = 16;
              break;
            case 'x':
              if(flags & ALT_FLAG)
                prefix = "0x";
              flags |= ITOA_FLAG;
              base = 16;
              break;
            case 'X':
              if(flags & ALT_FLAG)
                prefix = "0X";
              flags |= ITOA_FLAG;
              base = 16;
              break;
            case 'o':
              if(flags & ALT_FLAG)
                prefix = "0";
              flags |= ITOA_FLAG;
              base = 8;
              break;
            case 'b':
              if(flags & ALT_FLAG)
                prefix = "0b";
              flags |= ITOA_FLAG;
              base = 2;
              break;
            case 's':
              flags &= ~(ALT_FLAG | LENGTH_FLAG);
              flags |= STRING_FLAG;
              break;
            case 'c':
              buf[0] = (char)va_arg(aplist, int);
              buf[1] = '\0';
              flags &= ~(LENGTH_FLAG | THOUSANDS_SEP_FLAG | PRECISION_FLAG | ALT_FLAG | PRECISION_ARG_FLAG);
              break;
            case 'a':
              break;
            case 'A':
              break;
            case 'n':
            {
              if(!(flags & LENGTH_FLAG))
                *(int *)va_arg(aplist, int *) = writeCount;
              else
              {
                switch(length)
                {
                  case LENGTH_CHAR:
                    *(signed char *)va_arg(aplist, signed char *) = writeCount;
                    break;
                  case LENGTH_SHORT:
                    *(short int *)va_arg(aplist, short int *) = writeCount;
                    break;
                  case LENGTH_LONG:
                    *(long int *)va_arg(aplist, long int *) = writeCount;
                    break;
                  case LENGTH_LONG_LONG:
                    *(long long int *)va_arg(aplist, long long int *) = writeCount;
                    break;
                  case LENGTH_INTMAX_T:
                    *(intmax_t *)va_arg(aplist, intmax_t *) = writeCount;
                    break;
                  case LENGTH_PTRDIFF_T:
                    *(ptrdiff_t *)va_arg(aplist, ptrdiff_t *) = writeCount;
                    break;
                  case LENGTH_SIZE_T:
                    *(size_t *)va_arg(aplist, size_t *) = writeCount;
                    break;
                  default:
                    *(int *)va_arg(aplist, int *) = writeCount;
                    break;
                }
              }
              break;
            }
            default:
              goto invalid_format;
          }

          if(flags & ITOA_FLAG)
          {
            if(!(flags & LENGTH_FLAG))
              itoa(va_arg(aplist, int), buf, base);
            else
            {
              switch(length)
              {
                case LENGTH_CHAR:
                  itoa((va_arg(aplist, int) & 0xFF), buf, base);
                  break;
                case LENGTH_SHORT:
                  itoa((va_arg(aplist, int) & 0xFFFF), buf, base);
                  break;
                case LENGTH_LONG:
                  //ltoa(va_arg(aplist, long int), buf, base);
                  break;
                case LENGTH_LONG_LONG:
                  //lltoa(va_arg(aplist, long long int), buf, base);
                  break;
                case LENGTH_INTMAX_T:
                  //intmaxtoa(va_arg(aplist, intmax_t), buf, base);
                  break;
                case LENGTH_PTRDIFF_T:
                  //ptrdifftoa(va_arg(aplist, ptrdiff_t), buf, base);
                  break;
                case LENGTH_SIZE_T:
                  //sizetoa(va_arg(aplist, size_t), buf, base);
                  break;
                default:
                  itoa(va_arg(aplist, int), buf, base);
                  break;
              }
            }
          }

          if(flags & STRING_FLAG)
          {
            char *str = (char *)va_arg(aplist, char *);
            size_t strLen = strlen(str);
            size_t precisionLen = ((flags & PRECISION_FLAG) ? (size_t)min((int)strLen, precision) : strLen);
            int spaceLen = max(width - precisionLen, 0);

            if(!(flags & LEFT_ALIGN_FLAG))
            {
              for(int i=0; i < spaceLen; i++, writeCount++)
                add_char(spaceChar, &s, isStream);
            }

            for(size_t n=0; n < strLen; n++, str++)
            {
              if(!(flags & PRECISION_FLAG) || n < (size_t)precision)
                add_char(*str, &s, isStream);

              writeCount++;
            }

            if(flags & LEFT_ALIGN_FLAG)
            {
              for(int i=0; i < spaceLen; i++, writeCount++)
                add_char(spaceChar, &s, isStream);
            }
          }
          else
          {
            // If number width is less than precision, then pad the number with zeros until number width is at least precision chars long
            // If number width (including prefixes, sign, and thousands separators) is less than field width, then add spaces (or zeros) until number width matches
            // field width

            int signLen, numberWidth, prefixLen, precisionLen, spaceLen, zeroLen;
            int nonDecimalLen, thousandsLen, bufLen;

            char *decimalLoc = strchr(buf, '.');

            signLen = buf[0] == '-' ? 1 : 0;
            bufLen = strlen(buf);
            numberWidth = bufLen - signLen;

            if(!decimalLoc)
              nonDecimalLen = numberWidth;
            else
              nonDecimalLen = decimalLoc - buf;

            prefixLen = strlen(prefix) + ((signLen || positiveChar != '\0') ? 1 : 0);
            precisionLen = ((flags & PRECISION_FLAG) ? max(precision, numberWidth) : numberWidth);
            thousandsLen = ((flags & THOUSANDS_SEP_FLAG) ? (nonDecimalLen-1) / 3 : 0);
            spaceLen = ((flags & WIDTH_FLAG) ? max(0, width - (precisionLen + prefixLen + thousandsLen)) : 0);
            zeroLen = ((flags & PRECISION_FLAG) ? max(0, precisionLen - numberWidth) : 0);

            // Convert string to upper case if format type is uppercase

            if(isupper(c))
            {
              for(int i=0; i < bufLen; i++)
              {
                if(islower(buf[i]))
                  buf[i] = toupper(buf[i]);
              }
            }


            if(!(flags & LEFT_ALIGN_FLAG))
            {
              for(int n=0; n < spaceLen; n++, writeCount++)
                add_char(spaceChar, &s, isStream);
            }

            add_char(buf[0] == '-' ? '-' : positiveChar, &s, isStream);

            if(buf[0] == '-' || positiveChar != '\0')
              writeCount++;

            for(char *ch=prefix; *ch; ch++, writeCount++)
              add_char(*ch, &s, isStream);

            for(int n=0; n < zeroLen; n++, writeCount++)
              add_char('0', &s, isStream);

            for(int n=0; n < numberWidth; n++, writeCount++)
            {
              add_char(buf[signLen+n], &s, isStream);

              if((flags & THOUSANDS_SEP_FLAG) && (numberWidth-n-1) % 3 == 0 && n != numberWidth - 1)
              {
                add_char(',', &s, isStream);
                writeCount++;
              }
            }

            if(flags & LEFT_ALIGN_FLAG)
            {
              for(int n=0; n < spaceLen; n++, writeCount++)
                add_char(spaceChar, &s, isStream);
            }
          }
          state = STATE_NONE;
          ptr++;
        }
        else
          goto invalid_format;
        break;
      case STATE_FORMAT:
        if(isFlagChar(c))
        {
          state = STATE_FLAGS;
          break;
        }
check_width:
        if(isdigit(c) || c == '*')
        {
          state = STATE_WIDTH;
          break;
        }
check_dot:
        if(c == '.')
        {
          state = STATE_DOT;
          ptr++;
          break;
        }
check_length:
        if(isLengthChar(c))
        {
          state = STATE_LENGTH;
          break;
        }
check_type:
        if(isTypeChar(c))
        {
          state = STATE_TYPE;
          break;
        }
invalid_format:
        state = STATE_NONE;
        break;
      default:
        break;
    }
  }

  return writeCount;
}
