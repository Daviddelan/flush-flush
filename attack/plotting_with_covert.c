#include <stdio.h>
#include <stdint.h>

volatile char shared[64];
volatile char sink;

static inline uint64_t rdtsc(void){
    uint64_t a,d; __asm__ __volatile__("mfence\n\t""rdtscp\n\t":"=a"(a),"=d"(d)::"rcx");
    return (d<<32)|a;
}
static inline uint64_t time_flush(void *addr){
    uint64_t s=rdtsc();
    __asm__ __volatile__("clflush (%0)"::"r"(addr):"memory");
    return rdtsc()-s;
}
static inline void touch(void *addr){ sink = *(volatile char*)addr; }
static inline void flush(void *addr){ __asm__ __volatile__("clflush (%0)"::"r"(addr):"memory"); }

// self-calibrate: build histogram, return midpoint between the two peaks
static uint64_t calibrate(void){
    enum{ N=2000 };
    static uint64_t hc[N]={0}, hu[N]={0};
    for(int i=0;i<100000;i++){
        touch((void*)shared);
        uint64_t tc=time_flush((void*)shared); if(tc>=N)tc=N-1; hc[tc]++;
        uint64_t tu=time_flush((void*)shared); if(tu>=N)tu=N-1; hu[tu]++;
    }
    int pu=0,pc=0;
    for(int c=0;c<N;c++){ if(hu[c]>hu[pu])pu=c; if(hc[c]>hc[pc])pc=c; }
    uint64_t thr=(pu+pc)/2;
    printf("[calib] uncached peak=%d  cached peak=%d  -> THRESHOLD=%lu\n", pu, pc, thr);
    return thr;
}

int main(void){
    uint64_t THRESHOLD = calibrate();

    char message[]="Hi there!";
    int len=sizeof(message)-1;
    printf("Sending:   %s\n", message);

    for(int i=0;i<len;i++){
        char c=message[i];
        char got=0;
        for(int b=0;b<8;b++){
            int bit=(c>>b)&1;
            int ones=0,zeros=0;
            for(int rep=0;rep<15;rep++){
                if(bit==1) touch((void*)shared); else flush((void*)shared);
                if(time_flush((void*)shared) > THRESHOLD) ones++; else zeros++;
            }
            int rx=(ones>zeros)?1:0;
            got|=(rx<<b);
            if(i==0) printf("  [bitdbg] letter0 bit%d: expected=%d  ones=%2d zeros=%2d  got=%d\n",
                            b, bit, ones, zeros, rx);
        }
        // nothing
        if(i==0) printf("  [bitdbg] letter0 decoded as '%c'\n", got);
    }

    // full decode print
    printf("Receiving: ");
    for(int i=0;i<len;i++){
        char c=message[i]; char got=0;
        for(int b=0;b<8;b++){
            int bit=(c>>b)&1;
            int ones=0,zeros=0;
            for(int rep=0;rep<15;rep++){
                if(bit==1) touch((void*)shared); else flush((void*)shared);
                if(time_flush((void*)shared) > THRESHOLD) ones++; else zeros++;
            }
            got|=(((ones>zeros)?1:0)<<b);
        }
        printf("%c", got);
    }
    printf("\n");
    return 0;
}