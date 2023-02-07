
#ifndef H_LIB_EXAMPLE_HW_H
#define H_LIB_EXAMPLE_HW_H

#define DIM	10

int Run_HW_Accelerator(float in[DIM][DIM], float inv_hw[DIM][DIM], int dma_size);

int Start_HW_Accelerator(void);

#endif
