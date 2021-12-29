#ifndef DRIVERS_PSEUDO_H
#define DRIVERS_PSEUDO_H

#define PSEUDO_MAJOR  0
#define PSEUDO_NAME   "pseudo"
#define NULL_MINOR    0
#define RAND_MINOR    1

int pseudo_init(void);

#endif /* DRIVERS_PSEUDO_H */
