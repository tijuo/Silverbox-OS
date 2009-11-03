#ifndef CTYPE_H
#define CTYPE_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#ifdef __GNUC__
#define _PURE(x)	x __attribute__((pure))
#else
#define _PURE(x)	x
#endif /* __GNUC__ */

_PURE(int isalnum(int c));
_PURE(int isalpha(int c));
_PURE(int isblank(int c));
_PURE(int iscntrl(int c));
_PURE(int isdigit(int c));
_PURE(int isgraph(int c));
_PURE(int islower(int c));
_PURE(int isprint(int c));
_PURE(int ispunct(int c));
_PURE(int isspace(int c));
_PURE(int isupper(int c));
_PURE(int isxdigit(int c));
_PURE(int tolower(int c));
_PURE(int toupper(int c));

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CTYPE_H */
