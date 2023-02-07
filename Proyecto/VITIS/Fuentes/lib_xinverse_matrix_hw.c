#include "lib_xinverse_matrix_hw.h"

#include "platform.h"
#include "xparameters.h"
#include "xscugic.h"
#include "xaxidma.h"
#include "xinverse_matrix_hw_wrapped.h"
#include "xil_printf.h"

volatile static int ResultExample = 0;


XInverse_matrix_hw_wrapped xinverse_matrix_dev;

XInverse_matrix_hw_wrapped_Config xinverse_matrix_config = {
		0,
		XPAR_INVERSE_MATRIX_HW_WR_0_S_AXI_CONTROL_BUS_BASEADDR
};


//Interrupt Controller Instance
XScuGic ScuGic;

// AXI DMA Instance
extern XAxiDma AxiDma;

int XInverseMatrixSetup(){
	return XInverse_matrix_hw_wrapped_CfgInitialize(&xinverse_matrix_dev, &xinverse_matrix_config);
}



void XInverseMatrixStart(void *InstancePtr){
	XInverse_matrix_hw_wrapped *pExample = (XInverse_matrix_hw_wrapped *)InstancePtr;
	XInverse_matrix_hw_wrapped_InterruptEnable(pExample, 1);
	XInverse_matrix_hw_wrapped_InterruptGlobalEnable(pExample);
	XInverse_matrix_hw_wrapped_Start(pExample);
}



void XInverseMatrixIsr(void *InstancePtr) {
	XInverse_matrix_hw_wrapped *pExample = (XInverse_matrix_hw_wrapped *)InstancePtr;

	//Disable the global interrupt
	XInverse_matrix_hw_wrapped_InterruptGlobalDisable(pExample);
	//Disable the local interrupt
	XInverse_matrix_hw_wrapped_InterruptDisable(pExample, 0xffffffff);

	// clear the local interrupt
	XInverse_matrix_hw_wrapped_InterruptClear(pExample,1);

	ResultExample = 1;

	XInverse_matrix_hw_wrapped_InterruptEnable(pExample,1);
	XInverse_matrix_hw_wrapped_InterruptGlobalEnable(pExample);
}


int XInverseMatrixSetupInterrupt() {
	//This functions sets up the interrupt on the ARM
		int result;
		XScuGic_Config *pCfg = XScuGic_LookupConfig(XPAR_SCUGIC_SINGLE_DEVICE_ID);
		if (pCfg == NULL){
			print("Interrupt Configuration Lookup Failed\n\r");
			return XST_FAILURE;
		}
		result = XScuGic_CfgInitialize(&ScuGic,pCfg,pCfg->CpuBaseAddress);
		if(result != XST_SUCCESS){
			return result;
		}
		// self test
		result = XScuGic_SelfTest(&ScuGic);
		if(result != XST_SUCCESS){
			return result;
		}
		// Initialize the exception handler
		Xil_ExceptionInit();
		// Register the exception handler
		//print("Register the exception handler\n\r");
		Xil_ExceptionRegisterHandler(XIL_EXCEPTION_ID_INT,(Xil_ExceptionHandler)XScuGic_InterruptHandler,&ScuGic);
		//Enable the exception handler
		Xil_ExceptionEnable();
		// Connect the Adder ISR to the exception table
		//print("Connect the Adder ISR to the Exception handler table\n\r");
		result = XScuGic_Connect(&ScuGic,XPAR_FABRIC_INVERSE_MATRIX_HW_WR_0_INTERRUPT_INTR,(Xil_InterruptHandler)XInverseMatrixIsr,&xinverse_matrix_dev);
		if(result != XST_SUCCESS){
			return result;
		}
		//print("Enable the Adder ISR\n\r");
		XScuGic_Enable(&ScuGic,XPAR_FABRIC_INVERSE_MATRIX_HW_WR_0_INTERRUPT_INTR);
		return XST_SUCCESS;
}


int Setup_HW_Accelerator(float A[DIM][DIM], float inv_hw[DIM][DIM], int dma_size) {
	//Setup the Vivado HLS Block
	int status = XInverseMatrixSetup();
	if (status != XST_SUCCESS) {
		print("Error: example setup failed\n");
		return XST_FAILURE;
	}
	status = XInverseMatrixSetupInterrupt();
	if(status != XST_SUCCESS){
		print("Error: interrupt setup failed\n");
		return XST_FAILURE;
	}

	XInverseMatrixStart(&xinverse_matrix_dev);

	return 0;
}


int Start_HW_Accelerator(void) {
	int status = XInverseMatrixSetup();
	if(status != XST_SUCCESS){
		print("Error: example setup failed\n");
		return XST_FAILURE;
	}
	status = XInverseMatrixSetupInterrupt();
	if(status != XST_SUCCESS){
		print("Error: interrupt setup failed\n");
		return XST_FAILURE;
	}

	XInverseMatrixStart(&xinverse_matrix_dev);

	return 0;
}


int Run_HW_Accelerator(float in[DIM][DIM], float inv_hw[DIM][DIM], int dma_size) {
	//transfer A to the Vivado HLS block
	int status = XAxiDma_SimpleTransfer(&AxiDma, (unsigned int) in, dma_size, XAXIDMA_DMA_TO_DEVICE);
	if (status != XST_SUCCESS) {
		print("Error: DMA transfer (A) to Vivado HLS block failed\n");
		return XST_FAILURE;
	}

	status = XAxiDma_SimpleTransfer(&AxiDma, (unsigned int) inv_hw, dma_size,
			XAXIDMA_DEVICE_TO_DMA);
	if (status != XST_SUCCESS) {
		print("Error: DMA transfer (C) from Vivado HLS block failed\n");
		return XST_FAILURE;
	}

	while (!ResultExample);
	ResultExample = 0;

	while ((XAxiDma_Busy(&AxiDma, XAXIDMA_DEVICE_TO_DMA)));

	// Accelerator must me restarted
	XInverseMatrixStart(&xinverse_matrix_dev);

	return 0;
}
