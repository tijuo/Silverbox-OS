#ifndef CTYPE_H
#define CTYPE_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

CONST int isalnum(int c);
CONST int isalpha(int c);
CONST int isblank(int c);
CONST int iscntrl(int c);
CONST int isdigit(int c);
CONST int isgraph(int c);
CONST int islower(int c);
CONST int isprint(int c);
CONST int ispunct(int c);
CONST int isspace(int c);
CONST int isupper(int c);
CONST int isxdigit(int c);
CONST int tolower(int c);
CONST int toupper(int c);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CTYPE_H */
