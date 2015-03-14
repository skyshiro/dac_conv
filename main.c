#include <msp430.h>

//Latch port allows user to change the pin used to latch CS
void DAC_cipher(int amplitude, int latch_port);
void DAC_setup();

#define SAMPLE_MAX 100

int sample_count;
int offset = 1150;  //934
int amplitude = 1150;
int m;

int DAC_flag;

int main(void)
{
	int period = 10000; // in us

	WDTCTL = WDTPW + WDTHOLD;                 // Stop WDT
	WDTCTL = WDTPW + WDTHOLD;                 // Stop watchdog timer

	P2DIR |= BIT2;                            // SMCLK set out to pins
	P2SEL |= BIT2;
	P7DIR |= BIT4+BIT7;                       // MCLK set out to pins
	P7SEL |= BIT7;

	P5SEL |= BIT2+BIT3;                       // Port select XT2

	UCSCTL6 &= ~XT2OFF;                       // Enable XT2
	UCSCTL3 |= SELREF_5;                      // FLLref = XT2
	UCSCTL2 |= FLLN9+FLLN5;
											// Since LFXT1 is not used,
											// sourcing FLL with LFXT1 can cause
											// XT1OFFG flag to set
	UCSCTL4 |= SELA_2;                        // ACLK=REFO,SMCLK=DCO,MCLK=DCO
	UCSCTL1 |= DCORSEL_2;


	// Loop until XT1,XT2 & DCO stabilizes - in this case loop until XT2 settles
	do
	{
	UCSCTL7 &= ~(XT2OFFG + XT1LFOFFG + DCOFFG);
											// Clear XT2,XT1,DCO fault flags
	SFRIFG1 &= ~OFIFG;                      // Clear fault flags
	}while (SFRIFG1&OFIFG);                   // Test oscillator fault flag

	UCSCTL6 &= ~XT2DRIVE0;                    // Decrease XT2 Drive according to
											  // expected frequency
	UCSCTL4 |= SELS_5 + SELM_5;               // SMCLK=MCLK=XT2

	_BIS_SR(GIE);

	DAC_setup();

	TA0CCTL0 = CCIE;                            // CCR0 interrupt enabled
	TA0CTL = TASSEL_2 + MC_1 + TAIE + ID_2;     // SMCLK, upmode, /4 for 1 MHz timer clk,

	//Timer resoultion = T_period/(MAX_samples*T_clk)
	//For 1 us clock and T_period in us = period/SAMPLE_MAX

	TA0CCR0 = period/SAMPLE_MAX;

	sample_count = 0;
	DAC_flag = 0;
	while(1)
	{
		//DAC_flag is used to determine time to change value of DAC
		if(DAC_flag)
		{
			DAC_cipher(4095/SAMPLE_MAX*sample_count, BIT0);

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

void DAC_setup()
{
	P2DIR |= BIT0;						// Will use P2.0 to activate /CE on the DAC
	P3SEL	= BIT0 + BIT2;	// + BIT4;	// SDI on P3.0 and SCLK on P3.2

	UCB0CTL0 |= UCCKPL + UCMSB + UCMST + /* UCMODE_2 */ + UCSYNC;
	UCB0CTL1 |= UCSSEL_2;	// UCB0 will use SMCLK as the basis for

	//Divides UCB0 clk, I think @ 4 MHz CPU clk it should be fine
	//UCB0BR0 |= 0x10;	// (low divider byte)
	//UCB0BR1 |= 0x00;	// (high divider byte)
	UCB0CTL1 &= ~UCSWRST;	// **Initialize USCI state machine**
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
	_delay_cycles(25);
	//raise CS pin
	P2OUT |= latch_port;

	return;
}
