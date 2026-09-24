/* VfsRamFs stress: in each round, create FILES files in each directory under /tmp, then per directory one thread
 * stats 2 * FILES names (half never exist), one lists the directory 20 times, and one unlinks the files (every 7th
 * while holding it open). On KasperskyOS CE 1.4.0.102 (QEMU), with THREADS=4, VfsRamFs took an unhandled page fault
 * in the first round. DIRS (build option THREADS) directories are stressed at once, 3 threads each. */
#include <dirent.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

#ifndef DIRS
#define DIRS 1
#endif
#define FILES 3000
#define ROUNDS 20

static void name(char* buf, size_t n, int dir, int file) { snprintf(buf, n, "/tmp/st%d/f%05d", dir, file); }

static void* creator(void* arg)
{
    int dir = (int)(long)arg;
    char p[64];
    for (int f = 0; f < FILES; f++)
    {
        name(p, sizeof(p), dir, f);
        int fd = open(p, O_CREAT | O_RDWR, 0600);
        if (fd >= 0) { (void)!write(fd, "x", 1); close(fd); }
    }
    return NULL;
}

static void* statter(void* arg)
{
    int dir = (int)(long)arg;
    char p[64];
    struct stat st;
    for (int f = 0; f < 2 * FILES; f++)
    {
        name(p, sizeof(p), dir, f);
        (void)stat(p, &st);
    }
    return NULL;
}

static void* lister(void* arg)
{
    char p[32];
    snprintf(p, sizeof(p), "/tmp/st%d", (int)(long)arg);
    for (int i = 0; i < 20; i++)
    {
        DIR* d = opendir(p);
        if (!d) continue;
        while (readdir(d) != NULL) { }
        closedir(d);
    }
    return NULL;
}

static void* unlinker(void* arg)
{
    int dir = (int)(long)arg;
    char p[64];
    for (int f = FILES - 1; f >= 0; f--)
    {
        name(p, sizeof(p), dir, f);
        int fd = (f % 7 == 0) ? open(p, O_RDONLY) : -1;
        (void)unlink(p);
        if (fd >= 0) close(fd);
    }
    return NULL;
}

int main(void)
{
    char p[32];
    pthread_t t[3 * DIRS];
    for (int d = 0; d < DIRS; d++) { snprintf(p, sizeof(p), "/tmp/st%d", d); mkdir(p, 0755); }
    fprintf(stderr, "[stress] %d directories, %d files, %d threads during stat/readdir/unlink\n", DIRS, FILES, 3 * DIRS);
    for (int round = 1; round <= ROUNDS; round++)
    {
        int n = 0;
        fprintf(stderr, "[stress] round %d: create\n", round);
        for (long d = 0; d < DIRS; d++) pthread_create(&t[n++], NULL, creator, (void*)d);
        for (int i = 0; i < n; i++) pthread_join(t[i], NULL);
        n = 0;
        fprintf(stderr, "[stress] round %d: stat + readdir + unlink\n", round);
        for (long d = 0; d < DIRS; d++)
        {
            pthread_create(&t[n++], NULL, statter, (void*)d);
            pthread_create(&t[n++], NULL, lister, (void*)d);
            pthread_create(&t[n++], NULL, unlinker, (void*)d);
        }
        for (int i = 0; i < n; i++) pthread_join(t[i], NULL);
    }
    fprintf(stderr, "[stress] DONE\n");
    return 0;
}
