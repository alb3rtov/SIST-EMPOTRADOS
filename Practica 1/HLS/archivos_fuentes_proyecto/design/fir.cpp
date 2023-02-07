#include <stdio.h>
#include <stdlib.h>

#include "fir.h"

void fir_hw(T p_coeffs[FILTER_LEN], T p_in[SIZE], T p_out[SIZE]) {
#pragma HLS ARRAY_PARTITION dim=1 factor=2 type=block variable=p_coeffs
#pragma HLS ARRAY_PARTITION dim=1 factor=2 type=block variable=p_in
  int i, j, k;
  T tmp;

  L1:for (k = 0; k < SIZE; k++) {
    tmp = 0;
    L2:for (i = 0; i < FILTER_LEN; i++){
      j = k-i;
      if (j >= 0) {
        tmp += p_coeffs[i] * p_in[j];
      }
    }
    p_out[k] = tmp;
  }
  return;
}


void fir_hw_wrapped(
		hls::stream<AXI_VAL> &INPUT_STREAM,
		hls::stream<AXI_VAL> &OUTPUT_STREAM)
{

#pragma HLS INTERFACE s_axilite port=return     bundle=CONTROL_BUS
#pragma HLS INTERFACE axis      port=INPUT_STREAM
#pragma HLS INTERFACE axis      port=OUTPUT_STREAM

	ap_uint<U> user;
	ap_uint<TI> id;
	ap_uint<TD> dest;

	T a[SIZE];
	T b[SIZE];
	T out[SIZE];

	// stream in first matrix

	for(int j=0; j<SIZE; j+= NUM_ELEMS_WORD)
		{
#pragma HLS PIPELINE II=1
			WORD_MEM w = INPUT_STREAM.read().data;
			conv_t c;
			for (int k=0; k<NUM_ELEMS_WORD;k++) {
				c.in = w((k+1)*32-1,k*32);
				a[j+k] = c.out;
			}
		}

	// stream in second matrix
		for(int j=0; j<SIZE; j+= NUM_ELEMS_WORD)
		{
#pragma HLS PIPELINE II=1
			WORD_MEM w = INPUT_STREAM.read().data;
			conv_t c;
			for (int k=0; k<NUM_ELEMS_WORD;k++) {
				c.in = w((k+1)*32-1,k*32);
				b[j+k] = c.out;
			}

		}

	fir_hw(a, b , out);

	// stream out result matrix
		for(int j=0; j<SIZE; j+= NUM_ELEMS_WORD)
		{
#pragma HLS PIPELINE II=1
			AXI_VAL e;
			conv_t c;
			WORD_MEM w;
			for (int k=0; k<NUM_ELEMS_WORD;k++) {

				c.out = out[j+k];
				w((k+1)*32-1,k*32)= c.in;
			}
			//señales de control y estado del bus
			e.data = w;
			e.strb = -1;
			e.keep = 15; //e.strb;
			e.user = U;
			e.last = (j == (SIZE-NUM_ELEMS_WORD));
			e.id = TI;
			e.dest = TD;
			OUTPUT_STREAM.write(e);
		}

	return;

}
