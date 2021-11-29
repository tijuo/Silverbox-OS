#include <stdbool.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>

#define ITOSTR_BUF_LEN      65

enum PrintfLength {
  DEFAULT = 0,
  CHAR,
  SHORT_INT,
  LONG_INT,
  LONG_LONG_INT,
  INTMAX,
  SIZE,
  PTRDIFF,
  LONG_DOUBLE
};
enum PrintfParseState {
  FLAGS = 0, WIDTH, PRECISION, LENGTH, SPECIFIER
};

struct PrintfState {
  bool isZeroPad;
  bool usePrefix;
  bool isLeftJustify;
  bool isForcedPlus;
  bool isBlankPlus;
  bool inPercent;
  bool isError;
  bool isNegative;
  size_t width;
  int precision;
  enum PrintfParseState printfState;
  enum PrintfLength length;
  size_t percentIndex;
  char prefix[2];
  size_t argLength;
  char *string;
};

int do_printf(void **s, int (*writeFunc)(int, void**), const char *formatStr,
              va_list args);
void reset_printf_state(struct PrintfState *state);

void reset_printf_state(struct PrintfState *state) {
  memset(state, 0, sizeof *state);
  state->precision = -1;
}

int do_printf(void **s, int (*writeFunc)(int, void**), const char *formatStr,
              va_list args)
{
  struct PrintfState state;
  char buf[ITOSTR_BUF_LEN];
  size_t i;
  int charsWritten = 0;

  reset_printf_state(&state);

  for(i = 0; formatStr[i]; i++) {
    if(state.inPercent) {
      switch(formatStr[i]) {
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
          switch(state.printfState) {
            case FLAGS:
            case WIDTH:
              if(state.printfState == FLAGS) {
                if(formatStr[i] == '0')
                  state.isZeroPad = true;

                state.printfState = WIDTH;
              }

              state.width = 10 * state.width + formatStr[i] - '0';
              continue;
            case PRECISION:
              state.precision = 10 * state.precision + formatStr[i] - '0';
              continue;
            case LENGTH:
            default:
              state.isError = true;
              break;
          }
          break;
        case 'l':
          if(state.printfState == LENGTH) {
            state.length = LONG_LONG_INT;
            state.printfState = SPECIFIER;
            continue;
          }
          else if(state.printfState < LENGTH) {
            state.length = LONG_INT;
            state.printfState = LENGTH;
            continue;
          }
          else {
            state.isError = true;
            break;
          }
          break;
        case 'h':
          if(state.printfState == LENGTH) {
            state.length = SHORT_INT;
            state.printfState = SPECIFIER;
            continue;
          }
          else if(state.printfState < LENGTH) {
            state.length = CHAR;
            state.printfState = LENGTH;
            continue;
          }
          else {
            state.isError = true;
            break;
          }
          break;
        case 'j':
          if(state.printfState != SPECIFIER) {
            state.length = INTMAX;
            state.printfState = SPECIFIER;
            continue;
          }
          else {
            state.isError = true;
            break;
          }
          break;
        case 'z':
          if(state.printfState != SPECIFIER) {
            state.length = SIZE;
            state.printfState = SPECIFIER;
            continue;
          }
          else {
            state.isError = true;
            break;
          }
          break;
        case 't':
          if(state.printfState != SPECIFIER) {
            state.length = PTRDIFF;
            state.printfState = SPECIFIER;
            continue;
          }
          else {
            state.isError = true;
            break;
          }
          break;
        case 'L':
          if(state.printfState != SPECIFIER) {
            state.length = LONG_DOUBLE;
            state.printfState = SPECIFIER;
            continue;
          }
          else {
            state.isError = true;
            break;
          }
          break;
        case 'c':
        case '%':
          if(formatStr[i] == 'c')
            buf[0] = formatStr[i] == 'c' ? (char)va_arg(args, int) : '%';

          state.string = buf;
          state.argLength = 1;
          break;
        case '.':
          switch(state.printfState) {
            case FLAGS:
            case WIDTH:
              state.precision = 0;
              state.printfState = PRECISION;
              continue;
            default:
              state.isError = true;
              break;
          }
          break;
        case 'p':
        case 'x':
        case 'X': {
          int argLen;

          if(formatStr[i] == 'p')
            state.precision = 2 * sizeof(void*);

          switch(state.length) {
            case LONG_LONG_INT:
              argLen = itostr(va_arg(args, unsigned long long int), buf, 16);
              break;
            case CHAR:
              argLen = itostr((unsigned char)va_arg(args, unsigned int), buf, 16);
              break;
            case SHORT_INT:
              argLen = itostr((unsigned short int)va_arg(args, unsigned int), buf, 16);
              break;
            case LONG_INT:
              argLen = itostr((unsigned long int)va_arg(args, unsigned long int), buf, 16);
              break;
            case DEFAULT:
            default:
              argLen = itostr(va_arg(args, unsigned int), buf, 16);
              break;
          }

          if(argLen > 0) {
            state.argLength = argLen;

            if(state.usePrefix) {
              state.prefix[0] = '0';
              state.prefix[1] = formatStr[i] != 'X' ? 'x' : 'X';
            }

            if(formatStr[i] == 'X') {
              for(size_t pos = 0; pos < state.argLength; pos++) {
                if(buf[pos] >= 'a' && buf[pos] <= 'z')
                  buf[pos] -= 'a' - 'A';
              }
            }

            state.string = buf;
          }
          break;
        }
        case 'o': {
          int argLen;

          switch(state.length) {
            case LONG_LONG_INT:
              argLen = itostr(va_arg(args, unsigned long long int), buf, 8);
              break;
            case CHAR:
              argLen = itostr((unsigned char)va_arg(args, unsigned int), buf, 8);
              break;
            case SHORT_INT:
              argLen = itostr((unsigned short int)va_arg(args, unsigned int), buf, 8);
              break;
            case LONG_INT:
              argLen = itostr((unsigned long int)va_arg(args, unsigned long int), buf, 8);
              break;
            case DEFAULT:
            default:
              argLen = itostr(va_arg(args, unsigned int), buf, 8);
              break;
          }

          if(argLen > 0) {
            state.argLength = (size_t)argLen;

            if(state.usePrefix) {
              state.prefix[0] = '0';
              state.prefix[1] = '\0';
            }
          }

          state.string = buf;
        }
          break;
        case 's':
          state.string = va_arg(args, char*);

          if(state.precision == -1)
            for(state.argLength = 0; state.string[state.argLength];
                state.argLength++)
              ;
          else
            state.argLength = state.precision;

          break;
        case 'u': {
          int argLen;

          switch(state.length) {
            case LONG_LONG_INT:
              argLen = itostr(va_arg(args, unsigned long long int), buf, 10);
              break;
            case CHAR:
              argLen = itostr((unsigned char)va_arg(args, unsigned int), buf, 10);
              break;
            case SHORT_INT:
              argLen = itostr((unsigned short int)va_arg(args, unsigned int), buf, 10);
              break;
            case LONG_INT:
              argLen = itostr((unsigned long int)va_arg(args, unsigned long int), buf, 10);
              break;
            case DEFAULT:
            default:
              argLen = itostr(va_arg(args, unsigned int), buf, 10);
              break;
          }

          if(argLen > 0)
            state.argLength = (size_t)argLen;

          state.string = buf;
        }
          break;
        case 'd':
        case 'i': {
          int argLen;

          switch(state.length) {
            case LONG_LONG_INT:
              argLen = itostr(va_arg(args, long long int), buf, 10);
              break;
            case CHAR:
              argLen = itostr((signed char)va_arg(args, int), buf, 10);
              break;
            case SHORT_INT:
              argLen = itostr((short int)va_arg(args, int), buf, 10);
              break;
            case LONG_INT:
              argLen = itostr((long int)va_arg(args, long int), buf, 10);
              break;
            case DEFAULT:
            default:
              argLen = itostr(va_arg(args, int), buf, 10);
              break;
          }

          if(argLen > 0) {
            state.argLength = (size_t)argLen;
            state.isNegative = buf[0] == '-';
          }
          state.string = buf;
        }
          break;
        case '#':
          if(state.printfState == FLAGS) {
            state.usePrefix = true;
            continue;
          }
          else
            state.isError = true;
          break;
        case '-':
          if(state.printfState == FLAGS) {
            state.isLeftJustify = true;
            continue;
          }
          else
            state.isError = true;
          break;
        case '+':
          if(state.printfState == FLAGS) {
            state.isForcedPlus = true;
            continue;
          }
          else
            state.isError = true;
          break;
        case ' ':
          if(state.printfState == FLAGS) {
            state.isBlankPlus = true;
            continue;
          }
          else
            state.isError = true;
          break;
        case '*':
          switch(state.printfState) {
            case WIDTH:
              state.width = (size_t)va_arg(args, int);
              continue;
            case PRECISION:
              state.precision = (size_t)va_arg(args, int);
              continue;
            default:
              state.isError = true;
              break;
          }
          break;
        case 'n':
          if(state.printfState == WIDTH || state.printfState == PRECISION
             || state.isForcedPlus || state.isBlankPlus || state.isZeroPad
             || state.usePrefix || state.isLeftJustify) {
            state.isError = true;
            break;
          }
          else {
            switch(state.length) {
              case LONG_LONG_INT:
                *va_arg(args, long long int*) = charsWritten;
                break;
              case CHAR:
                *va_arg(args, signed char*) = charsWritten;
                break;
              case SHORT_INT:
                *va_arg(args, short int*) = charsWritten;
                break;
              case LONG_INT:
                *va_arg(args, long int*) = charsWritten;
                break;
              case INTMAX:
                *va_arg(args, intmax_t*) = charsWritten;
                break;
              case SIZE:
                *va_arg(args, size_t*) = charsWritten;
                break;
              case PTRDIFF:
                *va_arg(args, ptrdiff_t*) = charsWritten;
                break;
              case DEFAULT:
              default:
                *va_arg(args, int*) = charsWritten;
                break;
            }
          }
          continue;
        default:
          state.isError = true;
          break;
      }

      if(state.isError) {
        for(size_t j = state.percentIndex; j <= i; j++, charsWritten++) {
          if(writeFunc(formatStr[j], s) != 0)
            return -1;
        }
      }
      else {
        size_t argLength = state.argLength - (state.isNegative ? 1 : 0);
        size_t signLength =
            (state.isNegative || state.isForcedPlus || state.isBlankPlus) ? 1 :
                                                                            0;
        size_t prefixLength;
        size_t zeroPadLength;
        size_t spacePadLength;

        if(state.usePrefix)
          prefixLength =
              state.prefix[0] == '\0' ? 0 : (state.prefix[1] == '\0' ? 1 : 2);
        else
          prefixLength = 0;

        if(formatStr[i] == 's' || state.precision == -1 || argLength > INT_MAX
           || (int)argLength >= state.precision)
          zeroPadLength = 0;
        else
          zeroPadLength = state.precision - argLength;

        if(argLength + signLength + prefixLength + zeroPadLength < state.width)
          spacePadLength = state.width - argLength - signLength - prefixLength
                           - zeroPadLength;
        else
          spacePadLength = 0;

        if(!state.isLeftJustify) {
          for(; spacePadLength > 0; spacePadLength--, charsWritten++) {
            if(writeFunc(' ', s) != 0)
              return -1;
          }
        }

        if(state.isNegative) {
          if(writeFunc('-', s) != 0)
            return -1;
          charsWritten++;
        }
        else if(state.isForcedPlus) {
          if(writeFunc('+', s) != 0)
            return -1;
          charsWritten++;
        }
        else if(state.isBlankPlus) {
          if(writeFunc(' ', s) != 0)
            return -1;
          charsWritten++;
        }

        for(size_t pos = 0; pos < prefixLength; pos++, charsWritten++) {
          if(writeFunc(state.prefix[pos], s) != 0)
            return -1;
        }

        for(; zeroPadLength > 0; zeroPadLength--, charsWritten++) {
          if(writeFunc('0', s) != 0)
            return -1;
        }

        for(size_t pos = (state.isNegative ? 1 : 0); argLength > 0;
            argLength--, pos++, charsWritten++)
        {
          if(writeFunc(state.string[pos], s) != 0)
            return -1;
        }

        if(state.isLeftJustify) {
          for(; spacePadLength > 0; spacePadLength--, charsWritten++) {
            if(writeFunc(' ', s) != 0)
              return -1;
          }
        }
      }
      reset_printf_state(&state);
    }
    else {
      switch(formatStr[i]) {
        case '%':
          state.percentIndex = i;
          state.inPercent = true;
          memset(buf, 0, ITOSTR_BUF_LEN);
          break;
        case '\n':
          if(writeFunc('\r', s) != 0 || writeFunc('\n', s) != 0)
            return -1;
          charsWritten += 2;
          break;
        case '\r':
          if(writeFunc('\r', s) != 0)
            return -1;
          charsWritten++;
          break;
        default:
          if(writeFunc(formatStr[i], s) != 0)
            return -1;
          charsWritten++;
          break;
      }
    }
  }
  return 0;
}
