#include <msp430.h>

//Latch port allows user to change the pin used to latch CS
void DAC_cipher(int amplitude, int latch_port);

#define SAMPLE_MAX 500

int sample_count;
int offset = 1150;  //934
int amplitude = 1150;
int m;

int DAC_flag;

int main(void)
{
	int period = 10; // in ms

	WDTCTL = WDTPW + WDTHOLD;                 // Stop WDT
	_BIS_SR(GIE);

	P1DIR |= BIT0;                            // P1.0 output
	P2DIR |= BIT0;	// Will use BIT4 to activate /CE on the DAC
	P2SEL |= BIT2;
	P3SEL	= BIT0 + BIT1;

	UCB0CTL0 |= UCCKPL + UCMSB + UCMST + /* UCMODE_2 */ + UCSYNC;
	UCB0CTL1 |= UCSSEL_2;	// UCB0 will use SMCLK as the basis for

	UCB0BR0 |= 0x10;	// (low divider byte)
	UCB0BR1 |= 0x00;	// (high divider byte)
	UCB0CTL1 &= ~UCSWRST;	// **Initialize USCI state machine**

	TA0CCTL0 = CCIE;                             // CCR0 interrupt enabled
	TA0CCR0 = ((period*2000)/SAMPLE_MAX)-2;				  //convert period to us and divide by 1500 to get samples;  500-8 for 40ms period
	TA0CTL = TASSEL_2 + MC_1 + TAIE + ID_3;                  // SMCLK, upmode

	sample_count = 0;
	DAC_flag = 0;
	while(1)
	{
		//DAC_flag is used to determine time to change value of DAC
		if(DAC_flag)
		{
			//slope of triangle wave is twice the amplitude divided by half the period
			m = (2*amplitude) / (SAMPLE_MAX/2) ;
			//for half the period the waveform, it will decrease from +A to -A
			if(sample_count < SAMPLE_MAX/2)
			{
				DAC_cipher((amplitude+offset)-(m*sample_count),BIT0);
			}
			//for other half of the period it wil lincrease from -A to +A
			else if(sample_count >= SAMPLE_MAX/2)
			{
				DAC_cipher((offset-amplitude)+(m*(sample_count-SAMPLE_MAX/2)),BIT0);
			}
			sample_count++;
			if(sample_count == SAMPLE_MAX)
			{
				sample_count = 0;
			}

			DAC_flag = 0;
		}
	}
}

// Timer A0 interrupt service routine
// DAC_flag is raised in ISR to change DAC value
#pragma vector=TIMER0_A0_VECTOR
__interrupt void Timer_A (void)
{
	//int timerVal = TAR;
	TA0CTL &= ~TAIFG;
	DAC_flag = 1;
}

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
	_delay_cycles(200); //170
	//raise CS pin
	P2OUT |= latch_port;
}
