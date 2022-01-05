struct fs_callbacks {
  int (*create)(int type);
  int (*read)();
  int (*write)();
  int (*get)(int id, ...); // list
  int (*set)(int id, ...); // rename,
  int (*remove)();
};

// Mount a device to a name and flags
// fs may be FS_AUTODETECT
// there has to be a way to distingush which fs is on a device
// mount(int device, char *name, int flags, int fs)
