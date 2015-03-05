#include <msp430.h>
#include <stdlib.h>

void DAC_cipher(int amplitude, int latch_port);
void DAC_setup();
void square_wave(int amplitude, int period, float duty_cycle); //amplitude in Volts, period in ms, duty_cycle in decimal

#define SAMPLE_MAX 50
#define HALF_SAMPLE_MAX SAMPLE_MAX/2

#define SQUARE_WAVE 1
#define SINE_WAVE 2
#define SAWTOOTH_WAVE 3
#define AM_WAVE 4
#define FM_WAVE 5
#define TRIANGLE_WAVE 6
#define NOISE_WAVE 7
#define DC_WAVE 8
#define NUMBER_OF_WAVEFORMS 9

#define DUTY_CAL 0.03		//used to set duty cycle to right percentage

#define WAVE_BUTTON	BIT3
#define PERIOD_BUTTON BIT4
#define DUTY_BUTTON BIT5

int sample_count;
int offset = 2047;
int amplitude = 2047;
int square_compare;
int triangle_compare_pos;
int triangle_compare_neg;

int wave_flag,period_flag,duty_flag,DAC_flag,break_flags;
int period_state=0;

int sine_val[] = {2048,2305,2557,2802,3035,3252,3450,3626,3777,3901,3996,4060,4092,4092,4060,3996,3901,3777,3626,3450,3252,3035,2802,2557,2305,2048,1791,1539,1294,1061,844,646,470,319,195,100,36,4,4,36,100,195,319,470,646,844,1061,1294,1539,1791,2048};
int am_val[] = {1056,2284,2070,621,122,1677,3419,2935,838,156,2060,4024,3313,907,163,2060,3867,3058,804,138,1677,3008,2268,567,92,1056,1775,1246,287,43,435,640,380,71,8,52,36,3,1,1,52,193,258,104,26,435,1052,1048,341,72,1056};
int fm_val[] = {4012,169,133,2038,2134,3295,49,72,3244,3321,2102,693,766,3993,4021,888,1854,1950,3999,3967,118,3090,3171,3260,3181,84,3927,3963,2058,1962,801,4047,4024,852,775,1994,3403,3330,103,75,3208,2242,2146,97,129,3978,1006,925,836,915,4012};

int period[] = {10000,5000,3333,2500,2000};

float duty[] = {0.5+DUTY_CAL,0.6+DUTY_CAL,0.7+DUTY_CAL,0.8+DUTY_CAL,0.9+DUTY_CAL,0.1+DUTY_CAL,0.2+DUTY_CAL,0.3+DUTY_CAL,0.4+DUTY_CAL};

int main(void)
{
 	WDTCTL = WDTPW + WDTHOLD;                 // Stop WDT
	_BIS_SR(GIE);

	P1DIR |= BIT0;                            // P1.0 output

	//Set up the three buttons as inputs and turn on interrupts for each button
	P2DIR &= ~WAVE_BUTTON & ~PERIOD_BUTTON & ~DUTY_BUTTON;
	P2IE |= WAVE_BUTTON + PERIOD_BUTTON + DUTY_BUTTON;
	P2IES &= ~WAVE_BUTTON + ~PERIOD_BUTTON + ~DUTY_BUTTON;

	DAC_setup();

	//initialize flags
	wave_flag = 1;
	duty_flag = 0;
	period_flag = 0;
	period_state = 0;

	TA0CCTL0 = CCIE;                             // CCR0 interrupt enabled

	TA0CCTL0 = 2*period[period_state]/SAMPLE_MAX;	//set initial period
	//CCR0 = ((2000)/SAMPLE_MAX)-2;

	TA0CTL = TASSEL_2 + MC_1 + TAIE + ID_3;                  // SMCLK, upmode, x8 clock divider

	sample_count = 0;
	DAC_flag = 0;
	square_compare = SAMPLE_MAX*duty[duty_flag];			//precalculate values for the square and triangle wave to reduce calculation time
	triangle_compare_pos = 4*amplitude/SAMPLE_MAX;
	triangle_compare_neg = -4*amplitude/SAMPLE_MAX;

	while(1)
	{
		//Will only send value to DAC when DAC_flag is raised
		if(DAC_flag)
		{
			if(wave_flag == SAWTOOTH_WAVE)
			{
				DAC_cipher(4095/SAMPLE_MAX*sample_count, BIT0);
			}

			else if(wave_flag == DC_WAVE)
			{
				DAC_cipher(4095*duty[duty_flag],BIT0);
			}

			if(sample_count == SAMPLE_MAX)
			{
				sample_count = 0;
			}
			sample_count++;

			DAC_flag = 0; //keep waiting until timer interrupts again
		}

		if(period_flag)
		{
			period_state++;

			if(period_state == 5) //checks to make sure if its in array
			{
				period_state = 0;
			}
			//CCR0 = ((2000*2)/SAMPLE_MAX)-2;
			TA0CCR0 = 2*period[period_state]/SAMPLE_MAX-2;
			period_flag = 0;
		}
	}
}

// Timer A0 interrupt service routine
// DAC_flag is raised to change the DAC
#pragma vector=TIMER0_A0_VECTOR
__interrupt void Timer_A (void)
{
	TA0CTL &= ~TAIFG;
	DAC_flag = 1;
}

#pragma vector=PORT2_VECTOR
__interrupt void PORT2 (void)
{
	if(P2IFG & WAVE_BUTTON)
	{
		//check for wavsdform button to be pressed
		wave_flag++;
		if(wave_flag == NUMBER_OF_WAVEFORMS) //checks to see if its greater than elements in waveform array
		{
			wave_flag = 1;
		}
	}
	else if(P2IFG & PERIOD_BUTTON)
	{
		//check for period button to be pressed
		period_flag = 1;
	}
	else if(P2IFG & DUTY_BUTTON)
	{
		duty_flag++;
		square_compare = SAMPLE_MAX*duty[duty_flag];

		if(duty_flag == 9)
		{
			duty_flag = 0;
		}
	}
	P2IFG ^= P2IFG;
}

//Sets up UCSI module for DAC communicatiosn
void DAC_setup()
{
	P2DIR |= BIT0;	// Will use BIT4 to activate /CE on the DAC
	P3SEL	= BIT0 + BIT1;	// + BIT4;	// These two lines dedicate P3.0 and P3.1 for UCB0SIMO and UCB0CLK respectively

	UCB0CTL0 |= UCCKPL + UCMSB + UCMST + UCSYNC;
	UCB0CTL1 |= UCSSEL_2;	// UCB0 will use SMCLK as the basis for

	UCB0BR0 |= 0x00;	// (low divider byte)
	UCB0BR1 |= 0x00;	// (high divider byte)
	UCB0CTL1 &= ~UCSWRST;	// Initialize USCI state machine
	return;
}

//Amplitude is value a value from 0 to 4095 and latch port is pin on PORT2 to control CS
void DAC_cipher(int amplitude, int latch_port)
{
	int DAC_code;

	DAC_code = (0x3000)|(amplitude & 0xFFF); //Gain to 1 and DAC on

	P2OUT &= ~latch_port; //Lower CS pin low
	//send first 8 bits of code
	UCB0TXBUF = (DAC_code >> 8);

	//wait for code to be sent
	while(!(UCTXIFG & UCB0IFG));

	//send last 8 bits of code
	UCB0TXBUF = (char)(DAC_code & 0xFF);

	//wait for code to be sent
	while(!(UCTXIFG & UCB0IFG));

	//wait until latch
	_delay_cycles(100); //170
	//raise CS pin
	P2OUT |= latch_port;

	return;
}
