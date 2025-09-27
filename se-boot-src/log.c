#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <dirent.h>
#include <sys/file.h>
#include <getopt.h>
#include "se-boot-src/log.h"
#include "se-boot-src/path.h"

#define SE_LOG_MAX_FILE_SIZE (1024 * 16)
#define SE_LOG_MAX_MSG_SIZE (2048)
#define SE_LOG_READ_BUFFER_SIZE (SE_LOG_MAX_FILE_SIZE * 3) // 10MB默认缓冲区

#define LOG_FILTER_FLAG_ON_FIRST (1 << 0)
#define LOG_FILTER_FLAG_exclude_timestamp (1 << 1)
#define LOG_FILTER_FLAG_exclude_time (1 << 2)
#define LOG_FILTER_FLAG_exclude_type (1 << 3)
#define LOG_FILTER_FLAG_exclude_pid (1 << 4)
#define LOG_FILTER_FLAG_exclude_path (1 << 5)
#define LOG_FILTER_FLAG_exclude_name (1 << 6)
#define LOG_FILTER_FLAG_human_time (1 << 7)
#define LOG_FILTER_FLAG_human_type (1 << 8)

extern const char *log_type_map[];

typedef struct log_filter_t {
    int filter_num;
    int flag;

    long filter_time_start;
    long filter_time_end;

    int *filter_type;
    unsigned int filter_type_size;
    int *filter_exclude_type;
    unsigned int filter_exclude_type_size;

    int *filter_pid;
    unsigned int filter_pid_size;
    int *filter_exclude_pid;
    unsigned int filter_exclude_pid_size;

    char **filter_path;
    unsigned int filter_path_size;
    char **filter_exclude_path;
    unsigned int filter_exclude_path_size;

    char **filter_name;
    unsigned int filter_name_size;
    char **filter_exclude_name;
    unsigned int filter_exclude_name_size;
} log_filter_t;

const char *log_type_map[] = {"process", "boot"};

// 获取当前时间戳（毫秒）
static long get_timestamp() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (long)tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

// 将毫秒时间戳转换为可读时间格式
static void timestamp_to_human(long timestamp, char *buffer, size_t buffer_size) {
    time_t seconds     = timestamp / 1000;
    long milliseconds  = timestamp % 1000;
    struct tm *tm_info = localtime(&seconds);

    strftime(buffer, buffer_size, "%Y-%m-%d %H:%M:%S", tm_info);
    snprintf(buffer + strlen(buffer), buffer_size - strlen(buffer), ":%06ld", milliseconds * 1000);
}

// 获取文件大小
long get_file_size(const char *filename) {
    struct stat st;
    if (stat(filename, &st) == 0) {
        return st.st_size;
    }
    return -1;
}

// 获取日志文件锁
static int lock_log_file(int fd) {
    return flock(fd, LOCK_EX);
}

// 释放日志文件锁
static int unlock_log_file(int fd) {
    return flock(fd, LOCK_UN);
}

static int copy_file(const char *src, const char *dst) {
    FILE *src_file = fopen(src, "r");
    if (!src_file)
        return -1;

    FILE *dst_file = fopen(dst, "w");
    if (!dst_file) {
        fclose(src_file);
        return -1;
    }

    char buffer[1024];
    size_t bytes;
    while ((bytes = fread(buffer, 1, sizeof(buffer), src_file)) > 0) {
        fwrite(buffer, 1, bytes, dst_file);
    }

    fclose(src_file);
    fclose(dst_file);
    return 0;
}

int log_write(int type, int pid, const char *path, const char *name, const char *msg) {
    int fd = open(SE_LOG, O_WRONLY | O_CREAT | O_APPEND, 0666);
    if (fd < 0) {
        return -1;
    }

    // 获取文件锁
    if (lock_log_file(fd) < 0) {
        close(fd);
        return -1;
    }

    // 检查文件大小
    long file_size = get_file_size(SE_LOG);
    if (file_size > SE_LOG_MAX_FILE_SIZE) {
        // 备份当前日志文件

        if (copy_file(SE_LOG, SE_LOG_LAST) < 0){
            close(fd);
            unlock_log_file(fd);
            return -1;
        }

        // 重新打开文件
        close(fd);
        fd = open(SE_LOG, O_WRONLY | O_CREAT | O_TRUNC, 0666);
        if (fd < 0) {
            unlock_log_file(fd);
            return -1;
        }
    }

    // 获取当前时间戳
    long timestamp = get_timestamp();

    // 构建日志消息
    char log_msg[SE_LOG_MAX_MSG_SIZE];
    int msg_len = snprintf(log_msg, sizeof(log_msg), "[%ld][%d][%d][%s][%s]:%s\n", timestamp, type, pid, path, name, msg);

    // 如果消息过长，截断
    if (msg_len >= sizeof(log_msg)) {
        log_msg[sizeof(log_msg) - 2] = '\n';
        log_msg[sizeof(log_msg) - 1] = '\0';
    }

    // 写入日志
    ssize_t written = write(fd, log_msg, strlen(log_msg));

    // 释放文件锁并关闭文件
    unlock_log_file(fd);
    close(fd);

    return (written < 0) ? -1 : 0;
}

// 检查是否匹配过滤条件
static int match_filter(const char *line, log_filter_t *filter) {
    // 这里需要解析日志行并应用过滤条件
    // 简化实现：假设所有字段都存在且格式正确
    long timestamp;
    int type, pid;
    char path[256], name[256], msg[1024];

    if (sscanf(line, "[%ld][%d][%d][%255[^][]][%255[^][]]:%1023[^\n]", &timestamp, &type, &pid, path, name, msg) != 6) {
        return 0;
    }

    // 应用时间过滤
    if (filter->filter_time_start > 0 && timestamp < filter->filter_time_start) {
        return 0;
    }
    if (filter->filter_time_end > 0 && timestamp > filter->filter_time_end) {
        return 0;
    }

    // 应用类型过滤
    if (filter->filter_type_size > 0) {
        int found = 0;
        for (unsigned int i = 0; i < filter->filter_type_size; i++) {
            if (type == filter->filter_type[i]) {
                found = 1;
                break;
            }
        }
        if (!found)
            return 0;
    }

    // 应用排除类型过滤
    if (filter->filter_exclude_type_size > 0) {
        for (unsigned int i = 0; i < filter->filter_exclude_type_size; i++) {
            if (type == filter->filter_exclude_type[i]) {
                return 0;
            }
        }
    }

    // 应用PID过滤
    if (filter->filter_pid_size > 0) {
        int found = 0;
        for (unsigned int i = 0; i < filter->filter_pid_size; i++) {
            if (pid == filter->filter_pid[i]) {
                found = 1;
                break;
            }
        }
        if (!found)
            return 0;
    }

    // 应用排除PID过滤
    if (filter->filter_exclude_pid_size > 0) {
        for (unsigned int i = 0; i < filter->filter_exclude_pid_size; i++) {
            if (pid == filter->filter_exclude_pid[i]) {
                return 0;
            }
        }
    }

    // 应用路径过滤
    if (filter->filter_path_size > 0) {
        int found = 0;
        for (unsigned int i = 0; i < filter->filter_path_size; i++) {
            if (strcmp(path, filter->filter_path[i]) == 0) {
                found = 1;
                break;
            }
        }
        if (!found)
            return 0;
    }

    // 应用排除路径过滤
    if (filter->filter_exclude_path_size > 0) {
        for (unsigned int i = 0; i < filter->filter_exclude_path_size; i++) {
            if (strcmp(path, filter->filter_exclude_path[i]) == 0) {
                return 0;
            }
        }
    }

    // 应用名称过滤
    if (filter->filter_name_size > 0) {
        int found = 0;
        for (unsigned int i = 0; i < filter->filter_name_size; i++) {
            if (strcmp(name, filter->filter_name[i]) == 0) {
                found = 1;
                break;
            }
        }
        if (!found)
            return 0;
    }

    // 应用排除名称过滤
    if (filter->filter_exclude_name_size > 0) {
        for (unsigned int i = 0; i < filter->filter_exclude_name_size; i++) {
            if (strcmp(name, filter->filter_exclude_name[i]) == 0) {
                return 0;
            }
        }
    }

    return 1;
}

// 格式化日志行
static void format_log_line(char *dest, const char *src, log_filter_t *filter) {
    long timestamp;
    int type, pid;
    char path[256], name[256], msg[1024];

    if (sscanf(src, "[%ld][%d][%d][%255[^][]][%255[^][]]:%1023[^\n]", &timestamp, &type, &pid, path, name, msg) != 6) {
        strcpy(dest, src);
        return;
    }

    char human_time[32];
    char output[SE_LOG_MAX_MSG_SIZE] = {0};
    char temp[1200];

    if (!(filter->flag & LOG_FILTER_FLAG_exclude_timestamp)) {
        if (filter->flag & LOG_FILTER_FLAG_human_time) {
            timestamp_to_human(timestamp, human_time, sizeof(human_time));
            snprintf(temp, sizeof(temp), "[%s]", human_time);
        } else {
            snprintf(temp, sizeof(temp), "[%ld]", timestamp);
        }
        strcat(output, temp);
    }

    if (!(filter->flag & LOG_FILTER_FLAG_exclude_type)) {
        if (filter->flag & LOG_FILTER_FLAG_human_type) {
            snprintf(temp, sizeof(temp), "[%s]", log_type_map[type]);
        } else {
            snprintf(temp, sizeof(temp), "[%d]", type);
        }
        strcat(output, temp);
    }

    if (!(filter->flag & LOG_FILTER_FLAG_exclude_pid)) {
        snprintf(temp, sizeof(temp), "[%d]", pid);
        strcat(output, temp);
    }

    if (!(filter->flag & LOG_FILTER_FLAG_exclude_path)) {
        snprintf(temp, sizeof(temp), "[%s]", path);
        strcat(output, temp);
    }

    if (!(filter->flag & LOG_FILTER_FLAG_exclude_name)) {
        snprintf(temp, sizeof(temp), "[%s]", name);
        strcat(output, temp);
    }

    snprintf(temp, sizeof(temp), ":%s\n", msg);
    strcat(output, temp);

    strcpy(dest, output);
}

int log_read(char *buffer, unsigned int size, log_filter_t *filter) {
    char line[SE_LOG_MAX_MSG_SIZE];
    char formatted[SE_LOG_MAX_MSG_SIZE];
    unsigned int total_written = 0;
    int count                  = 0;

    // 读取SE_LOG_LAST文件（如果存在）
    FILE *file = fopen(SE_LOG_LAST, "rb");
    if (file) {
        while (fgets(line, sizeof(line), file)) {
            if (match_filter(line, filter)) {
                format_log_line(formatted, line, filter);
                unsigned int len = strlen(formatted);

                if (total_written + len >= size) {
                    fclose(file);
                    return -1; // 缓冲区不足
                }

                strcpy(buffer + total_written, formatted);
                total_written += len;
                count++;

                if (filter->filter_num > 0 && count >= filter->filter_num) {
                    fclose(file);
                    return count;
                }
            }
        }
        fclose(file);
    }

    // 读取SE_LOG文件
    file = fopen(SE_LOG, "r");
    if (!file) {
        return (count > 0) ? count : -1;
    }

    while (fgets(line, sizeof(line), file)) {
        if (match_filter(line, filter)) {
            format_log_line(formatted, line, filter);
            unsigned int len = strlen(formatted);

            if (total_written + len >= size) {
                fclose(file);
                return -1; // 缓冲区不足
            }

            strcpy(buffer + total_written, formatted);
            total_written += len;
            count++;

            if (filter->filter_num > 0 && count >= filter->filter_num) {
                break;
            }
        }
    }

    fclose(file);
    return count;
}

static int parse_int_list(const char *str, int **result, unsigned int *count) {
    if (!str || !*str)
        return -1;

    char *copy = strdup(str);
    if (!copy)
        return -1;

    // 计算元素数量
    *count = 1;
    for (char *p = copy; *p; p++) {
        if (*p == ',')
            (*count)++;
    }

    // 分配内存
    *result = malloc(*count * sizeof(int));
    if (!*result) {
        free(copy);
        return -1;
    }

    // 解析每个元素
    char *token = strtok(copy, ",");
    for (unsigned int i = 0; i < *count && token; i++) {
        (*result)[i] = atoi(token);
        token        = strtok(NULL, ",");
    }

    free(copy);
    return 0;
}

// 解析逗号分隔的字符串列表
static int parse_str_list(const char *str, char ***result, unsigned int *count) {
    if (!str || !*str)
        return -1;

    char *copy = strdup(str);
    if (!copy)
        return -1;

    // 计算元素数量
    *count = 1;
    for (char *p = copy; *p; p++) {
        if (*p == ',')
            (*count)++;
    }

    // 分配内存
    *result = malloc(*count * sizeof(char *));
    if (!*result) {
        free(copy);
        return -1;
    }

    // 解析每个元素
    char *token = strtok(copy, ",");
    for (unsigned int i = 0; i < *count && token; i++) {
        (*result)[i] = strdup(token);
        token        = strtok(NULL, ",");
    }

    free(copy);
    return 0;
}

// 释放字符串列表
static void free_str_list(char **list, unsigned int count) {
    if (!list)
        return;
    for (unsigned int i = 0; i < count; i++) {
        free(list[i]);
    }
    free(list);
}

int log_read_main(int argc, char *argv[]) {


    log_filter_t filter = {0};
    char *output_file   = NULL;
    int buffer_size     = SE_LOG_READ_BUFFER_SIZE;
    char *buffer        = NULL;
    FILE *output        = stdout;

    // 定义长选项
    static struct option long_options[] = {
        {"start-time", required_argument, 0, 's'},
        {"end-time", required_argument, 0, 'e'},
        {"type", required_argument, 0, 't'},
        {"exclude-type", required_argument, 0, 'x'},
        {"pid", required_argument, 0, 'p'},
        {"exclude-pid", required_argument, 0, 'X'},
        {"path", required_argument, 0, 'P'},
        {"exclude-path", required_argument, 0, 'E'},
        {"name", required_argument, 0, 'n'},
        {"exclude-name", required_argument, 0, 'N'},
        {"count", required_argument, 0, 'c'},
        {"human-time", no_argument, 0, 'H'},
        {"no-timestamp", no_argument, 0, 0},
        {"no-type", no_argument, 0, 0},
        {"no-pid", no_argument, 0, 0},
        {"no-path", no_argument, 0, 0},
        {"no-name", no_argument, 0, 0},
        {"output", required_argument, 0, 'o'},
        {0, 0, 0, 0}};

    int opt;
    int option_index = 0;

    while ((opt = getopt_long(argc, argv, "hs:e:t:x:p:X:P:E:n:N:c:Ho:", long_options, &option_index)) != -1) {
        switch (opt) {
            
        case 's':
            filter.filter_time_start = atol(optarg);
            break;

        case 'e':
            filter.filter_time_end = atol(optarg);
            break;

        case 't':
            if (parse_int_list(optarg, &filter.filter_type, &filter.filter_type_size) != 0) {
                fprintf(stderr, "Failed to parse type list\n");
                return 1;
            }
            break;

        case 'x':
            if (parse_int_list(optarg, &filter.filter_exclude_type, &filter.filter_exclude_type_size) != 0) {
                fprintf(stderr, "Failed to parse exclude type list\n");
                return 1;
            }
            break;

        case 'p':
            if (parse_int_list(optarg, &filter.filter_pid, &filter.filter_pid_size) != 0) {
                fprintf(stderr, "Failed to parse PID list\n");
                return 1;
            }
            break;

        case 'X':
            if (parse_int_list(optarg, &filter.filter_exclude_pid, &filter.filter_exclude_pid_size) != 0) {
                fprintf(stderr, "Failed to parse exclude PID list\n");
                return 1;
            }
            break;

        case 'P':
            if (parse_str_list(optarg, &filter.filter_path, &filter.filter_path_size) != 0) {
                fprintf(stderr, "Failed to parse path list\n");
                return 1;
            }
            break;

        case 'E':
            if (parse_str_list(optarg, &filter.filter_exclude_path, &filter.filter_exclude_path_size) != 0) {
                fprintf(stderr, "Failed to parse exclude path list\n");
                return 1;
            }
            break;

        case 'n':
            if (parse_str_list(optarg, &filter.filter_name, &filter.filter_name_size) != 0) {
                fprintf(stderr, "Failed to parse name list\n");
                return 1;
            }
            break;

        case 'N':
            if (parse_str_list(optarg, &filter.filter_exclude_name, &filter.filter_exclude_name_size) != 0) {
                fprintf(stderr, "Failed to parse exclude name list\n");
                return 1;
            }
            break;

        case 'c':
            filter.filter_num = atoi(optarg);
            break;

        case 'H':
            filter.flag |= LOG_FILTER_FLAG_human_time;
            break;

        case 'o':
            output_file = strdup(optarg);
            break;

        case 0:
            // 处理无短选项的长选项
            if (strcmp(long_options[option_index].name, "no-timestamp") == 0) {
                filter.flag |= LOG_FILTER_FLAG_exclude_timestamp;
            } else if (strcmp(long_options[option_index].name, "no-type") == 0) {
                filter.flag |= LOG_FILTER_FLAG_exclude_type;
            } else if (strcmp(long_options[option_index].name, "no-pid") == 0) {
                filter.flag |= LOG_FILTER_FLAG_exclude_pid;
            } else if (strcmp(long_options[option_index].name, "no-path") == 0) {
                filter.flag |= LOG_FILTER_FLAG_exclude_path;
            } else if (strcmp(long_options[option_index].name, "no-name") == 0) {
                filter.flag |= LOG_FILTER_FLAG_exclude_name;
            }
            break;

        default:
            fprintf(stderr, "Unknown option. Use -h for help.\n");
            return 1;
        }
    }

    // 分配缓冲区
    buffer = malloc(buffer_size);
    if (!buffer) {
        fprintf(stderr, "Failed to allocate buffer\n");
        goto cleanup;
    }

    // 打开输出文件
    if (output_file) {
        output = fopen(output_file, "w");
        if (!output) {
            fprintf(stderr, "Failed to open output file: %s\n", output_file);
            goto cleanup;
        }
    }

    // 读取日志
    int count = log_read(buffer, buffer_size, &filter);
    if (count < 0) {
        fprintf(stderr, "Failed to read log\n");
        goto cleanup;
    }

    // 输出结果
    fwrite(buffer, 1, strlen(buffer), output);

cleanup:
    // 清理资源
    if (buffer)
        free(buffer);
    if (output_file)
        free(output_file);
    if (output && output != stdout)
        fclose(output);

    if (filter.filter_type)
        free(filter.filter_type);
    if (filter.filter_exclude_type)
        free(filter.filter_exclude_type);
    if (filter.filter_pid)
        free(filter.filter_pid);
    if (filter.filter_exclude_pid)
        free(filter.filter_exclude_pid);

    if (filter.filter_path)
        free_str_list(filter.filter_path, filter.filter_path_size);
    if (filter.filter_exclude_path)
        free_str_list(filter.filter_exclude_path, filter.filter_exclude_path_size);
    if (filter.filter_name)
        free_str_list(filter.filter_name, filter.filter_name_size);
    if (filter.filter_exclude_name)
        free_str_list(filter.filter_exclude_name, filter.filter_exclude_name_size);

    return 0;
}