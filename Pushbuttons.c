/*
 * Lab2_PushButtons
 *
 * Debounces the 4 pushbuttons using the button_pio interrupt and timer_0.
 * No software delays are used.
 *
 * Flow:
 *   1. A button edge triggers button_isr: button interrupts are disabled
 *      and timer_0 is started (15 ms one-shot).
 *   2. Bounce edges during the 15 ms are ignored.
 *   3. timer_isr reads the buttons once the signal is stable. If a button
 *      is pressed, it is reported to main through button_event.
 *      Button interrupts are then re-enabled.
 */

#include <stdio.h>
#include <system.h>
#include "altera_avalon_pio_regs.h"
#include "sys/alt_irq.h"

// PIO register offsets
#define PIO_DATA           0
#define PIO_IRQ_MASK       2
#define PIO_EDGE_CAPTURE   3

// Timer register offsets
#define TIMER_STATUS    0
#define TIMER_CONTROL   1
#define TIMER_PERIODL   2
#define TIMER_PERIODH   3

// Set by timer_isr when a valid press is detected, cleared by main.
// Active high, one bit per button (bit 0 = PB0 ... bit 3 = PB3), 0 = no event.
volatile int button_event = 0;

static void button_isr(void *context, alt_u32 id);
static void timer_isr(void *context, alt_u32 id);

int main()
{
	printf("Lab2_PushButtons start\n");

	// Clear any old edges before enabling interrupts
	IOWR(BUTTON_PIO_BASE, PIO_EDGE_CAPTURE, 0);

	// Register ISRs
	alt_irq_register(BUTTON_PIO_IRQ, (void *)0, button_isr);
	alt_irq_register(TIMER_0_IRQ, (void *)0, timer_isr);

	// Timer: 15 ms period = 750,000 cycles at 50 MHz, register value = 749,999 = 0xB71AF
	IOWR(TIMER_0_BASE, TIMER_PERIODH, 0xB);
	IOWR(TIMER_0_BASE, TIMER_PERIODL, 0x71AF);
	// Control = 0x9: STOP=1, START=0, CONT=0 (one-shot), ITO=1 (interrupt enabled)
	IOWR(TIMER_0_BASE, TIMER_CONTROL, 0x9);
	// Clear timeout flag
	IOWR(TIMER_0_BASE, TIMER_STATUS, 0x0);

	// Enable interrupts for all 4 buttons (done last, after the timer is ready)
	IOWR(BUTTON_PIO_BASE, PIO_IRQ_MASK, 0xF);

	while (1) {
		if (button_event != 0) {
			// Show the pressed button on the LEDs
			IOWR(LED_PIO_BASE, 0, button_event);
			printf("OK\n");
			button_event = 0;
		}
	}

	return 0;
}

// Button edge detected: ignore further bounces and start the debounce timer
static void button_isr(void *context, alt_u32 id)
{
	// Disable button interrupts during the debounce window
	IOWR(BUTTON_PIO_BASE, PIO_IRQ_MASK, 0);
	// Start timer: START=1, ITO=1 (keep interrupt enabled), CONT=0
	IOWR(TIMER_0_BASE, TIMER_CONTROL, 0x5);
	// Clear the interrupt (last)
	IOWR(BUTTON_PIO_BASE, PIO_EDGE_CAPTURE, 0);
}

// 15 ms later: buttons are stable, check for a real press
static void timer_isr(void *context, alt_u32 id)
{
	int data;

	// Read buttons, invert (active low -> active high), keep the lower 4 bits
	data = IORD(BUTTON_PIO_BASE, PIO_DATA);
	data = (~data) & 0x0F;

	// Clear edges left by bounces BEFORE re-enabling interrupts,
	// otherwise a stale edge would trigger button_isr immediately
	IOWR(BUTTON_PIO_BASE, PIO_EDGE_CAPTURE, 0);
	IOWR(BUTTON_PIO_BASE, PIO_IRQ_MASK, 0xF);

	// Report only real presses; releases and noise read as 0 and are ignored
	if (data != 0) {
		button_event = data;
	}

	// Stop timer (one-shot already stops itself, kept for safety)
	IOWR(TIMER_0_BASE, TIMER_CONTROL, 0x9);
	// Clear the timeout flag (last)
	IOWR(TIMER_0_BASE, TIMER_STATUS, 0);
}