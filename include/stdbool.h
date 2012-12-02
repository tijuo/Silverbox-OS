#ifndef _STDBOOL_H
#define _STDBOOL_H

#ifndef __cplusplus

#define bool _Bool
#define false (_Bool)0
#define true  (_Bool)1

#else

#define _Bool   bool
#define false   false
#define true    true

#endif /* __cplusplus */

#define __bool_true_false_are_defined 1

#endif /* _STDBOOL_H */
