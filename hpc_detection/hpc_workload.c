// Generate Flush+Reload and Flush+Flush workloads for HPC analysis.
// The workloads are used to compare hardware performance counter behaviour.

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

volatile char target[64];
volatile char sink;

static inline uint64_t rdtsc(void){
    uint64_t a,d; __asm__ __volatile__("mfence\n\t""rdtscp\n\t":"=a"(a),"=d"(d)::"rcx");
    return (d<<32)|a;
}
static inline void flush(void *p){ __asm__ __volatile__("clflush (%0)"::"r"(p):"memory"); }

#define ROUNDS 1000000
#define DURATION_SECONDS 20

int main(int argc, char *argv[]){

    if(argc != 2){
        fprintf(stderr, "Usage: %s <fr|ff>\n", argv[0]);
        return 1;
    }
    if(strcmp(argv[1], "fr") !=0 && strcmp(argv[1], "ff") != 0){
        fprintf(stderr, "Invalid mode, Choose fr or ff.\n");
        return 1;
    }

    if(strcmp(argv[1], "fr") == 0){
        // ---- Flush+Reload workload loop ----
        // each round: flush, then RELOAD (a real memory access), then time the reload
        uint64_t fr_loads = 0, t0 = rdtsc();

        time_t begin = time(NULL);
        while(time(NULL) - begin < DURATION_SECONDS){
            for(long i=0;i<ROUNDS;i++){
                flush((void*)target);
                uint64_t s = rdtsc();
                sink = target[0];          // <-- RELOAD: a real memory access every round
                uint64_t e = rdtsc();
                (void)(e-s);
                fr_loads++;                // count the memory access
            }
        }
            uint64_t fr_time = rdtsc() - t0;

            printf("Batch size: %d rounds\n", ROUNDS);
            printf("                     memory loads   elapsed TSC cycles\n");
            printf("Flush+Reload workload  |  %10lu   | %12lu\n", fr_loads, fr_time);
    
    }else{
        // ---- Flush+Flush workelo loop ----
        // each round: flush, then time a second FLUSH. NO explicit target data load.
        uint64_t ff_loads = 0, t1 = rdtsc();

        time_t begin = time(NULL);
        while(time(NULL) - begin < DURATION_SECONDS){
            for(long i=0;i<ROUNDS;i++){
                flush((void*)target);
                uint64_t s = rdtsc();
                flush((void*)target);      // <-- just a flush, NOT a memory load
                uint64_t e = rdtsc();
                (void)(e-s);
        }}
            uint64_t ff_time = rdtsc() - t1;

            printf("                     memory loads   elapsed TSC cycles\n");
            printf("Flush+Flush  workload  |  %10lu   | %12lu\n", ff_loads, ff_time);
        
        }
    return 0;
}