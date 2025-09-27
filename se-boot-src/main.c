#include <sys/stat.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "se-boot-src/path.h"
#include "se-boot-src/boot.h"
#include "se-boot-src/proc.h"
#include "se-boot-src/log.h"

void help() {
    printf("sm_boot: run command as daemon or boot\n\n");
    printf("   <command>     run command\n");
    printf("   boot          boot script\n");
    printf("   help          show help\n");
    printf("   log          show log\n");
    printf("-------------------------\n");
    printf("available after <log>\n");
    printf("  -h, --help                  Show this help message\n");
    printf("  -s, --start-time TIMESTAMP  Start timestamp (milliseconds)\n");
    printf("  -e, --end-time TIMESTAMP    End timestamp (milliseconds)\n");
    printf("  -t, --type TYPE1[,TYPE2...] Include log types (comma separated)\n");
    printf("  -x, --exclude-type TYPE1[,TYPE2...] Exclude log types (comma separated)\n");
    printf("  -p, --pid PID1[,PID2...]    Include process IDs (comma separated)\n");
    printf("  -X, --exclude-pid PID1[,PID2...] Exclude process IDs (comma separated)\n");
    printf("  -P, --path PATH1[,PATH2...] Include paths (comma separated)\n");
    printf("  -E, --exclude-path PATH1[,PATH2...] Exclude paths (comma separated)\n");
    printf("  -n, --name NAME1[,NAME2...] Include names (comma separated)\n");
    printf("  -N, --exclude-name NAME1[,NAME2...] Exclude names (comma separated)\n");
    printf("  -c, --count NUM             Maximum number of log entries to return\n");
    printf("  -H, --human-time            Use human-readable time format\n");
    printf("      --no-timestamp          Do not show timestamp\n");
    printf("      --no-type               Do not show log type\n");
    printf("      --no-pid                Do not show process ID\n");
    printf("      --no-path               Do not show path\n");
    printf("      --no-name               Do not show name\n");
    printf("  -o, --output FILE           Output to file (default: stdout)\n");
}

int main(int argc, char **argv) {
    if (argc == 1) {
        help();
        return 0;
    }

    if (argc == 2 && (strcmp(argv[1], "help") == 0 || strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0)) {
        help();
        return 0;
    }

    struct stat st = {0};
    if (stat(SE_DIR, &st) == -1) {
        mkdir(SE_DIR, 0777);
    }

    if (stat(SE_DIR, &st) == -1 || stat(SE_DIR, &st) == -1) {
        printf("create ser_boot bad!\n");
        return -1;
    }

    if (access(SE_LOG, F_OK) == 0 && access(SE_LOG, R_OK) < 0) {
        perror(SE_LOG);
        return -1;
    }

    if (access(SE_LOG_LAST, F_OK) == 0 && access(SE_LOG_LAST, R_OK) < 0) {
        perror(SE_LOG_LAST);
        return -1;
    }

    if (argc == 2 && (strcmp(argv[1], "boot") == 0 || strcmp(argv[1], "--boot") == 0 || strcmp(argv[1], "-b") == 0)) {
        boot_main();
        return 0;
    }

    if (argc >= 2 && strcmp(argv[1], "log") == 0){
        log_read_main(argc, argv);
        return 0;
    }

    create_daemon((const char **)argv);
    return 0;
}