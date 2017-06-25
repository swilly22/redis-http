#include <string.h>
#include "util.h"

/**
Check if an argument exists in an argument list (argv,argc)
@return -1 if it doesn't exist, otherwise the offset it exists in
*/
int RMUtil_ArgIndex(const char *arg, RedisModuleString **argv, int argc) {

  size_t larg = strlen(arg);
  for (int offset = 0; offset < argc; offset++) {
    size_t l;
    const char *carg = RedisModule_StringPtrLen(argv[offset], &l);
    if (l != larg) continue;
    if (carg != NULL && strncasecmp(carg, arg, larg) == 0) {
      return offset;
    }
  }
  return -1;
}