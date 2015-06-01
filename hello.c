#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <stdio.h>
#include <string.h>
// F_CPU in makefile

// Pin defintions
#define MOT1 PB0
#define MOT2 PB1
#define IRIN PB2
#define LED1 PB3
#define LED2 PB4
#define RSTB PB5

// IR Pulse definitions
#define THRESH_LONG   30
#define THRESH_HL     15
#define THRESH_SHORT  5
#define PULSE_LONG    0
#define PULSE_HIGH    1
#define PULSE_LOW     2
#define PULSE_SHORT   3
#define TEST_PULSE(x) (x>THRESH_SHORT?(x>THRESH_HL?(x>THRESH_LONG?PULSE_LONG:PULSE_HIGH):PULSE_LOW):PULSE_SHORT)

// remote command decoding (all should be in reverse order)
#define KEY_FORWARD (0b0010010001010111)
#define KEY_REWIND  (0b0010110001010111)
#define KEY_PLAY    (0b0000010001010111)
#define KEY_STOP    (0b0001010001010111)

// setup eeprom writing variables
char text[12] = "#helr_wrld!\n";
char buff[12];
uint8_t eeprom_addr = 0;

void EEPROM_write(unsigned char ucAddress, unsigned char ucData)
{
  /* Wait for completion of previous write */
  while(EECR & (1<<EEPE));
  /* Set Programming mode */
  EECR = (0<<EEPM1)|(0<<EEPM0);
  /* Set up address and data registers */
  EEAR = ucAddress;
  EEDR = ucData;
  /* Write logical one to EEMPE */
  EECR |= (1<<EEMPE);
  /* Start eeprom write by setting EEPE */
  EECR |= (1<<EEPE);
}

void write_string( char * data, int length){
  int i;

  for( i=0; i<length; i++){
      if( eeprom_addr < 256 ) {
      EEPROM_write(eeprom_addr++, data[i]);
      }
  }
}


// IR commands variables
uint16_t irdata[100];
volatile uint8_t irindex=0;
uint8_t irsaved=0;

// Timing variables
volatile uint16_t t=0;
uint16_t prevtime = 0;

// Command/State variables
uint16_t tempcommand = 0;
uint16_t command = 0;
uint8_t  marker = 0;


// INT0 interrupt, don't do anything if buffer is full
ISR(INT0_vect)
{
  if( irindex < 100 ){
    irdata[irindex++] = t;
  }
}

// TIMER1 interrupt, simply count.
ISR(TIMER1_COMPA_vect)
{
  t++;
}


int main () {

  // Setup registers
  GIMSK |= _BV(INT0);  // enable INT0 interrupt
  MCUCR |= _BV(ISC00); // Interrupt on logical change of INT0

  TCCR1 |= _BV(CS10);  // no prescaler on timer1
  TCCR1 |= _BV(CTC1);  // clear timer1 on OCR1C match

  TIMSK |= _BV(OCIE1A); // Enable compare register A interrupt
  OCR1C = 100;          // sets the max value which resets the counter
  OCR1A = 100;          // actually generates the interrupt

  DDRB = (_BV(MOT1)|_BV(MOT2)|_BV(LED1)|_BV(LED2)); // setups pins
  PORTB = 0;                                        // set LED pin LOW, and turn off pullups

  //start everything at 0 to make sure...
  eeprom_addr=0;
  irindex=0;
  irsaved=0;
  
  _delay_ms(1000); //wait for a bit

  write_string(text,12);  // start by writing hello world header.
  
  while (1) {

    sei();

    // If buffer is full, do writing
    while( irindex == 100 && irindex != irsaved ){
      cli();

      // eeprom indicator
      PORTB = _BV(LED2)|_BV(MOT2);

      // record to eeprom
      sprintf( buff, "%03u\n", irdata[irsaved]-prevtime );
      write_string(buff,strlen(buff));

      // do command parse
      switch(TEST_PULSE(irdata[irsaved]-prevtime)){
      case PULSE_SHORT:
	break;
      case PULSE_HIGH:
	tempcommand |= (1<<marker++);
	break;
      case PULSE_LOW:
	marker++;
	break;
      case PULSE_LONG:
	tempcommand = 0;
	marker = 0;
      }
      if( marker == 16 ){
	command = tempcommand;
	marker=0;
	tempcommand=0;
      }

      prevtime = irdata[irsaved++];
      sei();
    }

   
    // blink one of the lights to know we're still alive
    PORTB = _BV(LED1)|_BV(MOT2);
    _delay_ms(400);
    PORTB = _BV(MOT2);
    _delay_ms(400);

    // if we have recorded enough data
    if( irsaved == 100 ) {
      cli();
      PORTB = _BV(LED1)|_BV(LED2)|_BV(MOT2);
      eeprom_addr = 253;
      write_string("\n!\n",3);

      eeprom_addr = 250;
      switch(command){
      case KEY_FORWARD:
	write_string("rew\n",4);
	break;
      case KEY_REWIND:
	write_string("ffd\n",4);
	break;
      case KEY_PLAY:
	write_string("ply\n",4);
	break;
      case KEY_STOP:
	write_string("stp\n",4);
	break;
      default:
	write_string("non\n",4);
      };

      break;
    };

  }

  for(;;){
    _delay_ms(100);
  }

  return 0;
}
