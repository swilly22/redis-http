#ifndef __UTIL_H__
#define __UTIL_H__

#include <redismodule.h>

/**
Check if an argument exists in an argument list (argv,argc)
@return -1 if it doesn't exist, otherwise the offset it exists in
*/
int RMUtil_ArgIndex(const char *arg, RedisModuleString **argv, int argc);

#endif