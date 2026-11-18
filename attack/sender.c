#include "ffshared.h"
#include <string.h>
int main(void){
    char *mem = map_shared(); if(!mem) return 1;
    volatile int *sender_seq = (volatile int*)(mem + OFF_SENDER_SEQ);
    volatile int *recv_seq   = (volatile int*)(mem + OFF_RECV_SEQ);
    char *data = mem + OFF_DATA;

    char payload[] = "Hi there! Flush+Flush works.";
    char frame[64]; frame[0]=(char)0xAA;               // preamble byte
    strcpy(frame+1, payload);
    int nbytes = 1 + (int)strlen(payload);
    int nbits = nbytes*8;
    printf("[sender] sending: \"%s\" (+1 preamble, %d bits)\n", payload, nbits);

    for(int i=0;i<nbits;i++){
        int byte=i/8,b=i%8;
        int bit=(frame[byte]>>b)&1;
        while(*recv_seq < i+1){
            if(bit==1) touch(data); else flush(data);
            *sender_seq = i+1;
        }
    }
    printf("[sender] done\n");
    return 0;
}
