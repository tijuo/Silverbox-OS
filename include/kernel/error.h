#ifndef ERROR_H
#define ERROR_H

#define NOERR		0
#define ENULL		1
#define EINVL		ENULL
#define EPERM		2
#define EFAIL		3

#define E_DONE		1
#define E_OK            0
#define E_FAIL          -1
#define E_INVALID_ARG   -2
#define E_NOT_MAPPED    -3
#define E_OVERWRITE     -4
#define E_RANGE		-5
#define E_PERM		-6
#define E_BLOCK		-7

#define IS_ERROR(x)	((x) < 0)

#endif /* ERROR_H */
