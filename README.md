# VfsRamFs crash reproducer (KasperskyOS CE 1.4.0.102)

On KasperskyOS Community Edition 1.4.0.102 (QEMU, aarch64), the SDK's prebuilt VfsRamFs takes an unhandled page fault
(a NULL pointer dereference in `inode_destructor`) when several threads of one client create, stat, list and unlink
files in `/tmp` at the same time. The kernel then terminates VfsRamFs, and its client loses its file system
(`[VFS_CLIENT] Connection to vfs lost.`).

Reported on the Kaspersky forum:
[VfsRamFs crashes (NULL dereference in inode_destructor) under concurrent stat/readdir/unlink](https://forum.kaspersky.com/topic/vfsramfs-crashes-null-dereference-in-inode_destructor-under-concurrent-statreaddirunlink-kasperskyos-ce-140102-59792/).

## What it does

One program, `repro.Stress`, whose file system is the SDK's `precompiled_vfs::VfsRamFs`; the security policy grants
everything. In each of 20 rounds it creates 3000 files in each of `THREADS` directories under `/tmp`, then per
directory one thread `stat`s 6000 names (half never exist), one lists the directory 20 times, and one unlinks the files
(every 7th while holding it open).

## Build and run

With the SDK's CMake (`SDK` is the SDK's install directory):

```
$SDK/toolchain/bin/cmake -B build -D CMAKE_TOOLCHAIN_FILE=$SDK/toolchain/share/toolchain-aarch64-kos.cmake -D THREADS=4
$SDK/toolchain/bin/cmake --build build --target sim
```

`THREADS` (1-4, default 1) is the number of directories stressed at once; each adds three threads (stat, readdir,
unlink). The program prints `[stress] round N: ...` as it goes and `[stress] DONE` at the end.

## Results

- `THREADS=4` (13 threads in the process): VfsRamFs faulted in the first round in 3 of 3 runs.
- `THREADS=1` (4 threads, within the documented limit of 5 threads per VFS client): 2 of 2 runs completed all 20
  rounds.
