/* semfs.c - Dynamic semantic filesystem with userland injection and kqueue support */

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

#include <openzfs/sys/zfs_context.h>

MALLOC_DEFINE(M_SEMFS, "semfs", "semantic fs memory");

SYSCTL_NODE(_vfs, OID_AUTO, semfs, CTLFLAG_RW | CTLFLAG_MPSAFE, 0, "semfs controls");
static int semfs_log_level = LOG_INFO;
SYSCTL_INT(_vfs_semfs, OID_AUTO, log_level, CTLFLAG_RW, &semfs_log_level, 0, "Logging severity for semfs");

static int semfs_max_nodes = 1024;
SYSCTL_INT(_vfs_semfs, OID_AUTO, max_nodes, CTLFLAG_RW, &semfs_max_nodes, 0, "Maximum allowed semantic nodes");

struct semfs_node {
    TAILQ_ENTRY(semfs_node) entries;
    char name[NAME_MAX];
    char *content;
    int namelen;
    int size;
};

static TAILQ_HEAD(, semfs_node) semfs_file_list;
static int semfs_node_count = 0;
SYSCTL_INT(_vfs_semfs, OID_AUTO, active_nodes, CTLFLAG_RD, &semfs_node_count, 0, "Current number of semantic nodes");
static struct sx semfs_lock;
static struct selinfo semfs_selinfo;
static int semfs_ready = 0;

static d_write_t semfs_dev_write;
static d_read_t semfs_dev_read;
static d_poll_t semfs_dev_poll;

static struct cdev *semfs_dev;

static struct cdevsw semfs_cdevsw = {
    .d_version = D_VERSION,
    .d_write = semfs_dev_write,
    .d_read = semfs_dev_read,
    .d_poll = semfs_dev_poll,
    .d_name = "semfsctl"
};

static void
semfs_clear_all_nodes(void)
{
    struct semfs_node *node;
    sx_xlock(&semfs_lock);
    while (!TAILQ_EMPTY(&semfs_file_list)) {
        node = TAILQ_FIRST(&semfs_file_list);
        TAILQ_REMOVE(&semfs_file_list, node, entries);
        free(node->content, M_SEMFS);
        free(node, M_SEMFS);
        semfs_node_count--;
    }
    sx_xunlock(&semfs_lock);
}

static int
semfs_dev_write(struct cdev *dev, struct uio *uio, int ioflag)
{
    char buf[512];
    int error;

    size_t len = MIN(uio->uio_resid, sizeof(buf) - 1);
    error = uiomove(buf, len, uio);
    if (error) return error;
    buf[len] = '\0';

    if (strcmp(buf, "!RESET") == 0) {
        semfs_clear_all_nodes();
        log(semfs_log_level, "semfs: all state cleared via !RESET\n");
        return 0;
    }

    char *line, *rest = buf;
    while ((line = strsep(&rest, "\n")) != NULL) {
        if (line[0] == '\0') continue;

        char *name = strsep(&line, ":");
        char *content = strsep(&line, ":");
        if (!name || !content ||
            strlen(name) < 1 || strspn(name, " \t") == strlen(name) ||
            strlen(content) < 1 || strspn(content, " \t") == strlen(content)) {
            log(semfs_log_level, "semfs: invalid name or content rejected.\n");
            continue;
        }

        sx_xlock(&semfs_lock);
        struct semfs_node *iter;
        TAILQ_FOREACH(iter, &semfs_file_list, entries) {
            if (strncmp(iter->name, name, NAME_MAX) == 0) {
                sx_xunlock(&semfs_lock);
                log(semfs_log_level, "semfs: duplicate entry '%s' rejected.\n", name);
                continue;
            }
        }

        if (semfs_node_count >= semfs_max_nodes) {
            sx_xunlock(&semfs_lock);
            log(semfs_log_level, "semfs: node limit (%d) exceeded. Rejecting entry '%s'.\n", semfs_max_nodes, name);
            break;
        }

        struct semfs_node *node = malloc(sizeof(*node), M_SEMFS, M_WAITOK | M_ZERO);
        strlcpy(node->name, name, sizeof(node->name));
        node->namelen = strlen(node->name);
        node->size = strlen(content);
        node->content = malloc(node->size + 1, M_SEMFS, M_WAITOK);
        strlcpy(node->content, content, node->size + 1);
        TAILQ_INSERT_TAIL(&semfs_file_list, node, entries);
        semfs_node_count++;
        sx_xunlock(&semfs_lock);

        log(semfs_log_level, "semfs: file '%s' created with %d bytes.\n", node->name, node->size);
    }

    return 0;
}

static int
semfs_dev_read(struct cdev *dev, struct uio *uio, int ioflag)
{
    static char output_buf[8192];
    size_t offset = 0;
    struct semfs_node *node;

    sx_slock(&semfs_lock);
    TAILQ_FOREACH(node, &semfs_file_list, entries) {
        if (offset + node->namelen + node->size + 2 >= sizeof(output_buf)) {
            break;
        }
        offset += snprintf(output_buf + offset, sizeof(output_buf) - offset,
                           "%s:%s\n", node->name, node->content);
    }
    sx_sunlock(&semfs_lock);

    return uiomove(output_buf + uio->uio_offset,
                   MIN(offset - uio->uio_offset, uio->uio_resid), uio);
}