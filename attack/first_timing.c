#include <stdio.h>      
#include <stdint.h>     

volatile char target[64];   // the cache line that us going to be experimented on

//A function that is going to help read the CPU's cycle counter 
// static inline uint64_t rdtsc(void) {
//     uint64_t a, d;
//     __asm__ __volatile__("rdtscp" : "=a"(a), "=d"(d) :: "rcx");  // just read clock
//     return (d << 32) | a;
// }
//Without fencing leads to a blur and the cycles are all a fixed figure

//A function that is going to help read the CPU's cycle counter aand has the fencing
static inline uint64_t rdtsc(void) {
    uint64_t a, d;
    __asm__ __volatile__("mfence\n\t"   
                         "rdtscp\n\t"   
                         : "=a"(a), "=d"(d) :: "rcx");
    return (d << 32) | a; 
}

//flush address 
static inline uint64_t time_flush(void *addr) {
    uint64_t start = rdtsc(); //the clock before
    __asm__ __volatile__("clflush (%0)" :: "r"(addr) : "memory"); // the flush operation
    uint64_t end   = rdtsc(); //the clock after
    return end - start; //returning how many cycles the flush took
}

int main(void) {
    printf("run | cached | uncached\n"); //just for making the result into a table form
    printf("----+--------+---------\n"); //just to make it more appealing

    for (int i = 0; i < 15; i++) {
        target[0] = 1; //accessing the memory, and it immediately goes into the cache
        uint64_t t_cached = time_flush((void *)target); //flushing it then timing it 
        //The time is supposed to be slow because there is already info in the cache

        // UNCACHED case:
        uint64_t t_uncached = time_flush((void *)target); //flushing again 
        //It is supposed to be faster now because there is no info in the cache

        printf(" %2d | %5lu  | %6lu\n", i, t_cached, t_uncached);
    }
    return 0; // finished, success
}