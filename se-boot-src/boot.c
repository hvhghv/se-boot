#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>
#include <ctype.h>
#include <errno.h>
#include <time.h>
#include "se-boot-src/path.h"
#include "se-boot-src/log.h"
#include "se-boot-src/proc.h"

/* 脚本信息结构体 */
typedef struct {
    int number; // 脚本序号
    int timeout;
    char *path; // 完整路径
} ScriptInfo;

/* 脚本比较函数用于qsort */
static int script_compare(const void *a, const void *b) {
    const ScriptInfo *sa = (const ScriptInfo *)a;
    const ScriptInfo *sb = (const ScriptInfo *)b;
    return sa->number - sb->number;
}

void boot_main() {
    /* 第1步：检查PID文件是否存在 */
    if (access(SE_PID_FILE, F_OK) == 0) {
        /* 第2步：检查文件中的PID是否仍在运行 */
        FILE *pid_file = fopen(SE_PID_FILE, "r");
        if (pid_file) {
            pid_t old_pid;
            if (fscanf(pid_file, "%d", &old_pid) == 1) {
                if (process_exists(old_pid)) {
                    fclose(pid_file);
                    return; // 进程存在则退出
                }
            }
            fclose(pid_file);
        }
    }

    daemonize();

    /* 第3步：写入当前PID到文件 */
    FILE *pid_file = fopen(SE_PID_FILE, "w");
    if (!pid_file) {
        log_write(LOG_TYPE_BOOT, getpid(), "/", "se-boot", strerror(errno));
        return;
    }

    fprintf(pid_file, "%d", getpid());
    fclose(pid_file);

    /* 第4步：执行/etc/sm_init下的脚本 */

    struct stat st = {0};
    if (stat(SCRIPT_DIR, &st) == -1) {
        mkdir(SCRIPT_DIR, 0777);
    }

    DIR *dir = opendir(SCRIPT_DIR);
    if (!dir) {
        log_write(LOG_TYPE_BOOT, getpid(), "/", "se-boot", strerror(errno));
        return;
    }

    /* 动态收集脚本文件 */
    ScriptInfo *scripts = NULL;
    size_t count        = 0;
    size_t capacity     = 0;

    struct dirent *entry;
    char msg[1200];
    while ((entry = readdir(dir)) != NULL) {
        /* 检查文件名格式：前两位为数字，第三位是下划线 */
        if (strlen(entry->d_name) < 6)
            continue;
        if (!isdigit(entry->d_name[0]) ||
            !isdigit(entry->d_name[1]) ||
            entry->d_name[2] != '_' ||
            !isdigit(entry->d_name[3]) ||
            !isdigit(entry->d_name[4]) ||
            entry->d_name[5] != '_') {

            snprintf(msg, sizeof(msg), "%s :file format err!", entry->d_name);
            log_write(LOG_TYPE_BOOT, getpid(), "/", "se-boot", msg);
            continue;
        }

        /* 提取脚本序号 */
        int num     = atoi(entry->d_name);
        int timeout = atoi(&entry->d_name[3]);

        /* 分配内存并存储路径 */
        char *path = malloc(strlen(SCRIPT_DIR) + strlen(entry->d_name) + 1);
        if (!path) {
            log_write(LOG_TYPE_BOOT, getpid(), "/", "se-boot", "no memory!");
            continue;
        }

        sprintf(path, "%s%s", SCRIPT_DIR, entry->d_name);

        /* 动态扩展数组 */
        if (count >= capacity) {
            capacity                = (capacity == 0) ? 16 : capacity * 2;
            ScriptInfo *new_scripts = realloc(scripts, capacity * sizeof(ScriptInfo));
            if (!new_scripts) {
                log_write(LOG_TYPE_BOOT, getpid(), "/", "se-boot", "no memory!");
                free(path);
                continue;
            }
            scripts = new_scripts;
        }

        /* 存储脚本信息 */
        scripts[count].number  = num;
        scripts[count].timeout = timeout;
        scripts[count].path    = path;
        count++;
    }
    closedir(dir);

    /* 按序号排序 */
    if (count > 0) {
        qsort(scripts, count, sizeof(ScriptInfo), script_compare);

        for (size_t i = 0; i < count; i++) {
            pid_t pid = fork();
            if (pid < 0){
                log_write(LOG_TYPE_BOOT, getpid(), "/", "se-boot", strerror(errno));
            }

            if (pid == 0){
                const char *argv[3] = {scripts[i].path, scripts[i].path, NULL};
                process_run(argv);
                exit(0);
                return;
            }

            if (pid > 0) {
                // 父进程
                int status;
                time_t start = time(NULL);

                while (time(NULL) - start < scripts[i].timeout) {
                    if (waitpid(pid, &status, WNOHANG) == pid) {
                        break;
                    }

                    usleep(10000); // 短暂睡眠避免忙等待
                }

                if (time(NULL) - start >= scripts[i].timeout) {
                    snprintf(msg, sizeof(msg), "%s :timeout!", scripts[i].path);
                    log_write(LOG_TYPE_BOOT, getpid(), "/", "se-boot", msg);
                }

                free(scripts[i].path);
            }
        }
    }

    free(scripts);

    while (1){
        pid_t wait_ret = wait(NULL);
        if (wait_ret == -1 && errno == ECHILD){
            break;
        }
    }

    while (1){
        pause();
    }
    /* 第5步：退出程序 */
    return;
}