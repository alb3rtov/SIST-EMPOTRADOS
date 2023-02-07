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
#include "xtmrctr.h"
#include "xaxidma.h"
#include "lib_fir_filter_hw.h"
#include <time.h>
#define NUM_TESTS 1024

void fir_filter_ref(float p_coeffs[FILTER_LEN], float p_in[SIZE], float p_out[SIZE]);

// TIMER Instance
XTmrCtr timer_dev;

// AXI DMA Instance
XAxiDma AxiDma;

int init_dma() {
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
	int i;
	int err=0;
	int status;

	float _Alignas(16) p_in[SIZE];
	float _Alignas(16) res_hw[SIZE];
	float _Alignas(16) res_sw[SIZE];
	float _Alignas(16) p_coeffs[FILTER_LEN];
	float _Alignas(16) new_p_coeffs[FILTER_LEN] = {
			-0.0508558, -0.0736313, -0.0529380,  0.0540186,
			 0.0540186, -0.0529380, -0.0736313, -0.0508558
	};
	//float p_coeffs[FILTER_LEN];
	//float p_in[SIZE];
	//float res_hw[SIZE];
	//float res_sw[SIZE];
	u32 selop = 52428;

	unsigned int dma_size1 = SIZE * sizeof(float);
	unsigned int dma_size2 = FILTER_LEN * sizeof(float);

    float acc_factor;
	unsigned int init_time, curr_time, calibration;
	unsigned int begin_time;
	unsigned int end_time;
	unsigned int run_time_sw = 0;
	unsigned int run_time_hw = 0;

    init_platform();

	xil_printf("\r************************************\r\n");
	xil_printf("       FIR FILTER ACCELERATOR       \r\n");
	xil_printf("  Author: Alberto Vázquez Martínez  \r\n");
	xil_printf("************************************\r\n");

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
	xil_printf("\rLoop time for %d iterations is %d cycles\n", NUM_TESTS, run_time_sw);

	// input data Initiation
	for (i = 0; i < FILTER_LEN; i++) {
		p_coeffs[i] = 1.0f;
	}

	for (i = 0; i < SIZE; i++) {
		p_in[i] = (float)1.0f;
		res_sw[i] = (float)1.0f;
		res_hw[i] = (float)1.0f;
	}
	/** End of Initiation */


	xil_printf("\rPARTE 1 - Sin modificar previamente el vector de coeficientes\n");
	// Call software version of FIR FILTER
	xil_printf("\rRunning Matrix fir filter in SW\n");
	XTmrCtr_Reset(&timer_dev, XPAR_AXI_TIMER_0_DEVICE_ID);
	begin_time = XTmrCtr_GetValue(&timer_dev, XPAR_AXI_TIMER_0_DEVICE_ID);
	for (i = 0; i < NUM_TESTS; i++) {
		fir_filter_ref(p_coeffs, p_in, res_sw);
	}
	end_time = XTmrCtr_GetValue(&timer_dev, XPAR_AXI_TIMER_0_DEVICE_ID);
	run_time_sw = end_time - begin_time - calibration;
	xil_printf("\r\nTotal run time for SW on Processor is %d cycles over %d tests.\n",
			run_time_sw/NUM_TESTS, NUM_TESTS);


	// Call hardware version of FIR FILTER
	XTmrCtr_Reset(&timer_dev, XPAR_AXI_TIMER_0_DEVICE_ID);
	begin_time = XTmrCtr_GetValue(&timer_dev, XPAR_AXI_TIMER_0_DEVICE_ID);

	// Setup the HW Accelerator
	status = Start_HW_Accelerator(selop); //Just once

	//flush the cache. You have to do this every time data is changed
	Xil_DCacheFlushRange((unsigned int)p_coeffs,dma_size2);
	Xil_DCacheFlushRange((unsigned int)p_in,dma_size1);
	Xil_DCacheFlushRange((unsigned int)res_hw,dma_size1);


	print("\rCache cleared\n\r");

	for (i = 0; i < NUM_TESTS; i++) {
		status = Run_HW_Accelerator(p_in, res_hw, p_coeffs, selop, dma_size1);
	}

	end_time = XTmrCtr_GetValue(&timer_dev, XPAR_AXI_TIMER_0_DEVICE_ID);
	run_time_hw = end_time - begin_time - calibration;
	xil_printf(
			"\r\nTotal run time for AXI DMA + HW accelerator is %d cycles over %d tests\n",
			run_time_hw/NUM_TESTS, NUM_TESTS);

	for (i = 0; i < SIZE; i++) {
		if (res_sw[i] != res_hw[i]) {
			err++;
		}
	}
	// HW vs. SW speedup factor
	acc_factor = (float) run_time_sw / (float) run_time_hw;

	printf("\r\033[1mAcceleration factor: %d.%d \033[0m\r\n\r\n",
               (int) acc_factor, (int) (acc_factor * 1000) % 1000);

	if (err == 0)
		print("\r\nSW and HW results match!\n\n");
	else
		printf("\r\nERROR: results mismatch (%d)\n\n",err);


	xil_printf("\r\nPARTE 2 - Modificar el vector de coeficientes");
	selop = 43690;

	//flush the cache. You have to do this every time data is changed
	Xil_DCacheFlushRange((unsigned int)p_coeffs,dma_size2);
	Xil_DCacheFlushRange((unsigned int)new_p_coeffs,dma_size2);
	Xil_DCacheFlushRange((unsigned int)p_in,dma_size1);
	Xil_DCacheFlushRange((unsigned int)res_hw,dma_size1);

	xil_printf("\r\nEjecutando componente acelerador..\n\r");
	status = Run_HW_Accelerator(p_in, res_hw, new_p_coeffs, selop, dma_size2);


	xil_printf("\r\nPARTE 3 - Con vector de coeficientes modificado");
	selop = 52428;

	// Call software version of FIR FILTER
	xil_printf("\r\nRunning Matrix fir filter in SW\n");
	XTmrCtr_Reset(&timer_dev, XPAR_AXI_TIMER_0_DEVICE_ID);
	begin_time = XTmrCtr_GetValue(&timer_dev, XPAR_AXI_TIMER_0_DEVICE_ID);
	for (i = 0; i < NUM_TESTS; i++)
	{
		fir_filter_ref(new_p_coeffs, p_in, res_sw);
	}
	end_time = XTmrCtr_GetValue(&timer_dev, XPAR_AXI_TIMER_0_DEVICE_ID);
	run_time_sw = end_time - begin_time - calibration;
	xil_printf("\r\nTotal run time for SW on Processor is %d cycles over %d tests.\n",
			run_time_sw/NUM_TESTS, NUM_TESTS);


	// Call hardware version of FIR FILTER
	XTmrCtr_Reset(&timer_dev, XPAR_AXI_TIMER_0_DEVICE_ID);
	begin_time = XTmrCtr_GetValue(&timer_dev, XPAR_AXI_TIMER_0_DEVICE_ID);

	//flush the cache. You have to do this every time data is changed
	Xil_DCacheFlushRange((unsigned int)p_coeffs,dma_size2);
	Xil_DCacheFlushRange((unsigned int)new_p_coeffs,dma_size2);
	Xil_DCacheFlushRange((unsigned int)p_in,dma_size1);
	Xil_DCacheFlushRange((unsigned int)res_hw,dma_size1);

	for (i = 0; i < NUM_TESTS; i++) {
		status = Run_HW_Accelerator(p_in, res_hw, new_p_coeffs, selop, dma_size1);
	}
	end_time = XTmrCtr_GetValue(&timer_dev, XPAR_AXI_TIMER_0_DEVICE_ID);
	run_time_hw = end_time - begin_time - calibration;
	xil_printf(
			"\r\nTotal run time for AXI DMA + HW accelerator is %d cycles over %d tests\n",
			run_time_hw/NUM_TESTS, NUM_TESTS);

	for (i = 0; i < SIZE; i++) {
		if (res_sw[i] != res_hw[i]) {
			err++;
		}
	}
	// HW vs. SW speedup factor
	acc_factor = (float) run_time_sw / (float) run_time_hw;
	//xil_printf("Acceleration factor: %d.%d \n\n",
	//		(int) acc_factor, (int) (acc_factor * 1000) % 1000);

	printf("\r\033[1mAcceleration factor: %d.%d \033[0m\r\n\r\n",
			(int) acc_factor, (int) (acc_factor * 1000) % 1000);

	if (err == 0)
		print("\r\nSW and HW results match!\n\n");
	else
		printf("\r\nERROR: results mismatch (%d)\n\n",err);

    cleanup_platform();
    return 0;
}

/* Reference software implementation */
void fir_filter_ref(float p_coeffs[FILTER_LEN], float p_in[SIZE], float p_out[SIZE]) {
	int i, j, k;
	float tmp;

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
