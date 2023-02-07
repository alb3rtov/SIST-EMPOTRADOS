#ifndef H_LIB_EXAMPLE_HW_H
#define H_LIB_EXAMPLE_HW_H

#define SIZE	2048
#define FILTER_LEN	8

int Run_HW_Accelerator(float in[SIZE], float out[SIZE], float p_coeffs[FILTER_LEN], u32 selop, int dma_size);

int Start_HW_Accelerator(u32 selop);

#endif
