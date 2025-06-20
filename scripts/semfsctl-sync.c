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
#include <ctype.h>

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

void unescape_string(char *str) {
    char *src = str, *dst = str;
    while (*src) {
        if (*src == '\\' && *(src + 1) == '"') {
            *dst++ = '"';
            src += 2;
        } else {
            *dst++ = *src++;
        }
    }
    *dst = '\0';
}

int evaluate_single(const char *expr, const struct semfs_node *node) {
    char key[64], op[3], val[128];

    const char *quote = strchr(expr, '"');
    if (quote) {
        const char *end_quote = strrchr(expr, '"');
        if (end_quote && end_quote > quote) {
            char quoted_val[128];
            size_t len = end_quote - quote - 1;
            if (len >= sizeof(quoted_val)) len = sizeof(quoted_val) - 1;
            strncpy(quoted_val, quote + 1, len);
            quoted_val[len] = '\0';
            unescape_string(quoted_val);

            if (sscanf(expr, "%63[^=><!]%2[=><!]\"", key, op) != 2)
                return 0;

            if (strcmp(op, "=") == 0 || strcmp(op, "==") == 0)
                return strcmp(node->key, key) == 0 && strcmp(node->value, quoted_val) == 0;
            else if (strcmp(op, "!=") == 0)
                return strcmp(node->key, key) == 0 && strcmp(node->value, quoted_val) != 0;
            return 0;
        }
    } else {
        if (sscanf(expr, "%63[^=><!]%2[=><!]%127s", key, op, val) != 3)
            return 0;

        if (strcmp(op, "=") == 0 || strcmp(op, "==") == 0) {
            return strcmp(node->key, key) == 0 && strcmp(node->value, val) == 0;
        } else if (strcmp(op, "!=") == 0) {
            return strcmp(node->key, key) == 0 && strcmp(node->value, val) != 0;
        } else if (strcmp(node->key, key) == 0) {
            int nval = atoi(node->value);
            int target = atoi(val);
            if (strcmp(op, ">") == 0) return nval > target;
            if (strcmp(op, "<") == 0) return nval < target;
            if (strcmp(op, ">=") == 0) return nval >= target;
            if (strcmp(op, "<=") == 0) return nval <= target;
        }
    }
    return 0;
}

int validate_parentheses(const char *expr) {
    int depth = 0;
    while (*expr) {
        if (*expr == '(') depth++;
        if (*expr == ')') depth--;
        if (depth < 0) return 0;
        expr++;
    }
    return depth == 0;
}

int evaluate_parenthesized(const char *expr, const struct semfs_node *node);

int evaluate_expression(const char *expr, const struct semfs_node *node) {
    if (!expr || !*expr) return 0;

    if (*expr == '(') {
        const char *end = strrchr(expr, ')');
        if (!end || end <= expr) return 0;
        char inner[MAX_LINE];
        strncpy(inner, expr + 1, end - expr - 1);
        inner[end - expr - 1] = '\0';
        return evaluate_parenthesized(inner, node);
    }

    return evaluate_single(expr, node);
}

int evaluate_parenthesized(const char *expr, const struct semfs_node *node) {
    if (!validate_parentheses(expr)) {
        fprintf(stderr, "Syntax error: mismatched parentheses in expression.\n");
        return 0;
    }

    char copy[MAX_LINE];
    strncpy(copy, expr, sizeof(copy) - 1);
    copy[sizeof(copy) - 1] = '\0';

    char *and_token = strtok(copy, "&&");
    while (and_token) {
        char *or_token = strtok(and_token, "||");
        int or_result = 0;
        while (or_token) {
            while (isspace(*or_token)) or_token++;
            or_result |= evaluate_expression(or_token, node);
            or_token = strtok(NULL, "||");
        }
        if (!or_result)
            return 0;
        and_token = strtok(NULL, "&&");
    }
    return 1;
}

void query_expression(const char *expression) {
    for (int i = 0; i < node_count; ++i) {
        if (evaluate_parenthesized(expression, &nodes[i])) {
            printf("%s=%s\n", nodes[i].key, nodes[i].value);
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc != 3 || strcmp(argv[1], "--query") != 0) {
        fprintf(stderr, "Usage: %s --query expression\n", argv[0]);
        return 1;
    }

    const char *expr = argv[2];
    if (!validate_parentheses(expr)) {
        fprintf(stderr, "Syntax error: unmatched parentheses in query.\n");
        return 1;
    }

    load_from_device("/dev/semfsctl");
    query_expression(expr);

    return 0;
}
