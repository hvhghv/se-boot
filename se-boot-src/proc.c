#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <string.h>
#include <libgen.h>
#include <errno.h>
#include "se-boot-src/proc.h"
#include "se-boot-src/log.h"

// 创建守护进程
int daemonize() {
    pid_t pid = fork();
    if (pid < 0) {
        return -1;
    } else if (pid > 0) {
        exit(0); // 父进程退出
    }

    // 子进程继续
    // 创建新的会话
    if (setsid() < 0){
        return -1;
    }

    pid = fork();
    if (pid < 0) {
        return -1;
    } else if (pid > 0) {
        exit(0); // 第二个父进程退出
    }

    return 0;
}

pid_t process_run(const char **argv){

    char *argv_clone = strdup(argv[1]);
    if (argv_clone == NULL){
        return -1;
    }

    char *base_name  = basename(argv_clone);
    char msg[256];
    umask(0);
    for (int i = 0; i < sysconf(_SC_OPEN_MAX); i++) {
        close(i);
    }

    int pipe_a[2]; // 用于输入到子进程
    int pipe_b[2]; // 用于从子进程输出

    if (pipe(pipe_a) < 0 || pipe(pipe_b) < 0) {
        log_write(LOG_TYPE_PROCESS, getpid(), argv[1], base_name, strerror(errno));
        free(argv_clone);
        return -1;
    }

    pid_t pid = fork();
    if (pid < 0) {
        log_write(LOG_TYPE_PROCESS, getpid(), argv[1], base_name, strerror(errno));
        free(argv_clone);
        return -2;
    } else if (pid == 0) {
        // 子进程
        close(pipe_a[1]); // 关闭写端
        close(pipe_b[0]); // 关闭读端

        // 重定向标准输入输出
        dup2(pipe_a[0], STDIN_FILENO);
        dup2(pipe_b[1], STDOUT_FILENO);
        dup2(pipe_b[1], STDERR_FILENO);
        
        // 执行新进程
        execvp(argv[1], (char **)(argv + 1));
        log_write(LOG_TYPE_PROCESS, getpid(), argv[1], base_name, strerror(errno));

        free(argv_clone);
        exit(1); // 如果execv失败
        return -3;
    } else {
        // 父进程（守护进程）
        close(pipe_a[0]); // 关闭读端
        close(pipe_b[1]); // 关闭写端

        char buffer[1024];
        ssize_t bytes_read;
        

        log_write(LOG_TYPE_PROCESS, pid, argv[1], base_name, "start!");

        // while ((bytes_read = read(pipe_b[0], buffer, sizeof(buffer) - 1)) > 0) {
        //     buffer[bytes_read] = '\0';
        //     log_write(LOG_TYPE_PROCESS, pid, argv[1], base_name, buffer);
        // }

        while ((bytes_read = read(pipe_b[0], buffer, sizeof(buffer) - 1)) > 0) {
            buffer[bytes_read] = '\0';

            char *line = buffer;
            char *next;

            while (next = strchr(line, '\r')) {
                *next = ' ';
                line  = next + 1;
            }

            line = buffer;

            do {
                next = strchr(line, '\n');
                if (next)
                    *next = '\0';
                if (*line != '\0'){
                    log_write(LOG_TYPE_PROCESS, pid, argv[1], base_name, line);
                }
                line = next + 1;
            } while (next);
        }

        // 等待子进程结束
        int status;
        waitpid(pid, &status, 0);

        close(pipe_a[1]);
        close(pipe_b[0]);


        log_write(LOG_TYPE_PROCESS, pid, argv[1], base_name, "exit!");
        free(argv_clone);
        return pid;
    }
}

pid_t create_daemon(const char **argv) {

    char *filename = strdup(argv[1]);
    if (!filename){
        return -1;
    }

    // 创建守护进程
    if (daemonize() < 0) {
        char msg[256];
        snprintf(msg, sizeof(msg), "daemonize failed!: %s", strerror(errno));
        log_write(LOG_TYPE_PROCESS, getpid(), argv[1], basename(filename), msg);
        free(filename);
        return -1;
    }


    free(filename);
    return process_run(argv);


}

int process_exists(pid_t pid) {
    return (kill(pid, 0) == 0);
}