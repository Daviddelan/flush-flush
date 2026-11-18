// Stealth measurement using hardware performance counters.
// Counts real CACHE MISSES for a Flush+Reload spy vs a Flush+Flush spy.
// Works on most real hardware; may be blocked inside VMs/sandboxes.
//
// Build: gcc -O2 -o stealth_perf stealth_perf.c
// Run:   ./stealth_perf
// If it prints "perf_event_open failed", the machine blocks counters.
// Try:   sudo sysctl kernel.perf_event_paranoid=1     then rerun.

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <linux/perf_event.h>

volatile char target[64];
volatile char sink;

static inline void flush(void *p){ __asm__ __volatile__("clflush (%0)"::"r"(p):"memory"); }

// open a hardware counter for the given event; returns a file descriptor
static long open_counter(uint64_t config){
    struct perf_event_attr pe;
    memset(&pe, 0, sizeof(pe));
    pe.type   = PERF_TYPE_HARDWARE;
    pe.size   = sizeof(pe);
    pe.config = config;                 // PERF_COUNT_HW_CACHE_MISSES
    pe.disabled = 1;
    pe.exclude_kernel = 1;
    pe.exclude_hv = 1;
    return syscall(__NR_perf_event_open, &pe, 0, -1, -1, 0);
}
static uint64_t read_counter(long fd){
    uint64_t v = 0;
    if (read(fd, &v, sizeof(v)) != sizeof(v)) return 0;
    return v;
}

#define ROUNDS 1000000

// run one spy loop while counting cache misses; mode 0 = Flush+Reload, 1 = Flush+Flush
static uint64_t measure(long fd, int mode){
    ioctl(fd, PERF_EVENT_IOC_RESET, 0);
    ioctl(fd, PERF_EVENT_IOC_ENABLE, 0);
    for (long i = 0; i < ROUNDS; i++){
        flush((void*)target);
        if (mode == 0) sink = target[0];       // Flush+Reload: real memory load
        else           flush((void*)target);   // Flush+Flush: just another flush
    }
    ioctl(fd, PERF_EVENT_IOC_DISABLE, 0);
    return read_counter(fd);
}

int main(void){
    long fd = open_counter(PERF_COUNT_HW_CACHE_MISSES);
    if (fd < 0){ perror("perf_event_open failed"); return 1; }

    uint64_t fr_misses = measure(fd, 0);
    uint64_t ff_misses = measure(fd, 1);

    printf("Cache misses over %d rounds each:\n", ROUNDS);
    printf("  Flush+Reload: %lu\n", fr_misses);
    printf("  Flush+Flush : %lu\n", ff_misses);
    if (ff_misses > 0)
        printf("\nFlush+Reload caused %.1fx more cache misses than Flush+Flush.\n",
               (double)fr_misses / (double)ff_misses);
    return 0;
}