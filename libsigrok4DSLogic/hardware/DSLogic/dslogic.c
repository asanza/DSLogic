#include "dslogic.h"
#include <stdint.h>
#include <stdio.h>
#include <libsigrok.h>
#include <libsigrok-internal.h>

#ifndef _WIN32
#undef min
#define min(a,b) ((a)<(b)?(a):(b))
#endif

#define FPGA_RLE_LEN   0xffff  //This is how long currently our RLE buffer in the FPGA
                   //The capture waits untill we get that many RLE samples.
                   //Note that this may be a lot of time to get that many RLE
                   //for some measured signals. This may be increase to 16M
                   //which means hours of recording
//The current approach is more a proofe of concept

SR_PRIV void rle_decode(uint16_t *data_rle, unsigned long len_rle, uint16_t *data, unsigned long len){
    unsigned long first_sample_idx, i,j, k;
    uint16_t count;
    //Sckip to the first sample
    first_sample_idx=0;
    while(data_rle[first_sample_idx++] & 0x8000);

    j=0;
    for(i=first_sample_idx-1; (j<len) && (i < min(len_rle, FPGA_RLE_LEN)); i++){
                if(data_rle[i] & 0x8000){//Count
            count=data_rle[i]&0x7fff;
                        for(k=0; k<count ;k++){
                data[j++]=data_rle[i-1];
                if(j>=len) break;
            }
        } else{		      //Sample
            data[j++]=data_rle[i];
        }

    }
}
