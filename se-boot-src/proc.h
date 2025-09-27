#ifdef __cplusplus
extern "C" {
#endif

#ifndef SE_BOOT_PROC_H
#define SE_BOOT_PROC_H

#include <unistd.h>

int process_run(const char **argv);
pid_t create_daemon(const char **argv);
int process_exists(pid_t pid);
int daemonize();
#endif

#ifdef __cplusplus
}
#endif