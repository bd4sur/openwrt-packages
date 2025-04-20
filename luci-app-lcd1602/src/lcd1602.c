// LCD1602/2004 I2C
// BD4SUR - 2017-02 2025-04

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "i2c1602.h"

void show_usage() {
    fprintf(stderr, "LCD1602 - Controlling LCD1602/2004 attached to I2C bus.\n");
    fprintf(stderr, "  (c) BD4SUR 2017-02 2025-04\n");
    fprintf(stderr, "Usage:   lcd1602 \"up to 80 chars\"\n");
    exit(-1);
}

int main(int argc, char **argv) {

    //              0                   1                   2                   3
    //              0123456789abcdefghij0123456789abcdefghij0123456789abcdefghij0123456789abcdefghij
    char pat[81] = "   BD4SUR OpenWrt                       2025-04-17  22:51:52     ARE YOU OK?    ";

    char *content = pat;

    int is_default = 0;

    if(argc >= 2) {
        int len = strlen(argv[1]);
        if(len > 0 && len <= 80) {
            is_default = 0;
            content = argv[1];
        }
    }

    for(int i = 2; i < argc; i += 2) {
        // do some basic validation
        if (i + 1 >= argc) { show_usage(); } // must have arg after flag
        if (argv[i][0] != '-') { show_usage(); } // must start with dash
        if (strlen(argv[i]) != 2) { show_usage(); } // must be -x (one dash, one letter)
        if (argv[i][1] == 'd') { is_default = 1; }
        else { show_usage(); }
    }

    i2c1602_init(20, 4);
    i2c1602_backlight();
    i2c1602_clear();

    if (is_default) {
        char buffer[32];
        while(1) {
            time_t now = time(NULL);
            struct tm *tm = localtime(&now);
            strftime(buffer, sizeof(buffer), "%Y-%m-%d  %H:%M:%S", tm);
            for (int i = 40; i < 60; i++) {
                content[i] = buffer[i-40];
            }
            i2c1602_printstr(content);
            usleep(10*1000);
        }
    }
    else {
        char buffer[100];
        while(1) {
            FILE *fp;
            fp = popen("uci get lcd1602.config.content", "r");
            if (fp == NULL) {
                perror("popen failed");
                return 1;
            }
        
            // 逐行读取输出
            while (fgets(buffer, sizeof(buffer), fp) != NULL) {
                printf("Output: [%s]", buffer);
            }

            int buffer_length = strlen(buffer);
            buffer[buffer_length - 1] = '\0'; // 去掉\n
            buffer_length = strlen(buffer);
            if (buffer_length > 0 && buffer_length <= 80) {
                i2c1602_printstr(buffer);
            }

            // 关闭流并获取命令的退出状态
            int status = pclose(fp);
            if (status == -1) {
                perror("pclose failed");
            } else {
                printf("Command exited with status: %d\n", WEXITSTATUS(status));
            }

            usleep(100*1000);
        }
    }

    i2c1602_close();

    return 0;
}
