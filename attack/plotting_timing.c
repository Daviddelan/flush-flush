#include <stdio.h>
#include <stdint.h>

#define SAMPLES 100000
#define MAXCYC  2000

volatile char target[64];

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

int main(void) {
    static uint64_t hist_cached[MAXCYC]   = {0};
    static uint64_t hist_uncached[MAXCYC] = {0};

    for (int i = 0; i < SAMPLES; i++) {
        target[0] = 1;
        uint64_t t = time_flush((void *)target);
        if (t >= MAXCYC) t = MAXCYC - 1;
        hist_cached[t]++;
    }
    for (int i = 0; i < SAMPLES; i++) {
        time_flush((void *)target);
        uint64_t t = time_flush((void *)target);
        if (t >= MAXCYC) t = MAXCYC - 1;
        hist_uncached[t]++;
    }

    int peak_uncached = 0, peak_cached = 0;
    for (int c = 0; c < MAXCYC; c++) {
        if (hist_uncached[c] > hist_uncached[peak_uncached]) peak_uncached = c;
        if (hist_cached[c]   > hist_cached[peak_cached])     peak_cached   = c;
    }

    // old method: lowest point between the peaks (can sit too close to a hump)
    int lo = peak_uncached, hi = peak_cached;
    int valley = lo;
    uint64_t smallest = (uint64_t)-1;
    for (int c = lo; c <= hi; c++) {
        if (hist_cached[c] + hist_uncached[c] < smallest) {
            smallest = hist_cached[c] + hist_uncached[c];
            valley = c;
        }
    }

    // better method: halfway between the two peaks (safe margin on both sides)
    int midpoint = (peak_uncached + peak_cached) / 2;

    printf("uncached peak at %d cycles\n", peak_uncached);
    printf("cached   peak at %d cycles\n", peak_cached);
    printf("valley (lowest point)      = %d cycles   [can be too close to a hump]\n", valley);
    printf("midpoint between peaks     = %d cycles   <-- RECOMMENDED threshold\n\n", midpoint);
    printf(">>> Use THRESHOLD = %d in your covert channel.\n\n", midpoint);

    printf("cycles | uncached        | cached\n");
    for (int c = lo - 20; c <= hi + 20; c++) {
        if (c < 0) continue;
        if (hist_uncached[c] == 0 && hist_cached[c] == 0) continue;
        printf("%5d  | u:%-8lu | c:%-8lu\n", c, hist_uncached[c], hist_cached[c]);
    }
    return 0;
}