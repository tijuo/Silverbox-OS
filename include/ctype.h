#ifndef CTYPE_H
#define CTYPE_H

#ifdef __GNUC__
#define _PURE(x)	(x) __attribute__((pure))
#else
#define _PURE(x)	(x)
#endif /* __GNUC__ */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

int _PURE(isalnum(int c));
int _PURE(isalpha(int c));
int _PURE(isblank(int c));
int _PURE(iscntrl(int c));
int _PURE(isdigit(int c));
int _PURE(isgraph(int c));
int _PURE(islower(int c));
int _PURE(isprint(int c));
int _PURE(ispunct(int c));
int _PURE(isspace(int c));
int _PURE(isupper(int c));
int _PURE(isxdigit(int c));
int _PURE(tolower(int c));
int _PURE(toupper(int c));

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CTYPE_H */
