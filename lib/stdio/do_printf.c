#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>

#define FLG_SPEC      1
#define FLG_FLAG      2
#define FLG_WIDTH     4
#define FLG_PREC      8
#define FLG_LEN	      16
#define FLG_WSTART    32
#define FLG_PSTART    64

#define OPT_LJUST     1		//	'-'
#define OPT_FORCE     2		//	'+'
#define OPT_BLANK     4		//	' '
#define OPT_ZERO      8		//	'0'
#define OPT_POUND     16	//	'#'
#define OPT_WIDTH     32
#define OPT_PREC      64

#define LEN_DEFAULT   0
#define LEN_CHAR      1
#define LEN_SHORT     2
#define LEN_LONG      3
#define LEN_DOUBLE    4

/* FLG_SPEC is set when we first encounter the %.
   FLG_FLAG is set when we're done checking for the flags portion
   FLG_WIDTH is set when we're done checking for the width portion
   FLG_PREC is set when we're don checking for the precision portion
   FLG_LEN is set when we're done checking for the length portion
   FLG_WSTART is set when we've read in the first number of a width
   FLG_PSTART is set when we've read in the first number after a precision

   OPT_WIDTH is set when the width to be used is an argument on the stack
   OPT_PREC is set when the precision to be used is an argument on the stack
*/

/* Flags: -,+,(space),#,0
   Width: (number),*
   Precision: .(number), .*
   Length: hh,h,l,L
*/ 

int do_printf(void *s, const char *format, va_list ap, int is_stream);
static int add_char(char c, void **s, int is_stream);
static int _htostr(unsigned num, char *buf);
static int _itostr(int num, char *buf, int unsign);

static int add_char(char c, void **s, int is_stream)
{
  if( is_stream )
  {
    FILE *stream = *(FILE **)s;
    return fputc( c, stream );
  }
  else
  {
    char **_s = (char **)s;
    char *str = *_s;

    *str = c;
    (*_s)++;
  }
  return 0;
}

// buf should be 8 bytes long

static int _htostr(unsigned num, char *buf)
{
  int chars=0;
  char tmp[8];
  char *ptr = buf;
  char digits[16] = "0123456789abcdef";

  if( num == 0 )
  {
    *ptr = '0';
    return 1;
  }

  while( num )
  {
    tmp[chars++] = digits[num % 16];
    num /= 16;
  }

  while(chars)
    *ptr++ = tmp[--chars];

  return (ptr - buf);
}

// buf should be 10 bytes long

static int _itostr(int num, char *buf, int unsign)
{
  size_t chars=0;
  char tmp[10];
  char *ptr = buf;
  unsigned _num = (unsigned)num;

  /*if( !unsign && num < 0 )
  {
    neg = 1;
    *ptr++ = '-';
  }
  else */if( (unsign ? (int)_num : num) == 0 )
  {
    *ptr = '0';
    return 1;
  }

  if( !unsign )
    num = abs(num);

  while( (unsign ? (int)_num : num) )
  {
    if( unsign )
    {
      tmp[chars++] = '0' + (_num % 10);
      _num /= 10;
    }
    else
    {
      tmp[chars++] = '0' + (num % 10);
      num /= 10;
    }
  }

  while(chars)
    *ptr++ = tmp[--chars];

  return (ptr - buf);
}

int do_printf(void *s, const char *format, va_list ap, int is_stream)
{
  int flags=0, width, precision, options, length;
  const char *ptr = format;
  int lower, write_count=0;

  if( s == NULL )
    return -1;

  while(*ptr)
  {
    if( !(flags & FLG_SPEC) )
    {
      flags = 0;

      if( *ptr == '%' )
      {
        ptr++;
        lower = 0;
        flags = FLG_SPEC;
        width = 0;
        precision = 1;
        options = 0;
        length = LEN_DEFAULT;
        continue;
      }
      else
      {
        add_char(*ptr++, &s, is_stream);
        write_count++;
      }
    }
    else
    {
      if( !(flags & FLG_FLAG) )
      {
        switch( *ptr++ )
        {
          case '-':
            options |= OPT_LJUST;
            break;
          case '+':
            options |= OPT_FORCE;
            break;
          case ' ':
            options |= OPT_BLANK;
            break;
          case '#':
            options |= OPT_POUND;
            break;
          case '0':
            options |= OPT_ZERO;
            break;
          default:
            ptr--;
            break;
        }
        flags |= FLG_FLAG;
      }
      else if( !(flags & FLG_WIDTH) )
      {
        if( isdigit(*ptr) )
        {
          width = width * 10 + (*ptr++ - '0');
          flags |= FLG_WSTART;
        }
        else if( *ptr == '*' && !(flags & FLG_WSTART) )
        {
          options |= OPT_WIDTH;
          flags |= FLG_WIDTH;
          ptr++;
        }
        else if( flags & FLG_WSTART )
          flags |= FLG_WIDTH;
        else
        {
          width = 0;
          flags |= FLG_WIDTH;
        }
      }
      else if( !(flags & FLG_PREC) )
      {
        if( *ptr == '.' && !(flags & FLG_PSTART) )
        {
          flags |= FLG_PSTART;
          precision = 0;
          ptr++;

          if( *ptr == '*' )
          {
            flags |= FLG_PREC;
            options |= OPT_PREC;
            ptr++;
          }
        }
        else if( isdigit(*ptr) && (flags & FLG_PSTART) )
          precision = precision * 10 + (*ptr++ - '0');
        else if( flags & FLG_PSTART )
          flags |= FLG_PREC;
        else
        {
          flags |= FLG_PREC;
          precision = 1;
        }
      }
      else if( !(flags & FLG_LEN) )
      {
        switch(*ptr++)
        {
          case 'h':
            length = LEN_SHORT;

            if( *ptr == 'h' )
            {
              ptr++;
              length = LEN_CHAR;
            }
            break;
          case 'l':
            length = LEN_LONG;
            break;
          case 'L':
            length = LEN_DOUBLE;
            break;
          default:
            length = LEN_DEFAULT;
            ptr--;
            break;
        }
        flags |= FLG_LEN;
      }
      else
      {
        char buf[10];
        char padding;

        if( (options & OPT_LJUST) )
          options &= ~OPT_ZERO;

        padding = (options & OPT_ZERO) ? '0' : ' ';

        if( (options & OPT_WIDTH) )
          width = va_arg(ap, int);

        if( width < 0 )
        {
          options |= OPT_LJUST;
          width = abs(width);
        }

        if( (options & OPT_PREC) )
          precision = va_arg(ap, int);

        if( precision < 0 )
          options &= ~OPT_PREC;

        switch( *ptr++ )
        {
          case 'c':
            add_char(va_arg(ap, char), &s, is_stream);
            write_count++;
            break;
          case 'd':
          case 'i':
          {
            int num = va_arg(ap,int);
            int neg = (num < 0 ? 1 : 0);
            int int_len = _itostr(num,buf, 0), count=0;
            
            if( !(flags & FLG_PSTART) )
              precision = 1;

            if( (options & OPT_LJUST) )
            {
              if( neg )
              {
                count++;
                add_char('-', &s, is_stream);
                write_count++;
              }
              else if( (options & OPT_BLANK) && !neg )
              {
                count++;
                add_char(' ', &s, is_stream);
                write_count++;
              }
              else if( (options & OPT_FORCE) && !neg )
              {
                count++;
                add_char('+', &s, is_stream);
                write_count++;
              }

              for( int i=precision-int_len; i > 0; i-- )
              {
                count++;
                add_char('0', &s, is_stream);
                write_count++;
              }

              if( !(precision == 0 && *buf == '0' && int_len == 1) )
              {
                for( int i=0; i < int_len; i++ )
                {
                  add_char(buf[i], &s, is_stream);
                  write_count++;
                }

                count += int_len;
              }

              for( int i=count; i < width; i++ )
              {
                add_char(padding, &s, is_stream);          
                write_count++;
              }
            }
            else
            {
              if( neg || (!neg && ((options & OPT_BLANK) || 
                                   (options & OPT_FORCE))) )
              {
                count++;
                write_count++;
              }

              if( !(precision == 0 && *buf == '0' && int_len == 1) )
              {
                count += int_len;

                if( precision > int_len )
                  count += precision - int_len;
              }

              for( int i=count; i < width; i++ )
              {
                add_char(padding, &s, is_stream);
                write_count++;
              }

              if( neg )
                add_char('-', &s, is_stream);
              else if( (options & OPT_BLANK) && !neg )
                add_char(' ', &s, is_stream);
              else if( (options & OPT_FORCE) && !neg )
                add_char('+', &s, is_stream);

              for( int i=0; i < precision - int_len; i++ )
                add_char('0', &s, is_stream);

              if( !(precision == 0 && *buf == '0' && int_len == 1) )
              {
                for( int i=0; i < int_len; i++ )
                {
                  add_char(buf[i], &s, is_stream);
                  write_count++;
                }
              }
            }            

            break;
          }
          case 'e':
            break;
          case 'E':
            break;
          case 'f':
            break;
          case 'g':
            break;
          case 'G':
            break;
          case 'o':
            break;
          case 's':
          {
            const char *in_str = va_arg(ap, const char *);
            size_t in_str_len;

            if( (flags & FLG_PSTART) && precision < (int)in_str_len )
              in_str_len = (size_t)precision;
            else
              in_str_len = strlen(in_str);

            if( options & OPT_LJUST )
            {
              for( int i=0; i < (int)in_str_len; i++ )
              {
                add_char(in_str[i], &s, is_stream);
                write_count++;
              }

              for( int i=0; i < width - (int)in_str_len; i++ )
              {
                add_char(' ', &s, is_stream);
                write_count++;
              }
            }
            else
            {
              for( int i=0; i < width - (int)in_str_len; i++ )
              {
                add_char(' ', &s, is_stream);
                write_count++;
              }

              for( int i=0; i < (int)in_str_len; i++ )
              {
                add_char(in_str[i], &s, is_stream);
                write_count++;
              }
            }
            break;
          }
          case 'u':
          {
            unsigned num = va_arg(ap,unsigned);
            int int_len = _itostr(num,buf, 1), count=0;

            if( !(flags & FLG_PSTART) )
              precision = 1;

            if( (options & OPT_LJUST) )
            {
              if( (options & OPT_BLANK) )
              {
                count++;
                add_char(' ', &s, is_stream);
                write_count++;
              }
              else if( (options & OPT_FORCE) )
              {
                count++;
                add_char('+', &s, is_stream);
                write_count++;
              }

              for( int i=0; i < precision-int_len; i++, write_count++ )
              {
                count++;
                add_char('0', &s, is_stream);
              }

              if( !(precision == 0 && *buf == '0' && int_len == 1) )
              {
                for( int i=0; i < int_len; i++, write_count++ )
                  add_char(buf[i], &s, is_stream);    
                count += int_len;
              }

              for( int i=count; i < width; i++, write_count++ )
                add_char(padding, &s, is_stream);
            }
            else
            {
              if( ((options & OPT_BLANK) || (options & OPT_FORCE)) )
              {
                count++;
                write_count++;
              }

              if( !(precision == 0 && buf[0] == '0' && int_len == 1) )
              {
                count += int_len;

                if( precision > int_len )
                  count += precision - int_len;
              }

              for( int i=count; i < width; i++, write_count++ )
                add_char(padding, &s, is_stream);

              if( (options & OPT_BLANK) )
                add_char(' ', &s, is_stream);
              else if( (options & OPT_FORCE) )
                add_char('+', &s, is_stream);

              for( int i=0; i < precision - int_len; i++, write_count++ )
                add_char('0', &s, is_stream);

              if( !(precision == 0 && buf[0] == '0' && int_len == 1) )
              {
                for( int i=0; i < int_len; i++, write_count++ )
                  add_char(buf[i], &s, is_stream);
              }
            }
            break;
          }
          case 'p':
            options |= OPT_POUND;
            length |= LEN_LONG;
          case 'x':
            lower = 1;
          case 'X':
          {
            unsigned num = va_arg(ap,unsigned);
            int h_len = _htostr(num,buf), count=0;

            if( !(flags & FLG_PSTART) )
              precision = 1;

            if( options & OPT_LJUST )
            {
              if( !(precision == 0 && buf[0] == '0' && h_len == 1) )
              {
                if( options & OPT_POUND )
                {
                  add_char('0', &s, is_stream);
                  add_char((lower ? 'x' : 'X'), &s, is_stream);
                  count += 2;
                  write_count += 2;
                }

                for( int i=0; i < precision - h_len; i++, write_count++ )
                {
                  add_char('0', &s, is_stream);
                  count++;
                }

                for( int i=0; i < h_len; i++, write_count++ )
                  add_char(buf[i], &s, is_stream);
              }

              for( int i=count; i < width; i++, write_count++ )
                add_char(padding, &s, is_stream);
            }
            else
            {
              if( !(precision == 0 && buf[0] == '0' && h_len == 1) )
              {
                if( options & OPT_POUND )
                  count += 2;

                count += h_len;
              }

              if( precision > h_len )
                count += (precision - h_len);

              for( int i=count; i < width; i++, write_count++ )
                add_char(padding, &s, is_stream);

              if( !(precision == 0 && buf[0] == '0' && h_len == 1) )
              {
                if( options & OPT_POUND )
                {
                  add_char('0', &s, is_stream);
                  add_char((lower ? 'x' : 'X'), &s, is_stream);
                  write_count += 2;
                }

                for( int i=0; i < precision - h_len; i++, write_count++ )
                  add_char('0', &s, is_stream);

                for( int i=0; i < h_len; i++, write_count++ )
                  add_char(buf[i], &s, is_stream);
              }         
            }
            break;
          }
          case 'n':
          {
            int *count_buf = va_arg(ap, int *);
            *count_buf = write_count;
            break;
          }
          case '%':
            add_char('%', &s, is_stream);
            write_count++;
            break;
          default:
            break;
        }
        flags = 0;
      }
    }
  }

  return write_count;
}
