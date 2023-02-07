#include "platform.h"
#include "xparameters.h"
#include "xscugic.h"
#include "xaxidma.h"
#include "xfir_hw.h"
#include "lib_fir_filter_hw.h"
#include "xil_printf.h"

//#define XPAR_FIR_HW_0_INTERRUPT_INTR XPAR_FABRIC_FIR_HW_0_INTERRUPT_INTR
//#define XPAR_FIR_HW_0_S_AXI_CONTROL_BUS_BASEADDR XPAR_FIR_HW_0_S_AXI_CONTROL_BUS_BASEADDR


volatile static int ResultExample = 0;

XFir_hw xfir_dev;

XFir_hw_Config xfir_config = {
	0,
	XPAR_FIR_HW_0_S_AXI_CONTROL_BUS_BASEADDR
};

//Interrupt Controller Instance
XScuGic ScuGic;

// AXI DMA Instance
extern XAxiDma AxiDma;

int XFirFilterSetup() {
	return XFir_hw_CfgInitialize(&xfir_dev,&xfir_config);
}

void XFirFilterStart(void *InstancePtr) {
	XFir_hw *pExample = (XFir_hw *)InstancePtr;
	XFir_hw_InterruptEnable(pExample,1);
	XFir_hw_InterruptGlobalEnable(pExample);
	XFir_hw_Start(pExample);
}

void XFirFilterIsr(void *InstancePtr) {
	XFir_hw *pExample = (XFir_hw *)InstancePtr;
	XFir_hw_InterruptGlobalDisable(pExample);
	XFir_hw_InterruptDisable(pExample, 0xffffffff);
	XFir_hw_InterruptClear(pExample, 1);

	ResultExample = 1;

	XFir_hw_InterruptEnable(pExample,1);
	XFir_hw_InterruptGlobalEnable(pExample);
}

int XFirFilterSetupInterrupt() {
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
	result = XScuGic_Connect(&ScuGic,XPAR_FABRIC_FIR_HW_0_INTERRUPT_INTR,(Xil_InterruptHandler)XFirFilterIsr,&xfir_dev);
	if(result != XST_SUCCESS){
		return result;
	}
	//print("Enable the Adder ISR\n\r");
	XScuGic_Enable(&ScuGic,XPAR_FABRIC_FIR_HW_0_INTERRUPT_INTR);
	return XST_SUCCESS;
}

int Setup_HW_Accelerator(float in[SIZE], float out[SIZE], float p_coeffs[FILTER_LEN], int dma_size) {
	int status = XFirFilterSetup();

	if(status != XST_SUCCESS){
		print("Error: example setup failed\n");
		return XST_FAILURE;
	}
	status =  XFirFilterSetupInterrupt();
	if(status != XST_SUCCESS){
		print("Error: interrupt setup failed\n");
		return XST_FAILURE;
	}

	XFirFilterStart(&xfir_dev);
	return 0;
}

int Start_HW_Accelerator(u32 selop) {
	int status = XFirFilterSetup();
	if(status != XST_SUCCESS){
		print("Error: example setup failed\n");
		return XST_FAILURE;
	}
	status =  XFirFilterSetupInterrupt();
	if(status != XST_SUCCESS){
		print("Error: interrupt setup failed\n");
		return XST_FAILURE;
	}

	XFirFilterStart(&xfir_dev);

	return 0;
}

int Run_HW_Accelerator(float in[SIZE], float out[SIZE], float p_coeffs[FILTER_LEN], u32 selop, int dma_size) {

	XFir_hw_Set_selop(&xfir_dev, selop);

	if (XFir_hw_Get_selop(&xfir_dev) == 52428) {
		XFirFilterStart(&xfir_dev);

		int status = XAxiDma_SimpleTransfer(&AxiDma, (unsigned int) in, dma_size, XAXIDMA_DMA_TO_DEVICE);
		if (status != XST_SUCCESS) {
			print("Error: DMA transfer (in) to Vivado HLS block failed\n");
			return XST_FAILURE;
		}

		status = XAxiDma_SimpleTransfer(&AxiDma, (unsigned int) out, dma_size,
				XAXIDMA_DEVICE_TO_DMA);
		if (status != XST_SUCCESS) {
			print("Error: DMA transfer (out) from Vivado HLS block failed\n");
			return XST_FAILURE;
		}

		while ((XAxiDma_Busy(&AxiDma, XAXIDMA_DEVICE_TO_DMA)));

	}

	if (XFir_hw_Get_selop(&xfir_dev) == 43690) {
		XFirFilterStart(&xfir_dev);

		int status = XAxiDma_SimpleTransfer(&AxiDma, (unsigned int) p_coeffs, dma_size, XAXIDMA_DMA_TO_DEVICE);
		if (status != XST_SUCCESS) {
			print("Error: DMA transfer (p_coeffs) to Vivado HLS block failed\n");
			return XST_FAILURE;
		}

	}

	while (XAxiDma_Busy(&AxiDma, XAXIDMA_DMA_TO_DEVICE));
}
