#ifndef FFSHARED_H
#define FFSHARED_H
#include <stdio.h>
#include <stdint.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

#define SHARED_FILE "ff_channel.bin"

// control counters live at the start of the page (direct-write clock)
// the DATA cache line lives far away so it is its own cache line
#define OFF_SENDER_SEQ 0       // int: sender announces "on bit i+1"
#define OFF_RECV_SEQ   64       // int: receiver acks "got bit i+1"  (separate line)
#define OFF_DATA       1024     // the cache line that carries the secret bit

static char *map_shared(void) {
    int fd = open(SHARED_FILE, O_RDWR);
    if (fd < 0) { perror("open"); return NULL; }
    char *p = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);
    if (p == MAP_FAILED) { perror("mmap"); return NULL; }
    return p;
}
static inline uint64_t rdtsc(void) {
    uint64_t a, d;
    __asm__ __volatile__("mfence\n\t" "rdtscp\n\t" : "=a"(a), "=d"(d) :: "rcx");
    return (d << 32) | a;
}
static inline uint64_t time_flush(void *addr) {
    uint64_t s = rdtsc();
    __asm__ __volatile__("clflush (%0)" :: "r"(addr) : "memory");
    return rdtsc() - s;
}
static inline void touch(void *addr){ static volatile char sink; sink = *(volatile char*)addr; }
static inline void flush(void *addr){ __asm__ __volatile__("clflush (%0)"::"r"(addr):"memory"); }
#endif
