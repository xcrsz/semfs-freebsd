#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX_LINE 1024

int main() {
    FILE *fp = fopen("/dev/semfsctl", "w");
    if (!fp) {
        perror("open /dev/semfsctl");
        return 1;
    }

    char *entries[][2] = {
        {"system.status", "Running"},
        {"net.state", "Online"},
        {"semfs.readonly", "1"},
        {NULL, NULL}
    };

    for (int i = 0; entries[i][0]; i++) {
        char line[MAX_LINE];
        snprintf(line, sizeof(line), "%s:%s\n", entries[i][0], entries[i][1]);
        fputs(line, fp);
    }

    fclose(fp);
    return 0;
}