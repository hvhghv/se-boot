#ifdef __cplusplus
extern "C" {
#endif

#ifndef SE_BOOT_LOG_H
#define SE_BOOT_LOG_H

#include <sys/types.h>


#define LOG_TYPE_PROCESS 0
#define LOG_TYPE_BOOT 1

#define LOG_DEFAULT_COUNT 30



int log_write(int type, int pid, const char *path, const char *name, const char *msg);
int log_read_main(int argc, char *argv[]);

#endif

#ifdef __cplusplus
}
#endif