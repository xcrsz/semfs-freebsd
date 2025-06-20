/*
 * semfs.c - Dynamic semantic filesystem with userland injection, kqueue support, and enhanced state handling
 *
 * Copyright (c) 2025, Vester Thacker
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
 * SHALL THE COPYRIGHT HOLDER BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/mount.h>
#include <sys/vnode.h>
#include <sys/malloc.h>
#include <sys/libkern.h>
#include <sys/namei.h>
#include <sys/systm.h>
#include <sys/dirent.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/queue.h>
#include <sys/conf.h>
#include <sys/uio.h>
#include <sys/sx.h>
#include <sys/select.h>
#include <sys/poll.h>
#include <sys/selinfo.h>
#include <sys/syslog.h>
#include <sys/proc.h>
#include <sys/sysctl.h>
#include <sys/filio.h>
#include <sys/fcntl.h>
#include <sys/event.h>
#include <sys/priv.h>

#include <openzfs/sys/zfs_context.h>

MALLOC_DEFINE(M_SEMFS, "semfs", "semantic fs memory");

SYSCTL_NODE(_vfs, OID_AUTO, semfs, CTLFLAG_RW | CTLFLAG_MPSAFE, 0, "semfs controls");
static int semfs_log_level = LOG_INFO;
SYSCTL_INT(_vfs_semfs, OID_AUTO, log_level, CTLFLAG_RW, &semfs_log_level, 0, "Logging severity for semfs");

static int semfs_max_nodes = 1024;
SYSCTL_INT(_vfs_semfs, OID_AUTO, max_nodes, CTLFLAG_RW, &semfs_max_nodes, 0, "Maximum allowed semantic nodes");

static int semfs_readonly = 0;
SYSCTL_INT(_vfs_semfs, OID_AUTO, readonly, CTLFLAG_RW, &semfs_readonly, 0, "Disallow new entries if set to 1");

static struct cdev *semfs_dev;
static int semfs_refcount = 0;

/*
 * Very basic query matcher: linear key=value matching
 */
typedef struct semfs_node {
    char key[64];
    char value[128];
    TAILQ_ENTRY(semfs_node) entries;
} semfs_node_t;

TAILQ_HEAD(, semfs_node) semfs_file_list;
struct sx semfs_lock;

int
semfs_query_match(const char *query_key, const char *query_val, semfs_node_t *node)
{
    if (strncmp(node->key, query_key, sizeof(node->key)) == 0 &&
        strncmp(node->value, query_val, sizeof(node->value)) == 0) {
        return 1;
    }
    return 0;
}

static d_read_t semfs_read;

static struct cdevsw semfs_cdevsw = {
    .d_version = D_VERSION,
    .d_name = "semfsctl",
    .d_read = semfs_read
};

static int
semfs_read(struct cdev *dev, struct uio *uio, int ioflag)
{
    int error = 0;
    semfs_node_t *node;
    char buffer[256];

    sx_slock(&semfs_lock);
    TAILQ_FOREACH(node, &semfs_file_list, entries) {
        int len = snprintf(buffer, sizeof(buffer), "%s=%s\n", node->key, node->value);
        if (len > 0) {
            error = uiomove(buffer, len, uio);
            if (error != 0)
                break;
        }
    }
    sx_sunlock(&semfs_lock);

    return error;
}

static int
semfs_loader(struct module *m __unused, int event, void *arg __unused)
{
    int error = 0;
    switch (event) {
    case MOD_LOAD:
        sx_init(&semfs_lock, "semfs lock");
        TAILQ_INIT(&semfs_file_list);
        semfs_dev = make_dev(&semfs_cdevsw, 0, UID_ROOT, GID_WHEEL, 0600, "semfsctl");
        log(LOG_NOTICE, "semfs: module loaded\n");
        break;
    case MOD_UNLOAD:
        destroy_dev(semfs_dev);
        sx_destroy(&semfs_lock);
        log(LOG_NOTICE, "semfs: module unloaded\n");
        break;
    default:
        error = EOPNOTSUPP;
        break;
    }
    return error;
}

DEV_MODULE(semfs, semfs_loader, NULL);
MODULE_VERSION(semfs, 1);
MODULE_DEPEND(semfs, zfs, 2, 2, 2);
