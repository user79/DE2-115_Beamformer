// Code for beamformer demo (running on Tiva, working together with DE2-115 board)
// 8/9/2016

#include <stdio.h>
// to make a Tiva driverlib project, include the includes below, and also in project settings:
//   - under CCS Build / Arm Compiler / Include Options, add "C:\ti\TivaWare_C_Series-2.1.2.111" or equivalent
//   - under CCS Build / Arm Linker / File Search Path, add "C:\ti\TivaWare_C_Series-2.1.2.111\driverlib\ccs\Debug\driverlib.lib" or equivalent
// if want to use UARTprintf(), need to either include the pre-built .lib from the example directory (see the "hello" example imported project settings for details) or explicitly add the /utils/uartstdio.c to be compiled
#include <stdint.h>
#include <stdbool.h>
#include "inc/hw_memmap.h"
#include "driverlib/fpu.h"
#include "driverlib/sysctl.h"
#include "driverlib/rom.h"
#include "driverlib/pin_map.h"
#include "driverlib/uart.h"
#include "grlib/grlib.h"
#include "utils/uartstdio.h"
#include "driverlib/gpio.h"
#include "driverlib/adc.h"

void ConfigureUART();

int main(void)
{
	uint32_t data[6];// will hold the data from the ADC FIFO
	int32_t temp; // convert to signed data before sending out...

    // Set the clocking to run directly from the crystal.
    SysCtlClockSet(SYSCTL_SYSDIV_2_5 | SYSCTL_USE_PLL | SYSCTL_XTAL_16MHZ | SYSCTL_OSC_MAIN); // run at 80MHz (but ~150ns/line of C code?)

    ConfigureUART(); // Initialize the UART.
    UARTprintf("Hello, world!\n");
	
	SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);
	while(!SysCtlPeripheralReady(SYSCTL_PERIPH_GPIOA));
	GPIOPinTypeGPIOOutput(GPIO_PORTA_BASE, GPIO_PIN_2 | GPIO_PIN_3 | GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7); //databus outputs to DE2

	SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOB);
	while(!SysCtlPeripheralReady(SYSCTL_PERIPH_GPIOB));
	GPIOPinTypeGPIOOutput(GPIO_PORTB_BASE, GPIO_PIN_2);// for debug output
	GPIOPinTypeGPIOOutput(GPIO_PORTB_BASE, GPIO_PIN_3);// "tivaserialclock" trigger that tells DE2's shift registers when to clock in each 12-bit sample
	GPIOPinTypeADC(GPIO_PORTB_BASE,  GPIO_PIN_5); // adc microphone input mic5

	SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOC);
	while(!SysCtlPeripheralReady(SYSCTL_PERIPH_GPIOC));
	GPIOPinTypeGPIOOutput(GPIO_PORTC_BASE, GPIO_PIN_6 | GPIO_PIN_7 );// databus outputs to DE2

	SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOD);
	while(!SysCtlPeripheralReady(SYSCTL_PERIPH_GPIOD));
	GPIOPinTypeGPIOOutput(GPIO_PORTD_BASE, GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3 );//databus outputs to DE2

	SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOE);
	while(!SysCtlPeripheralReady(SYSCTL_PERIPH_GPIOE));
	GPIOPinTypeADC(GPIO_PORTE_BASE,  GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3 | GPIO_PIN_5); // adc microphone inputs mic3, mic2, mic1, mic0, mic4

	SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOF);
	while(!SysCtlPeripheralReady(SYSCTL_PERIPH_GPIOF));
	GPIOPinTypeGPIOInput(GPIO_PORTF_BASE, GPIO_PIN_4  );// write_readyp trigger from DE2

	SysCtlPeripheralEnable(SYSCTL_PERIPH_ADC0); // Enable the ADC0 module.
	while(!SysCtlPeripheralReady(SYSCTL_PERIPH_ADC0)); // Wait for the ADC0 module to be ready.
	// Enable the first sample sequencer (to capture the value of all 8 channels), with priority 0, when the processor trigger occurs.
	ADCSequenceConfigure(ADC0_BASE, 0, ADC_TRIGGER_PROCESSOR, 0);
	ADCSequenceStepConfigure(ADC0_BASE, 0, 0, ADC_CTL_CH0); // set up each "step" of the "sequence"
	ADCSequenceStepConfigure(ADC0_BASE, 0, 1, ADC_CTL_CH1);
	ADCSequenceStepConfigure(ADC0_BASE, 0, 2, ADC_CTL_CH2);
	ADCSequenceStepConfigure(ADC0_BASE, 0, 3, ADC_CTL_CH3);
	ADCSequenceStepConfigure(ADC0_BASE, 0, 4, ADC_CTL_CH8);
	ADCSequenceStepConfigure(ADC0_BASE, 0, 5,  ADC_CTL_IE | ADC_CTL_END | ADC_CTL_CH11); // final step, set interrupt flag
	ADCSequenceEnable(ADC0_BASE, 0);

	while(1)
	{
		//GPIOPinWrite(GPIO_PORTB_BASE,GPIO_PIN_2,GPIO_PIN_2);// for debugging only

		// wait for "write_readyp" high pulse from DE2, coming into pin PF0 (assume it goes low again before next iteration)
		volatile uint32_t test = GPIOPinRead(GPIO_PORTF_BASE,GPIO_PIN_4);
		while(!test)
		{
			test = GPIOPinRead(GPIO_PORTF_BASE,GPIO_PIN_4);
		}

		// the following code samples each channel one after another, using 'sequencer 0', in 6 'steps', then copies the results from the FIFO into an array
		ADCProcessorTrigger(ADC0_BASE, 0);// Trigger the beginning of a new ADC sample sequence.
		// While the ADC read is progressing, use this time to send PREVIOUS data sample to DE2 (because current one hasn't finished
		//    yet), one 12-bit sample at a time, starting with 'mic0', then 'mic1', etc... up to 'mic5'
		volatile int mic;
		for(mic = 0;  mic < 6;  mic++)
		{
			temp = (int32_t)(data[mic]) - 2048; // convert unsigned signed to signed (2's complement) data... ?   12 bit means max reading is 4095
			//todo: fix this:
			//temp = temp >> 3; // attenuate if desired, for convenience?   but this loses dynamic range... better to attenuate after the beamformer, if possible...
			GPIOPinWrite(GPIO_PORTA_BASE,  GPIO_PIN_2 | GPIO_PIN_3 | GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7,  temp<<3 ); // put bits 0,1,2,3,4,5 on the bus
			GPIOPinWrite(GPIO_PORTD_BASE,  GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3,  temp>>6 ); // put bits 6,7,8,9 on the bus
			GPIOPinWrite(GPIO_PORTC_BASE,  GPIO_PIN_6 | GPIO_PIN_7,  temp>>4 ); // put bits 10,11 on the bus
			GPIOPinWrite(GPIO_PORTB_BASE,  GPIO_PIN_3,GPIO_PIN_3); //send "data is ready to read" strobe pulse to DE2 on tivaserialclock
			GPIOPinWrite(GPIO_PORTB_BASE,  GPIO_PIN_3,0);
		}
		while(!ADCIntStatus(ADC0_BASE, 0, false));// check whether the current sample sequence has completed.
		ADCIntClear(ADC0_BASE, 0);
		ADCSequenceDataGet(ADC0_BASE, 0, data);// Read the values from the ADC FIFO into the "data" array.


		// for debugging only
		//__delay_cycles(300);
		//GPIOPinWrite(GPIO_PORTB_BASE,GPIO_PIN_2,0);
		//volatile int temp1;
		//for(temp1=0; temp1<10; temp1++);
		//GPIOPinWrite(GPIO_PORTB_BASE,GPIO_PIN_2,GPIO_PIN_2);
		//for(temp1=0; temp1<100; temp1++);
		//GPIOPinWrite(GPIO_PORTB_BASE,GPIO_PIN_2,0);

		//UARTprintf("%d %d %d %d %d %d\n", data[0], data[1], data[2], data[3], data[4], data[5]);
		//__delay_cycles(80000000);
	}

	return 0;
}


//*****************************************************************************
//
// Configure the UART and its pins.  This must be called before UARTprintf().
//
//*****************************************************************************
void
ConfigureUART(void)
{
    // Enable the GPIO Peripheral used by the backchannel UART.
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);

    // Enable UART0
    SysCtlPeripheralEnable(SYSCTL_PERIPH_UART0);

    // Configure GPIO Pins for UART mode.
    GPIOPinConfigure(GPIO_PA0_U0RX);
    GPIOPinConfigure(GPIO_PA1_U0TX);
    GPIOPinTypeUART(GPIO_PORTA_BASE, GPIO_PIN_0 | GPIO_PIN_1);

    // Use the internal 16MHz oscillator as the UART clock source.
    UARTClockSourceSet(UART0_BASE, UART_CLOCK_PIOSC);

    // Initialize the UART for console I/O.
    UARTStdioConfig(0, 115200, 16000000);
    //UARTStdioConfig(0, 115200, 80000000);
}



