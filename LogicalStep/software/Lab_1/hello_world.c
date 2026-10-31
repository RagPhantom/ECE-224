/*
 * "Hello World" example.
 *
 * This example prints 'Hello from Nios II' to the STDOUT stream. It runs on
 * the Nios II 'standard', 'full_featured', 'fast', and 'low_cost' example
 * designs. It runs with or without the MicroC/OS-II RTOS and requires a STDOUT
 * device in your system's hardware.
 * The memory footprint of this hosted application is ~69 kbytes by default
 * using the standard reference design.
 *
 * For a reduced footprint version of this template, and an explanation of how
 * to reduce the memory footprint for a given application, see the
 * "small_hello_world" template.
 *
 */

#include <stdio.h>
#include <system.h>
#include "altera_avalon_pio_regs.h"
#include "sys/alt_irq.h"

#define EGM_BUSY 1
#define EGM_PERIOD 2
#define EGM_PULSE_WIDTH 3
#define EGM_AVERAGE_LATENCY 4
#define EGM_MISSED 5
#define EGM_MULTI 6

int missed_pulses = 0;
int multi_pulses = 0;
int average_latency = 0;
int counter = 0;

#define LED2 0x4
#define LED1 0x2
#define LED0 0x1
#define LED_CLOSE 0x0


static void stimulus_isr(void *context, alt_u32 id);
int background();
void interrupt();
void tight_polling();
int characterization();

int main()
{
	int mode = 0;
	if ((IORD(SWITCH_PIO_BASE, 0)&0x1) == 0){
		printf("Interrupt method selected.\n");

	}
	else {
		printf("Tight polling method selected.\n");
		mode = 1;
	}
	printf("Period, Pulse_Width, BG_Tasks Run, Latency, Missed, Multiple\n");
	printf("Please, press PB0 to continue.\n");
	while ((~IORD(BUTTON_PIO_BASE, 0)&0x1) != 1){
	}
	if (mode == 0){
		interrupt();
	}
	else{
		tight_polling();
	}

  return 0;
}

static void stimulus_isr(void *context, alt_u32 id){
	// send response out pulse
	IOWR(RESPONSE_OUT_BASE, 0, 1);
	IOWR(RESPONSE_OUT_BASE, 0, 0);

	IOWR(LED_PIO_BASE, 0, LED2);

	// Set edge detection to 0 (clear the interrupt)
	IOWR(STIMULUS_IN_BASE, 3, 0);

	IOWR(LED_PIO_BASE, 0, LED_CLOSE);
}

int background()
{
	IOWR(LED_PIO_BASE, 0, LED0);
    int j;
    int x = 0;
    int grainsize = 4;
    int g_taskProcessed = 0;

    for(j = 0; j < grainsize; j++)
    {
        g_taskProcessed++;
    }
	IOWR(LED_PIO_BASE, 0, LED_CLOSE);
    return x;
}

void interrupt(){

	// register the interrupt for stimulus in
	alt_irq_register(STIMULUS_IN_IRQ, (void *)0, stimulus_isr);
	// enabling the interrupt
	IOWR(STIMULUS_IN_BASE, 2, 1);
	// background task and experiments
	for (int period = 2; period <= 4000; period += 2 ) {
		IOWR(LED_PIO_BASE, 0, LED1);
		IOWR(LED_PIO_BASE, 0, LED_CLOSE);

		IOWR(EGM_BASE, EGM_PERIOD, period);
		// changes here, change EGM_PERIOD to EGM PULSE_WIDTH
		IOWR(EGM_BASE, EGM_PULSE_WIDTH, period/2);
        IOWR(EGM_BASE, 0, 1);
		// printf("period: %d\n", period);

		while(1) {
			int busy = IORD(EGM_BASE, EGM_BUSY);
			if (busy == 0) {
				// egm stop sending pulses, record values
				missed_pulses = IORD(EGM_BASE, EGM_MISSED);
				multi_pulses = IORD(EGM_BASE, EGM_MULTI);
				average_latency = IORD(EGM_BASE, EGM_AVERAGE_LATENCY);
				printf("%d,%d,%d,%d,%d,%d\n", period, period/2, counter, average_latency, missed_pulses, multi_pulses);

				// disable the egm base to reset it
				IOWR(EGM_BASE, 0, 0);

				break;
			}
            else{
                // run background task
                    background();
                    counter++;
            }
		}
        counter = 0; // clear the counter
	}
}

void tight_polling(){
	int last_pulse = 0;
	int curr_pulse = 0;
	// background task and experiments
	for (int period = 2; period <= 4000; period += 2 ) {
		IOWR(LED_PIO_BASE, 0, LED1);
		IOWR(LED_PIO_BASE, 0, LED_CLOSE);

		IOWR(EGM_BASE, EGM_PERIOD, period);
		// changes here, change EGM_PERIOD to EGM PULSE_WIDTH
		IOWR(EGM_BASE, EGM_PULSE_WIDTH, period/2);
        IOWR(EGM_BASE, 0, 1);
		// printf("period: %d\n", period);
        last_pulse = 0;
        curr_pulse = 0;
        int N_tested = 0;
        int N = 0;

		while(1) {
			int busy = IORD(EGM_BASE, EGM_BUSY);
			if (busy == 0) {
				// egm stop sending pulses, record values
				missed_pulses = IORD(EGM_BASE, EGM_MISSED);
				multi_pulses = IORD(EGM_BASE, EGM_MULTI);
				average_latency = IORD(EGM_BASE, EGM_AVERAGE_LATENCY);
				printf("%d,%d,%d,%d,%d,%d\n", period, period/2, counter, average_latency, missed_pulses, multi_pulses);

				// disable the egm base to reset it
				IOWR(EGM_BASE, 0, 0);

				break;
			}
            else{
                last_pulse = curr_pulse;
                curr_pulse = IORD(STIMULUS_IN_BASE, 0);

                if (last_pulse == 0 && curr_pulse == 1){ // a rising edge detected, cpu response
                    	// send response out pulse
	                    IOWR(RESPONSE_OUT_BASE, 0, 1);
	                    IOWR(RESPONSE_OUT_BASE, 0, 0);

                        // if this is the first time entering this EGM, calculate the number of times(N) background task needs to run
                        if (N_tested == 0){
                            N = characterization() - 1;
                            N_tested = 1;
                        }

                        // run background task N times
                        for (int i = 0; i < N; i++){
                            // in case EGM ends here, if EGM is busy, run background task
                            if (IORD(EGM_BASE, EGM_BUSY) == 1){
                                background();
                                counter++;
                            }
                        }
                }
            }
		}
        counter = 0; // clear the counter
	}
}

int characterization(){
    int N = 0;
    // according to the real situation, during the first pulse, we have 'pulse' = 1
    // since it may react to the first stimulus, not the second one
    int last_pulse_c = 1;
    int curr_pulse_c = 1;

    while (1){
        // in case EGM ends here, if EGM is busy, run background task
        if (IORD(EGM_BASE, EGM_BUSY) == 1){
            background();
            counter++;
            N++; // count how many background tasks can be called in this pulse

            last_pulse_c = curr_pulse_c;
            curr_pulse_c = IORD(STIMULUS_IN_BASE, 0); // get the current pulse

            if (last_pulse_c == 0 && curr_pulse_c == 1){ // the rising edge of the second pulse, cpu response
                // send response out pulse
                IOWR(RESPONSE_OUT_BASE, 0, 1);
                IOWR(RESPONSE_OUT_BASE, 0, 0);
                break;
            }
        }
        else{
            return 0;
        }
    }
    return N;
}
