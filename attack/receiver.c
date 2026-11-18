#include "ffshared.h"
#include <string.h>

static uint64_t calibrate(char *probe){
    enum { N=2000 }; static uint64_t hc[N]={0},hu[N]={0};
    for(int i=0;i<100000;i++){
        touch(probe);
        uint64_t tc=time_flush(probe); if(tc>=N)tc=N-1; hc[tc]++;
        uint64_t tu=time_flush(probe); if(tu>=N)tu=N-1; hu[tu]++;
    }
    int pu=0,pc=0;
    for(int c=0;c<N;c++){ if(hu[c]>hu[pu])pu=c; if(hc[c]>hc[pc])pc=c; }
    uint64_t thr=(uint64_t)(pu+pc)/2;
    fprintf(stderr,"[recv] calib uncached=%d cached=%d THRESHOLD=%lu\n",pu,pc,thr);
    return thr;
}

int main(void){
    char *mem = map_shared(); if(!mem) return 1;
    volatile int *sender_seq=(volatile int*)(mem+OFF_SENDER_SEQ);
    volatile int *recv_seq  =(volatile int*)(mem+OFF_RECV_SEQ);
    char *data = mem + OFF_DATA;
    uint64_t THRESHOLD = calibrate(data);

    char out[256]; int nbytes=1+28; int nbits=nbytes*8;
    memset(out,0,sizeof(out));
    for(int i=0;i<nbits;i++){
        while(*sender_seq < i+1){ }
        // majority vote; the sender re-caches between our samples (two-program race)
        int ones=0,zeros=0;
        for(int rep=0; rep<11; rep++){
            if(time_flush(data) > THRESHOLD) ones++; else zeros++;
        }
        int bit = (ones>zeros) ? 1 : 0;
        out[i/8] |= (bit << (i%8));
        *recv_seq = i+1;
    }
    printf("[receiver] got: \"%s\"\n", out+1);
    return 0;
}
