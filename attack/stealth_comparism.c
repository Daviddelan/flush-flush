#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

volatile char target[64];
volatile char sink;

static inline uint64_t rdtsc(void){
    uint64_t a,d; __asm__ __volatile__("mfence\n\t""rdtscp\n\t":"=a"(a),"=d"(d)::"rcx");
    return (d<<32)|a;
}
static inline void flush(void *p){ __asm__ __volatile__("clflush (%0)"::"r"(p):"memory"); }

#define ROUNDS 1000000

int main(void){
    // ---- Flush+Reload spy loop ----
    // each round: flush, then RELOAD (a real memory access), then time the reload
    uint64_t fr_loads = 0, t0 = rdtsc();
    for(long i=0;i<ROUNDS;i++){
        flush((void*)target);
        uint64_t s = rdtsc();
        sink = target[0];          // <-- RELOAD: a real memory access every round
        uint64_t e = rdtsc();
        (void)(e-s);
        fr_loads++;                // count the memory access
    }
    uint64_t fr_time = rdtsc() - t0;

    // ---- Flush+Flush spy loop ----
    // each round: flush, then time a second FLUSH. NO memory load at all.
    uint64_t ff_loads = 0, t1 = rdtsc();
    for(long i=0;i<ROUNDS;i++){
        flush((void*)target);
        uint64_t s = rdtsc();
        flush((void*)target);      // <-- just a flush, NOT a memory load
        uint64_t e = rdtsc();
        (void)(e-s);
        // ff_loads stays 0: no memory access performed
    }
    uint64_t ff_time = rdtsc() - t1;

    printf("Over %d rounds each:\n\n", ROUNDS);
    printf("                     memory loads   total cycles\n");
    printf("Flush+Reload spy  |  %10lu   | %12lu\n", fr_loads, fr_time);
    printf("Flush+Flush  spy  |  %10lu   | %12lu\n", ff_loads, ff_time);
    printf("\nFlush+Reload does %lu memory loads; Flush+Flush does %lu.\n", fr_loads, ff_loads);
    printf("Those loads are what a cache-miss detector sees.\n");
    return 0;
}