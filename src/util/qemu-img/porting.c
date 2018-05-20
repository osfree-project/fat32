#include <stdlib.h>
#include <io.h> 

#define  INCL_LONGLONG
#define  INCL_BASE
#include <os2.h>

int ftruncate( int handle, int64_t size )
{  
   int rc = 0;
   HFILE hf = _os_handle(handle);
   
   if (size < _filelengthi64(handle)) {
      //rc = chsize(handle, size);
      rc = DosSetFileSizeL(hf, size);
   }
   
   if (rc) { 
     rc = -1; 
   }

   return rc;
}

char *realpath(const char *path, char *resolved_path)
{
    _fullpath(resolved_path, path, _MAX_PATH);
    return resolved_path;
}
