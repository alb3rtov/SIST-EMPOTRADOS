#include <stdio.h>
#include <stdlib.h>

#include "inverse_matrix.h"


// --------------------------------------------------------
// function to be accelerated in HW. Standalone mode.

void inverse_matrix_hw(T in[DIM][DIM], T inv[DIM][DIM]) {
	#pragma HLS ARRAY_PARTITION dim=1 factor=2 block variable=in

	int i,j,k;
	int n = DIM*2;
	float ratio;
	float temp[n][n];


	// Copy in matrix in temp DIM*2xDIM*2 matrix
	L1:for(i=0;i<DIM;i++) {
		L2:for(j=0;j<DIM;j++) {
			temp[i][j] = in[i][j];
		}
	}

	// Augmenting Identity Matrix of Order n
	L3:for(i=0;i<DIM;i++) {
		L4:for(j=0;j<DIM;j++) {
			if(i==j) {
				temp[i][j+DIM] = 1;
			}
			else {
				temp[i][j+DIM] = 0;
			}
		}
	}

	// Applying Gauss Jordan Elimination
	L5:for(i=0;i<DIM;i++) {
		if(temp[i][i] == 0.0) {
			printf("Mathematical Error!");
			exit(0);
		}
		L6:for(j=0;j<DIM;j++) {
			if(i!=j) {
				ratio = temp[j][i]/temp[i][i];
				L7:for(k=0;k<n;k++) {
					temp[j][k] =temp[j][k] - ratio*temp[i][k];
				}
			}
		}
	}

	// Row Operation to Make Principal Diagonal to 1
	k = 0;
	L8:for(i=0;i<DIM;i++) {
#pragma HLS UNROLL
		L9:for(j=DIM;j<n;j++) {
			inv[i][k++] = temp[i][j]/temp[i][i];
		}
		k = 0;
	}
	return;
}


//
// --------------------------------------------------------------------
// function to be accelerated in HW wrapped with AXI4-Stream interface
void inverse_matrix_hw_wrapped (
		hls::stream<AXI_VAL> &INPUT_STREAM,
		hls::stream<AXI_VAL> &OUTPUT_STREAM)
{
#pragma HLS INTERFACE s_axilite port=return     bundle=CONTROL_BUS
#pragma HLS INTERFACE axis      port=INPUT_STREAM
#pragma HLS INTERFACE axis      port=OUTPUT_STREAM

	ap_uint<U> user;
	ap_uint<TI> id;
	ap_uint<TD> dest;

	//Instanciación de las memorias de entrada y salida (memorias locales)
	T in[DIM][DIM];
	T inv[DIM][DIM];

	// stream in first matrix
	for(int i=0; i<DIM; i++)
		for(int j=0; j<DIM; j+= NUM_ELEMS_WORD)
		{
#pragma HLS PIPELINE II=1
			WORD_MEM w = INPUT_STREAM.read().data;
			conv_t c;
			for (int k=0; k<NUM_ELEMS_WORD;k++) {
				c.in = w((k+1)*32-1,k*32);
				in[i][j+k] = c.out;
			}
		}

	// do HW inverse matrix calculation
	inverse_matrix_hw(in, inv);

	// stream out result matrix
	for(int i=0; i<DIM; i++)
		for(int j=0; j<DIM; j+= NUM_ELEMS_WORD)
		{
#pragma HLS PIPELINE II=1
			AXI_VAL e;
			conv_t c;
			WORD_MEM w;
			for (int k=0; k<NUM_ELEMS_WORD;k++) {

				c.out = inv[i][j+k];
				w((k+1)*32-1,k*32)= c.in;
			}
			//señales de control y estado del bus
			e.data = w;
			e.strb = -1;
			e.keep = 15; //e.strb;
			e.user = U;
			e.last = ((i == (DIM-1)) && (j == (DIM-NUM_ELEMS_WORD)));
			e.id = TI;
			e.dest = TD;
			OUTPUT_STREAM.write(e);
		}

	return;

}

