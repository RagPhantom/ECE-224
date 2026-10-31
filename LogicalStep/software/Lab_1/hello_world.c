 /* "Hello World" example.*/


/*
 * ECE 224 Lab 1A
 * SW0 = 0 -> interrupt mode
 * SW0 = 1 -> tight polling mode
 * Press PB0 to start the experiment.
 * Each test prints one CSV line:
 * period, pulse width, BG tasks run, average latency, missed, multiple
 */

#include <stdio.h>
#include <system.h>                  // base addresses and IRQ numbers of all hardware
#include "altera_avalon_pio_regs.h"  // IORD / IOWR macros
#include "sys/alt_irq.h"             // interrupt registration

// EGM register offsets (manual Appendix A, Table 22)
#define EGM_BUSY 1
#define EGM_PERIOD 2
#define EGM_PULSE_WIDTH 3
#define EGM_AVERAGE_LATENCY 4
#define EGM_MISSED 5
#define EGM_MULTI 6

// results read from the EGM after each test
int missed_pulses = 0;
int multi_pulses = 0;
int average_latency = 0;
int counter = 0;      // number of background tasks run in the current test

// LED values used as SignalTap markers (one bit per LED)
#define LED2 0x4      // ISR marker
#define LED1 0x2      // start of test marker
#define LED0 0x1      // background task marker
#define LED_CLOSE 0x0 // all LEDs off


static void stimulus_isr(void *context, alt_u32 id);
int background();
void interrupt();
void tight_polling();
int characterization();

int main()
{
	int mode = 0;     // 0 = interrupt, 1 = tight polling

	// read SW0 (bit 0 of the switch PIO) to pick the mode
	if ((IORD(SWITCH_PIO_BASE, 0)&0x1) == 0){
		printf("Interrupt method selected.\n");

	}
	else {
		printf("Tight polling method selected.\n");
		mode = 1;
	}

	// print the CSV header
	printf("Period, Pulse_Width, BG_Tasks Run, Latency, Missed, Multiple\n");
	printf("Please, press PB0 to continue.\n");

	// wait here until PB0 is pressed (buttons are active low)
	while ((~IORD(BUTTON_PIO_BASE, 0)&0x1) != 1){
	}

	// run the experiment for the selected mode
	if (mode == 0){
		interrupt();
	}
	else{
		tight_polling();
	}

  return 0;
}

// ISR: runs every time the stimulus PIO sees a rising edge
static void stimulus_isr(void *context, alt_u32 id){
	// send response out pulse
	IOWR(RESPONSE_OUT_BASE, 0, 1);
	IOWR(RESPONSE_OUT_BASE, 0, 0);

	// turn on the ISR marker
	IOWR(LED_PIO_BASE, 0, LED2);

	// Set edge detection to 0 (clear the interrupt)
	IOWR(STIMULUS_IN_BASE, 3, 0);

	// turn the LED markers off
	IOWR(LED_PIO_BASE, 0, LED_CLOSE);
}

// background task (given on LEARN), wrapped with the BG marker
int background()
{
	// turn on the background task marker
	IOWR(LED_PIO_BASE, 0, LED0);
    int j;
    int x = 0;
    int grainsize = 4;
    int g_taskProcessed = 0;

    for(j = 0; j < grainsize; j++)
    {
        g_taskProcessed++;
    }
	// turn the background task marker off
	IOWR(LED_PIO_BASE, 0, LED_CLOSE);
    return x;
}

// interrupt mode: the ISR answers each stimulus,
// main code just keeps running background tasks
void interrupt(){

	// register the interrupt for stimulus in
	alt_irq_register(STIMULUS_IN_IRQ, (void *)0, stimulus_isr);
	// enabling the interrupt
	IOWR(STIMULUS_IN_BASE, 2, 1);
	// background task and experiments
	// one test for each period from 2 to 4000
	for (int period = 2; period <= 4000; period += 2 ) {
		// start of test marker (short pulse on LED1)
		IOWR(LED_PIO_BASE, 0, LED1);
		IOWR(LED_PIO_BASE, 0, LED_CLOSE);

		// set the period and a 50% pulse width
		IOWR(EGM_BASE, EGM_PERIOD, period);
		IOWR(EGM_BASE, EGM_PULSE_WIDTH, period/2);
		// enable the EGM to start this test
        IOWR(EGM_BASE, 0, 1);
		// printf("period: %d\n", period);

		while(1) {
			// check if the EGM is still running
			int busy = IORD(EGM_BASE, EGM_BUSY);
			if (busy == 0) {
				// egm stop sending pulses, record values
				missed_pulses = IORD(EGM_BASE, EGM_MISSED);
				multi_pulses = IORD(EGM_BASE, EGM_MULTI);
				average_latency = IORD(EGM_BASE, EGM_AVERAGE_LATENCY);
				// print one CSV line for this test
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

// tight polling mode: watch the raw stimulus signal for rising edges,
// respond, then run N background tasks before polling again
void tight_polling(){
	int last_pulse = 0;   // stimulus value from the previous read
	int curr_pulse = 0;   // stimulus value from the current read
	// background task and experiments
	// one test for each period from 2 to 4000
	for (int period = 2; period <= 4000; period += 2 ) {
		// start of test marker (short pulse on LED1)
		IOWR(LED_PIO_BASE, 0, LED1);
		IOWR(LED_PIO_BASE, 0, LED_CLOSE);

		// set the period and a 50% pulse width
		IOWR(EGM_BASE, EGM_PERIOD, period);
		IOWR(EGM_BASE, EGM_PULSE_WIDTH, period/2);
		// enable the EGM to start this test
        IOWR(EGM_BASE, 0, 1);
		// printf("period: %d\n", period);

		// reset the state for this test
        last_pulse = 0;
        curr_pulse = 0;
        int N_tested = 0;     // 1 once characterization is done
        int N = 0;            // background tasks to run after each response

		while(1) {
			// check if the EGM is still running
			int busy = IORD(EGM_BASE, EGM_BUSY);
			if (busy == 0) {
				// egm stop sending pulses, record values
				missed_pulses = IORD(EGM_BASE, EGM_MISSED);
				multi_pulses = IORD(EGM_BASE, EGM_MULTI);
				average_latency = IORD(EGM_BASE, EGM_AVERAGE_LATENCY);
				// print one CSV line for this test
				printf("%d,%d,%d,%d,%d,%d\n", period, period/2, counter, average_latency, missed_pulses, multi_pulses);

				// disable the egm base to reset it
				IOWR(EGM_BASE, 0, 0);

				break;
			}
            else{
                // read the raw stimulus value (data register, not edge capture)
                last_pulse = curr_pulse;
                curr_pulse = IORD(STIMULUS_IN_BASE, 0);

                if (last_pulse == 0 && curr_pulse == 1){ // a rising edge detected, cpu response
                    	// send response out pulse
	                    IOWR(RESPONSE_OUT_BASE, 0, 1);
	                    IOWR(RESPONSE_OUT_BASE, 0, 0);

                        // if this is the first time entering this EGM, calculate the number of times(N) background task needs to run
                        // (minus 1 to leave a safety margin)
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

// characterization: run background tasks until the next rising edge,
// respond to it, and return how many tasks fit in one period
// (returns 0 if the EGM finishes first)
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

            // check the stimulus after each background task
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
