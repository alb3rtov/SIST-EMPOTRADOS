#include <stdio.h>
#include <stdlib.h>

#include "fir.h"

void convert2array(WORD_MEM a, T out[NUM_ELEMS_WORD]);
void convert2word(WORD_MEM &a, T out[NUM_ELEMS_WORD]);
void generate_coeffs(T p_coeffs[FILTER_LEN]);

void fir_sw(T p_coeffs[FILTER_LEN], T p_in[SIZE], T p_out[SIZE]) {
  int i, j, k;
  T tmp;

  for (k = 0; k < SIZE; k++) {
    tmp = 0;
    for (i = 0; i < FILTER_LEN; i++){
      j = k-i;
      if (j >= 0) {
        tmp += p_coeffs[i] * p_in[j];
      }
    }
    p_out[k] = tmp;
  }
}

void generate_coeffs(T p_coeffs[FILTER_LEN]) {
    srand((unsigned int)time(NULL));
    for (int i = 0; i < FILTER_LEN/2; i++) {
        p_coeffs[i] = float(rand())/float((RAND_MAX)) * 1;
    }
    int j = (FILTER_LEN/2)-1;
    for (int i = FILTER_LEN/2; i < FILTER_LEN; i++) {
        p_coeffs[i] = p_coeffs[j];
        j--;
    }
}

int main_standalone(void) {
  int i, j;
  T p_in[SIZE];
  T p_out_sw[SIZE];
  T p_out_hw[SIZE];
  T p_coeffs[FILTER_LEN];

  generate_coeffs(p_coeffs);

  /*
  T p_coeffs[FILTER_LEN] = {
     0.0181163,  0.0087615,  0.0148049, -0.0132676,
	-0.0187804,  0.0006382,  0.0250536,  0.0299817,
	 0.0002609, -0.0345546, -0.0395620,  0.0000135,
	-0.0508558, -0.0736313, -0.0529380,  0.0540186,
	 0.0540186, -0.0529380, -0.0736313, -0.0508558,
	 0.0000135, -0.0395620, -0.0345546,  0.0002609,
	 0.0299817,  0.0250536,  0.0006382, -0.0187804,
	-0.0132676,  0.0148049,  0.0087615,  0.0181163,
  };*/


  for (i = 0; i < SIZE; i++) {
    p_in[i] = ((float)rand()/(float)(RAND_MAX));
  }

  printf("Software implementation\r\n");
  fir_sw(p_coeffs, p_in, p_out_sw);

  printf("Hardware implementation\r\n");
  fir_hw(p_coeffs, p_in, p_out_hw);

  int err = 0;
	for (j = 0; (j < SIZE && !err); j++)
		if (p_out_sw[j] != p_out_hw[j])
			err++;

	if (err == 0)
		printf("Vectors identical ... Test successful!\r\n");
	else
		printf("Test failed!\r\n");

  /*
  for (j = 0; j < FILTER_LEN; j++) {
    printf("Value [%d]: %f\r\n", j, p_out_sw[j]);
  }
  */
  return 0;
}

int main_axi(void) {
	int i,j, err;

	ap_uint<U> user;
	ap_uint<TI> id;
	ap_uint<TD> dest;

	T matOp1[SIZE];
	T matOp2[SIZE];
	T firFilter_sw[SIZE];
	T firFilter_hw[SIZE];

	/** Matrix Initiation */
		for(j = 0; j<SIZE; j++)
			matOp1[j] = (float)(j+1);

		for(j = 0; j<SIZE; j++)
			matOp2[j] = (float)(j+2);
	/** End of Initiation */


	//printf("DEBUGGING AXI4 STREAMING DATA TYPES!\r\n");

	// prepare data for the DUT
	hls::stream<AXI_VAL> inp_stream("INSW");
	hls::stream<AXI_VAL> out_stream("OUTSW");

	AXI_VAL e;

	// stream in the first input  matrix

		for(int j=0; j<SIZE; j+=NUM_ELEMS_WORD)
		{
			convert2word(e.data,&matOp1[j]);

			e.strb = -1;
			e.keep = 15; //e.strb;
			e.user = 0;
			e.last = 0;
			e.id = 1;
			e.dest = 2;

			inp_stream.write(e);

		}

	// stream in the second input  matrix

		for(int j=0; j<SIZE; j+=NUM_ELEMS_WORD)
		{

			convert2word(e.data,&matOp2[j]);
			e.strb = -1;
			e.keep = 15; //e.strb;
			e.user = 0;
			e.last =(j==(SIZE-NUM_ELEMS_WORD));
			e.id = 2;
			e.dest = 3;

			inp_stream.write(e);

		}

	//call the DUT
	fir_hw_wrapped(inp_stream, out_stream);

	// extract the output matrix from the out stream

		for(int j=0; j<SIZE; j+=NUM_ELEMS_WORD)
		{
			e = out_stream.read();
			convert2array(e.data,&firFilter_hw[j]);

			//printf("AXIS Control signals: %d\n",e.last.to_bool());
		}


	/* reference Matrix Multiplication */
	fir_sw(matOp1, matOp2, firFilter_sw);

	/** Matrix comparison */
	err = 0;

		for (j = 0; (j<SIZE); j++) {
			if (firFilter_sw[j] != firFilter_hw[j]) {
				err++;
				printf("Error (%d) %f %f\r\n",j,firFilter_sw[j],firFilter_hw[j]);

			}
		}


	if (err == 0)
		printf("Vectors identical ... Test successful!\r\n");
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

