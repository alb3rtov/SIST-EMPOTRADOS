#include <stdio.h>
#include <stdlib.h>

#include "inverse_matrix.h"

void convert2array(WORD_MEM a, T out[NUM_ELEMS_WORD]);
void convert2word(WORD_MEM &a, T out[NUM_ELEMS_WORD]);

/* Inverse matrix software implementation */
void inverse_matrix_sw(T in[DIM][DIM], T inv[DIM][DIM]) {
	int i,j,k;
	int n = DIM*2;
	float ratio;
	float temp[n][n];

	// Copy matrix input matrix in a DIM*2 order matrix
	for(i=0;i<DIM;i++) {
		for(j=0;j<DIM;j++) {
			temp[i][j] = in[i][j];
		}
	}

	// Augmenting Identity Matrix of Order n
	for(i=0;i<DIM;i++) {
		for(j=0;j<DIM;j++) {
			if(i==j) {
				temp[i][j+DIM] = 1;
			}
			else {
				temp[i][j+DIM] = 0;
			}
		}
	}

	// Applying Gauss Jordan Elimination
	for(i=0;i<DIM;i++) {
		if(temp[i][i] == 0.0) {
			printf("Mathematical Error!");
			exit(0);
		}
		for(j=0;j<DIM;j++) {
			if(i!=j) {
				ratio = temp[j][i]/temp[i][i];
				for(k=0;k<n;k++) {
					temp[j][k] =temp[j][k] - ratio*temp[i][k];
				}
			}
		}
	}

	// Row Operation to Make Principal Diagonal to 1
	k = 0;
	for(i=0;i<DIM;i++) {
		for(j=DIM;j<n;j++) {
			inv[i][k++] = temp[i][j]/temp[i][i];
		}
		k = 0;
	}
	return;
}


int main_standalone(void)
{
	int i,j,k, err;

	T in[DIM][DIM] = {
			{5,3,7,5,1,8,9,4,3,4},
			{6,1,5,4,8,1,1,5,1,5},
			{4,5,3,1,6,8,7,9,8,4},
			{9,6,8,4,7,5,2,6,9,8},
			{7,5,1,5,6,4,9,4,6,4},
			{6,8,4,6,1,8,1,8,4,8},
			{1,2,8,8,7,9,7,9,6,4},
			{9,3,2,6,4,5,9,8,7,5},
			{6,4,8,9,1,3,6,4,9,4},
			{8,4,9,9,3,6,4,8,4,5}
	};

	T inv_sw[DIM][DIM];
	T inv_hw[DIM][DIM];

	printf("NORMAL MODE\r\n");
	printf("Hardware implementation\r\n");
	inverse_matrix_hw(in, inv_hw);
	printf("Software implementation\r\n");
	inverse_matrix_sw(in, inv_sw);


	/** Matrix comparison */
	err = 0;
	for (i = 0; (i<DIM && !err); i++)
		for (j = 0; (j<DIM && !err); j++)
			if (inv_sw[i][j] != inv_hw[i][j])
				err++;

	/*
	for (i = 0; i < DIM; i++) {
		for (j = 0; j < DIM; j++) {
			printf("%f ",inv_sw[i][j]);
		}
		printf("\r\n");
	}*/

	if (err == 0)
		printf("Matrixes identical ... Test successful!\r\n");
	else
		printf("Test failed!\r\n");

	return err;

}


/* Test inverse_matrix_wrapped function */
int main_axi(void)
{
	int i,j, err;

	ap_uint<U> user;
	ap_uint<TI> id;
	ap_uint<TD> dest;



	T in[DIM][DIM] = {
			{5,3,7,5,1,8,9,4,3,4},
			{6,1,5,4,8,1,1,5,1,5},
			{4,5,3,1,6,8,7,9,8,4},
			{9,6,8,4,7,5,2,6,9,8},
			{7,5,1,5,6,4,9,4,6,4},
			{6,8,4,6,1,8,1,8,4,8},
			{1,2,8,8,7,9,7,9,6,4},
			{9,3,2,6,4,5,9,8,7,5},
			{6,4,8,9,1,3,6,4,9,4},
			{8,4,9,9,3,6,4,8,4,5}
	};

	T inv_sw[DIM][DIM];
	T inv_hw[DIM][DIM];



	//printf("DEBUGGING AXI4 STREAMING DATA TYPES!\r\n");

	// prepare data for the DUT
	hls::stream<AXI_VAL> inp_stream("INSW");
	hls::stream<AXI_VAL> out_stream("OUTSW");

	AXI_VAL e;

	// stream in the first input  matrix
	for(int i=0; i<DIM; i++)
		for(int j=0; j<DIM; j+=NUM_ELEMS_WORD)
		{

			convert2word(e.data,&in[i][j]);

			e.strb = -1;
			e.keep = 15; //e.strb;
			e.user = 0;
			e.last = 0;
			e.id = 1;
			e.dest = 2;

			inp_stream.write(e);

		}

	//call the DUT
	inverse_matrix_hw_wrapped(inp_stream, out_stream);

	// extract the output matrix from the out stream
	for(int i=0; i<DIM; i++)
		for(int j=0; j<DIM; j+=NUM_ELEMS_WORD)
		{


			e = out_stream.read();
			convert2array(e.data,&inv_hw[i][j]);

			//printf("AXIS Control signals: %d\n",e.last.to_bool());
		}

	/* reference of Inverse Matrix */
	inverse_matrix_sw(in, inv_sw);

	/** Matrix comparison */
	err = 0;
	for (i = 0; (i<DIM); i++) {
		for (j = 0; (j<DIM); j++) {
			if (inv_sw[i][j] != inv_hw[i][j]) {
				err++;
				printf("Error (%d,%d) %f %f\r\n",i,j,inv_sw[i][j],inv_hw[i][j]);

			}
		}
	}

	/*
	for (i = 0; i < DIM; i++) {
		for (j = 0; j < DIM; j++) {
			printf("%f ",inv_sw[i][j]);
		}
		printf("\r\n");
	}*/

	if (err == 0)
		printf("Matrixes identical ... Test successful!\r\n");
	else
		printf("Test failed! (%d)\r\n",err);

	return err;
}

int main(void) {
	//return main_standalone();
	return main_axi();
}


void convert2array(WORD_MEM w, T out[NUM_ELEMS_WORD]) {
	conv_t c;

	convert2array_label1:for (int k=0; k<NUM_ELEMS_WORD;k++) {

		c.in = w((k+1)*32-1,k*32);
		//
		out[k] = c.out;
	}

}
void convert2word(WORD_MEM &w, T in[NUM_ELEMS_WORD]) {
	conv_t c;
	WORD_MEM _w;

	convert2word_label0:for (int k=0; k<NUM_ELEMS_WORD;k++) {
		c.out = in[k];
		_w((k+1)*32-1,k*32)= c.in;
		//		_w((NUM_ELEMS_WORD-(k))*32-1,(NUM_ELEMS_WORD-(k+1))*32)= c.in;
	}
	w = _w;
}

