/*
 * semfsctl-sync.c - Userland utility for interacting with semfs semantic filesystem
 *
 * Copyright (c) 2025, Vester Thacker
 * All rights reserved.
 * Licensed under the BSD-2-Clause license.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <err.h>
#include <fcntl.h>
#include <unistd.h>

#define MAX_LINE 256
#define MAX_NODES 1024

struct semfs_node {
    char key[64];
    char value[128];
};

static struct semfs_node nodes[MAX_NODES];
static int node_count = 0;

void load_from_device(const char *path) {
    char buffer[MAX_LINE];
    FILE *fp = fopen(path, "r");
    if (!fp)
        err(1, "Failed to open %s", path);

    while (fgets(buffer, sizeof(buffer), fp)) {
        if (node_count >= MAX_NODES)
            break;
        char *eq = strchr(buffer, '=');
        if (!eq) continue;
        *eq = '\0';
        char *newline = strchr(eq + 1, '\n');
        if (newline) *newline = '\0';

        strncpy(nodes[node_count].key, buffer, sizeof(nodes[node_count].key) - 1);
        strncpy(nodes[node_count].value, eq + 1, sizeof(nodes[node_count].value) - 1);
        node_count++;
    }
    fclose(fp);
}

void query_nodes(const char *qkey, const char *qval) {
    for (int i = 0; i < node_count; ++i) {
        if (strcmp(nodes[i].key, qkey) == 0 && strcmp(nodes[i].value, qval) == 0) {
            printf("%s=%s\n", nodes[i].key, nodes[i].value);
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc != 3 || strcmp(argv[1], "--query") != 0) {
        fprintf(stderr, "Usage: %s --query key=value\n", argv[0]);
        return 1;
    }

    char *query = argv[2];
    char *key = strtok(query, "=");
    char *val = strtok(NULL, "=");

    if (!key || !val) {
        errx(1, "Invalid query format. Expected key=value");
    }

    load_from_device("/dev/semfsctl");
    query_nodes(key, val);

    return 0;
}
