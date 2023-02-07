/******************************************************************************
*
* Copyright (C) 2009 - 2014 Xilinx, Inc.  All rights reserved.
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* Use of the Software is limited solely to applications:
* (a) running on a Xilinx device, or
* (b) that interact with a Xilinx device through a bus or interconnect.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
* XILINX  BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
* WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF
* OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*
* Except as contained in this notice, the name of the Xilinx shall not be used
* in advertising or otherwise to promote the sale, use or other dealings in
* this Software without prior written authorization from Xilinx.
*
******************************************************************************/

/*
 * helloworld.c: simple test application
 *
 * This application configures UART 16550 to baud rate 9600.
 * PS7 UART (Zynq) is not initialized by this application, since
 * bootrom/bsp configures it to baud rate 115200
 *
 * ------------------------------------------------
 * | UART TYPE   BAUD RATE                        |
 * ------------------------------------------------
 *   uartns550   9600
 *   uartlite    Configurable only in HW design
 *   ps7_uart    115200 (configured by bootrom/bsp)
 */

#include <stdio.h>
#include <stdlib.h>
#include "platform.h"
#include "xparameters.h"

#include "lib_xinverse_matrix_hw.h"
#include "xtmrctr.h"
#include "xaxidma.h"

#define NUM_TESTS 1024

void inverse_matrix_ref(float in[DIM][DIM], float inv[DIM][DIM]);
// TIMER Instance
XTmrCtr timer_dev;

// AXI DMA Instance
XAxiDma AxiDma;


int init_dma(){
	XAxiDma_Config *CfgPtr;
	int status;

	CfgPtr = XAxiDma_LookupConfig( (XPAR_AXI_DMA_0_DEVICE_ID) );
	if(!CfgPtr){
		print("Error looking for AXI DMA config\n\r");
		return XST_FAILURE;
	}
	status = XAxiDma_CfgInitialize(&AxiDma,CfgPtr);
	if(status != XST_SUCCESS){
		print("Error initializing DMA\n\r");
		return XST_FAILURE;
	}
	//check for scatter gather mode
	if(XAxiDma_HasSg(&AxiDma)){
		print("Error DMA configured in SG mode\n\r");
		return XST_FAILURE;
	}
	/* Disable interrupts, we use polling mode */
	XAxiDma_IntrDisable(&AxiDma, XAXIDMA_IRQ_ALL_MASK, XAXIDMA_DEVICE_TO_DMA);
	XAxiDma_IntrDisable(&AxiDma, XAXIDMA_IRQ_ALL_MASK, XAXIDMA_DMA_TO_DEVICE);

	// Reset DMA
	XAxiDma_Reset(&AxiDma);
	while (!XAxiDma_ResetIsDone(&AxiDma)) {}

	return XST_SUCCESS;
}


int main(int argc, char **argv)
{
	int i, j;
	int err=0;
	int status;

	float _Alignas(16) in[DIM][DIM] = {
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

	float _Alignas(16) inv_hw[DIM][DIM];
	float _Alignas(16) inv_sw[DIM][DIM];

	unsigned int dma_size = DIM * DIM * sizeof(float);

    float acc_factor;
	unsigned int init_time, curr_time, calibration;
	unsigned int begin_time;
	unsigned int end_time;
	unsigned int run_time_sw = 0;
	unsigned int run_time_hw = 0;

	init_platform();

	xil_printf("***************************************************************\r\n");
	xil_printf("  Alberto Vazquez Martinez \r\n");
	xil_printf("  FP NxN INVERSE MATRIX CALCULATOR -> AXI DMA -> ARM HPM    \r\n");
	xil_printf("  Xilinx Version -> Vivado + Vitis HLS + Vitis 2021.1 \r\n");
	xil_printf("***************************************************************\r\n");


	/* ******************************************************************************* */
	// Init DMA
	status = init_dma();
	if(status != XST_SUCCESS){
		print("\rError: DMA init failed\n");
		return XST_FAILURE;
	}
	print("\nDMA Init done\n");

	/* ******************************************************************************* */
	// Setup HW timer
	status = XTmrCtr_Initialize(&timer_dev, XPAR_AXI_TIMER_0_DEVICE_ID);
	if(status != XST_SUCCESS)
	{
		print("Error: timer setup failed\n");
		//return XST_FAILURE;
	}
	XTmrCtr_SetOptions(&timer_dev, XPAR_AXI_TIMER_0_DEVICE_ID, XTC_ENABLE_ALL_OPTION);

	// Calibrate HW timer
	XTmrCtr_Reset(&timer_dev, XPAR_AXI_TIMER_0_DEVICE_ID);
	init_time = XTmrCtr_GetValue(&timer_dev, XPAR_AXI_TIMER_0_DEVICE_ID);
	curr_time = XTmrCtr_GetValue(&timer_dev, XPAR_AXI_TIMER_0_DEVICE_ID);
	calibration = curr_time - init_time;

	// Loop measurement
	XTmrCtr_Reset(&timer_dev, XPAR_AXI_TIMER_0_DEVICE_ID);
	begin_time = XTmrCtr_GetValue(&timer_dev, XPAR_AXI_TIMER_0_DEVICE_ID);
	for (i = 0; i< NUM_TESTS; i++);
	end_time = XTmrCtr_GetValue(&timer_dev, XPAR_AXI_TIMER_0_DEVICE_ID);
	run_time_sw = end_time - begin_time - calibration;
	xil_printf("Loop time for %d iterations is %d cycles\n", NUM_TESTS, run_time_sw);

	/* ******************************************************************************* */
	// input data Initiation
	for(i = 0; i<DIM; i++)
		for(j = 0; j<DIM; j++)
		{
			inv_sw[i][j] = (float)0.0f;
			inv_hw[i][j] = (float)0.0f;
		}
	/** End of Initiation */


		/* ******************************************************************************* */
		// call the software version of the function
		xil_printf("Running Matrix Inverse in SW\n");
		XTmrCtr_Reset(&timer_dev, XPAR_AXI_TIMER_0_DEVICE_ID);
		begin_time = XTmrCtr_GetValue(&timer_dev, XPAR_AXI_TIMER_0_DEVICE_ID);
		for (i = 0; i < NUM_TESTS; i++)
		{
			inverse_matrix_ref(in, inv_sw);
		}
		end_time = XTmrCtr_GetValue(&timer_dev, XPAR_AXI_TIMER_0_DEVICE_ID);
		run_time_sw = end_time - begin_time - calibration;
		xil_printf("\nTotal run time for SW on Processor is %d cycles over %d tests.\n",
				run_time_sw/NUM_TESTS, NUM_TESTS);

		/* ******************************************************************************* */
		// call the HW accelerator
		XTmrCtr_Reset(&timer_dev, XPAR_AXI_TIMER_0_DEVICE_ID);
		begin_time = XTmrCtr_GetValue(&timer_dev, XPAR_AXI_TIMER_0_DEVICE_ID);

		// Setup the HW Accelerator
		status = Start_HW_Accelerator(); //Just once

		//flush the cache. You have to do this every time data is changed
		Xil_DCacheFlushRange((unsigned int)in,dma_size);
		Xil_DCacheFlushRange((unsigned int)inv_hw,dma_size);

		print("\rCache cleared\n\r");

		for (i = 0; i < NUM_TESTS; i++) {
			status = Run_HW_Accelerator(in, inv_hw, dma_size);
		}
		end_time = XTmrCtr_GetValue(&timer_dev, XPAR_AXI_TIMER_0_DEVICE_ID);
		run_time_hw = end_time - begin_time - calibration;
		xil_printf(
				"Total run time for AXI DMA + HW accelerator is %d cycles over %d tests\n",
				run_time_hw/NUM_TESTS, NUM_TESTS);

		//Xil_DCacheFlushRange((unsigned int)res_hw,dma_size);

		/* ******************************************************************************* */
		//Compare the results from sw and hw
		for (i = 0; i < DIM; i++) {
			for (j = 0; j < DIM; j++) {
				if ((inv_sw[i][j] - inv_hw[i][j]) >= 0.00001) {
					err++;
				}
			}
		}

		// HW vs. SW speedup factor
		acc_factor = (float) run_time_sw / (float) run_time_hw;

		printf("\r\033[1mAcceleration factor: %d.%d \033[0m\r\n\r\n",
                   (int) acc_factor, (int) (acc_factor * 1000) % 1000);

	if (err == 0)
		print("SW and HW results match!\n\n");
	else
		printf("ERROR: results mismatch (%d)\n\n",err);


    cleanup_platform();
    return 0;
}

void inverse_matrix_ref(float in[DIM][DIM], float inv[DIM][DIM]) {
	int i,j,k;
	int n = DIM*2;
	float ratio;
	float temp[n][n];

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
