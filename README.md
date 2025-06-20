# SemFS-FreeBSD: Semantic Filesystem Module

SemFS is a synthetic kernel module for FreeBSD providing a semantic key-value filesystem exposed via `/dev/semfsctl`.
This enables semantic data injection, query, and state persistence from userland or monitoring agents.

## Features

- Synthetic file entries: `name:content`
- `kqueue` and `poll` support
- `readonly` mode via `sysctl vfs.semfs.readonly=1`
- Timestamp tracking
- Persistent state restoration via `/var/db/semfs.state`
- Userland injection tool: `semfsctl-sync`
- Devfs permission rules (0600)
- ATF/Kyua-compatible test suite

## Kernel Interface

Write entries:
```sh
echo "status.uptime:42d" > /dev/semfsctl
```

Read all entries:
```sh
cat /dev/semfsctl
```

Reset state:
```sh
echo "!RESET" > /dev/semfsctl
```

## Runtime Controls

```sh
sysctl vfs.semfs.readonly=1
sysctl vfs.semfs.max_nodes
```
