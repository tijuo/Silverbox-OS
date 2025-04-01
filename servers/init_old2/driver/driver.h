#ifndef DRIVER_H
#define DRIVER_H

int driver_register(major_t major, StrSlice name, const DriverOps *ops);
int driver_unregister(major_t major);
Driver *driver_lookup_major(major_t major);
major_t driver_lookup_name(StrSlice name);
major_t driver_lookup_name(StrSlice name);

#endif /* DRIVER_H */