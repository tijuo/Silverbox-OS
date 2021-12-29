#ifndef OSLIB_H
#define OSLIB_H

#include <os/mutex.h>
#include <types.h>

#ifdef __cplusplus
extern "C" {
#endif

  /*
int fatGetAttributes( const char *path, unsigned short devNum, struct FileAttributes *attrib );
int fatGetDirList( const char *path, unsigned short devNum, struct FileAttributes *attrib,
                   size_t maxEntries );
int fatReadFile( const char *path, unsigned int offset, unsigned short devNum, char *fileBuffer, size_t length );
int fatCreateDir( const char *path, const char *name, unsigned short devNum );
int fatCreateFile( const char *path, const char *name, unsigned short devNum );
   */
#ifdef __cplusplus
}
#endif /*__cplusplus */
#endif /* OSLIB_H */
