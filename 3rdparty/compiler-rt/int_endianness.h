#include <arch/loader.h>

#ifdef ARCH_LITTLE_ENDIAN
# define _YUGA_LITTLE_ENDIAN 1
# define _YUGA_BIG_ENDIAN    0
#else
# define _YUGA_LITTLE_ENDIAN 0
# define _YUGA_BIG_ENDIAN    1
#endif
