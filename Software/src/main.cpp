/*
 * LED-Strip Light Organ with 3 channels
 *
 * Bass:  level at ADC0 (PC0, Arduino Nano: A0) determines PWM value at OC0A (PD6, Arduino Nano: D6),
 *        associated overload signal at PD4 (PCINT20, Arduino Nano: D4)
 * Middle: level at ADC1 (PC1, Arduino Nano: A1) determines PWM value at OC0B (PD5, Arduino Nano: D5)
 *         associated overload signal at PB0 (PCINT0, Arduino Nano: D8)
 * Treble: level at ADC2 (PC2, Arduino Nano: A2) determines PWM value at OC2A (PB3, Arduino Nano: D11)
 *         associated overload signal at PB4 (PCINT4, Arduino Nano: D12)
 * Overload: LED at PB2 (Arduino Nano: D10)
 *           A single channel stays off for 5 sec after detecting a current overload.
 *           After detecting thermal overload (ca. 50° celsius) ALL channels stay off until temperature
 *           drops below 40° celsius or after reboot when temp < 50 deg.
 * Temperature control: LM35 delivers 0mV + 10mV per deg celsius at ADC6 (Arduino Nano: A6). 
 * Mode Switch: connected to PB1 (Arduino Nano: D9) for selecting operation mode.
 *
 * Created: 07.07.2017
 * Author : Thomas Jentzsch (yellobyte@bluewin.ch)
 */ 

#include <Arduino.h>

#define NUM_WORKING_MODES 3
#define NUM_CHANNELS      3
#define OL_TIMER_VALUE    5000          // 5s
#define TIMER_TEMPERATURE 10000         // 10s
#define ADC_REF_EXT       0
#define ADC_REF_VCC       1

volatile uint8_t overload;              // overload marker: Bit 7 for temp & Bit 2-0 for channels
volatile uint8_t overloadMessage;       // overload message to be printed
volatile uint8_t blinkStatus;           // overload LED status
volatile uint16_t timer[NUM_CHANNELS];  // timer per channel (for overload handling)
volatile uint16_t timerTemperature;     // timer for checking temp
volatile uint16_t timerMode;            // timer used in misc modes
volatile uint8_t timerBlink;
uint8_t workingMode;                    // 0 = normal light organ, 1 = bass rhythm, 2 = cyclic 

uint8_t valueOld[NUM_CHANNELS], 
        pwm, valueNew = 0, 
        i = 0, ii = 0;
uint16_t adc = 0;

// 50% PWM on an LED actually does not mean 50% brightness (it's probably more 75%), therefore we need some 
// kind of translation table for PWM -> brightness. Quite a lot of testing was required on that issue.
// strongly exponential:
/* uint8_t myExp[256] = { 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,                   // 0-15
            1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2,                                 // 16-31
            2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 3, 3,                                 // 32-47
            3, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 5,                                 // 48-63
            5, 5, 5, 5, 5, 6, 6, 6, 6, 6, 6, 7, 7, 7, 7, 7,                                 // 64-79
            8, 8, 8, 8, 9, 9, 9, 10, 10, 10, 11, 11, 11, 12, 12, 13,                        // 80-95
            13, 14, 14, 14, 15, 15, 16, 16, 17, 17, 18, 18, 19, 19, 20, 21,                 // 96-111
            21, 22, 22, 23, 24, 24, 25, 25, 26, 27, 27, 28, 29, 30, 30, 31,                 // 112-127
            32, 33, 33, 34, 35, 36, 37, 37, 38, 39, 40, 41, 42, 43, 44, 44,                 // 128-143
            45, 46, 47, 48, 49, 50, 51, 52, 54, 55, 56, 57, 58, 59, 60, 61,                 // 144-159
            62, 64, 65, 66, 67, 69, 70, 71, 72, 74, 75, 76, 78, 79, 81, 82,                 // 160-175
            83, 85, 86, 88, 89, 91, 92, 94, 95, 97, 98, 100, 102, 103, 105, 107,            // 176-191
            108, 110, 112, 114, 115, 117, 119, 121, 123, 124, 126, 128, 130, 132, 134, 136, // 192-207
            138, 140, 142, 144, 146, 148, 150, 152, 154, 157, 159, 161, 163, 165, 168, 170, // 208-223
            172, 175, 177, 179, 182, 184, 187, 189, 192, 194, 197, 199, 202, 204, 207, 209, // 224-239
            212, 215, 217, 220, 223, 226, 228, 231, 234, 237, 240, 243, 246, 249, 252, 255  // 240-255
};*/

// somewhat exponential - makes a good visual impression
uint8_t myExp[256] = { 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
            1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3,
            4, 4, 4, 4, 5, 5, 5, 5, 6, 6, 6, 7, 7, 7, 8, 8,
            9, 9, 9, 10, 10, 11, 11, 11, 12, 12, 13, 13, 14, 14, 15, 15,
            16, 16, 17, 17, 18, 18, 19, 19, 20, 20, 21, 22, 22, 23, 23, 24,
            25, 25, 26, 27, 27, 28, 29, 29, 30, 31, 31, 32, 33, 33, 34, 35,
            36, 36, 37, 38, 39, 40, 40, 41, 42, 43, 44, 44, 45, 46, 47, 48,
            49, 50, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63,
            64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 79, 80,
            81, 82, 83, 84, 85, 87, 88, 89, 90, 91, 93, 94, 95, 96, 97, 99,
            100, 101, 102, 104, 105, 106, 108, 109, 110, 112, 113, 114, 116, 117, 118, 120,
            121, 122, 124, 125, 127, 128, 129, 131, 132, 134, 135, 137, 138, 140, 141, 143,
            144, 146, 147, 149, 150, 152, 153, 155, 156, 158, 160, 161, 163, 164, 166, 168,
            169, 171, 172, 174, 176, 177, 179, 181, 182, 184, 186, 188, 189, 191, 193, 195,
            196, 198, 200, 202, 203, 205, 207, 209, 211, 212, 214, 216, 218, 220, 222, 224,
            225, 227, 229, 231, 233, 235, 237, 239, 241, 243, 245, 247, 249, 251, 253, 255
};

/* alternatives, abandoned in favor of fixed array
uint8_t myExp(float in) {
  float pwmf = (float) (in / 255);
  //pwmf = pwmf * pwmf  pwmf;
  pwmf = pwmf * pwmf;
  return pwmf * 255;
}
*/

void ADCInit(uint8_t reference) {
	
  if (reference == ADC_REF_EXT) {
    ADMUX &= ~((1<<REFS1)|(1<<REFS0));  // Use external reference
  }
  else {
    ADMUX |= (1<<REFS0);                // Set reference to AVcc
    ADMUX &= ~(1<<REFS1);
  }
  ADCSRA |= (1<<ADEN)|                        // Enable ADC
            (1<<ADPS2)|(1<<ADPS1)|(1<<ADPS0); // Set prescaler to 128
                                              // Fadc=Fcpu/prescaler=16000000/128=125kHz
                                              // Fadc should be between 50kHz and 200kHz
  // in case interrupt routine is used
  /*										
  ADCSRA |= (1<<ADIE)|                  // interrupt enabled
            (1<<ADFR) or (1<<ADATE);    // for free running (depending on chip)
  ADCSRA |= (1<<ADSC);                  // Start first conversion
  */
}

uint16_t ADCRead(uint8_t channel) {

#if defined(__AVR_ATmega168__) || defined(__AVR_ATmega168P__) || defined(__AVR_ATmega328__) || defined(__AVR_ATmega328P__)
  if (channel > 7) return 0;
#else	
  if (channel > 5) return 0;
#endif
  ADMUX &= ~((1<<MUX2)|(1<<MUX1)|(1<<MUX0));
  ADMUX |= (channel & 0x07);
  ADCSRA |= (1<<ADSC);                // start Single conversion
  while (!(ADCSRA & (1<<ADIF)));      // wait for conversion to complete
  ADCSRA |=(1<<ADIF);                 // clear ADIF by writing one to it
	
  return (ADC);
}

/*
// Interrupt routine for ADC conversion complete, not used
ISR(ADC_vect)
{
  ...
  erg = ADC; 
  ...
}
*/

// helper function
long map(long x, long in_min, long in_max, long out_min, long out_max)
{
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

// checking switch for mode selection, returns 1 when falling edge on PB1 detected
uint8_t switchCheck(void)
{
  static uint8_t  portBuffer, 
                  oldStatus = 1;  // switch not pressed
  uint8_t ret = 0;
  uint8_t counter;

  if ((PINB & (1<<PB1)) != (portBuffer & (1<<PB1))) {
    // switch status has changed
    for ( counter=0 ; counter<6 ; ) {
      portBuffer = PINB;
      _delay_us(150);
      if( (PINB & (1<<PB1)) == (portBuffer & (1<<PB1)) )
        counter++;    // stable
      else
        counter = 0;  // not stable yet
    }
    // stable for 6 * 150us = 900us
    if (oldStatus && !(portBuffer & (1<<PB1))) {  // falling edge ?
      ret = 1;
    }
    oldStatus = (portBuffer & (1<<PB1)) ? 1 : 0;  // remember last stable level
  }

  return(ret);
}

ISR(PCINT0_vect)
{
  if ((PINB & (1<<PB0)) != 0)
  {
    // LOW to HIGH at PB0 (Arduino Nano: D8, overload signal channel B)
    OCR0B = 0;                    // channel B: instant PWM off
    overload |= (1<<1);
    if (timer[1] == 0) {          // new overload only when old expired
      timer[1] = OL_TIMER_VALUE;  // otherwise IR2125 will trigger a message each PWM signal
      overloadMessage |= (1<<1);  // trigger message
    }
  }
  if ((PINB & (1<<PB4)) != 0)
  {
    // LOW to HIGH at PB4 (Arduino Nano: D12, overload signal channel C)
    OCR2A = 0;                    // channel C: instant PWM off
    overload |= (1<<2);
    if (timer[2] == 0) {          // new overload only when old expired
      timer[2] = OL_TIMER_VALUE;  // otherwise IR2125 will trigger a message each PWM signal
      overloadMessage |= (1<<2);  // trigger message	
    }
  }
}

ISR(PCINT2_vect)
{
  if ((PIND & (1<<PD4)) != 0)
  {
    // LOW to HIGH at PD4 (Arduino Nano: D4, overload signal channel A)
    OCR0A = 0;                    // channel A: instant PWM off
    overload |= (1<<0);
    if (timer[0] == 0) {          // new overload only when old expired
      timer[0] = OL_TIMER_VALUE;  // otherwise IR2125 will trigger a message each PWM signal
      overloadMessage |= (1<<0);  // trigger message	
    }
  }
}

//ISR(TIMER1_OVF_vect){
ISR(TIMER1_COMPA_vect){
  if (timer[0]) --timer[0];
  if (timer[1]) --timer[1];
  if (timer[2]) --timer[2];
  if (timerTemperature) --timerTemperature;
  if (timerMode) --timerMode;
  if (timerBlink) --timerBlink;
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(38400);

  overload = overloadMessage = 0;
  timerTemperature = TIMER_TEMPERATURE;
  timerMode = timerBlink = 0;
  workingMode = 0;
  for ( i=0; i<NUM_CHANNELS; i++) {
    valueOld[i] = 0;	
  }
	
  // port settings
  DDRD = ((1<<PD6) | (1<<PD5));           // PD6(D6) & PD5(D5) PWM output (OC0A & OC0B)
  DDRB = ((1<<PB3) | (1<<PB2));           // PB3(D11) PWM output (OC2A) & PB2(D10) output overload LED
  PORTB |= (1<<PB1);                      // activate pull-up for switch at PB1 (D9)
  PORTB &= ~(1<<PB2);                     // overload LED at PB2 (D10) off
	
  // initialize timer 0 (Ch A+B) & timer 2 (Ch C) for PWM
  OCR0A = OCR0B = OCR2A = 0;

  TCCR0A = (1<<WGM00);                    // phase correct PWM mode (mode 1) for timer 0 (Ch A+B)
  TCCR0A |= ((1<<COM0A1) | (1<<COM0B1));  // none inverting mode for OC0A & OC0B
  TCCR0B |= ((1<<CS01) | (1<<CS00));      // prescaler to 64 (phase correct PWM: 490Hz) & start PWM

  TCCR2A = (1<<WGM20);                    // phase correct PWM mode (mode 1) for timer 2 (Ch C)
  TCCR2A |= (1<<COM2A1);                  // none inverting mode for OC2A
  TCCR2B |= (1<<CS22);                    // prescaler to 64 (phase correct PWM: 490Hz) & start PWM

  // init timer 1
  //TCCR1B = (1<<CS11) | (1<<CS10);       // prescaler to 64 (top = 0xFFFF -> overflow irq every 0.26s)
  //TIMSK1 |= (1<<TOIE1);                 // timer 1 overflow interrupt enabled
  TCCR1B = ((1<<WGM12) | (1<<CS11) | (1<<CS10)); // prescaler to 64, CTC Mode 4 (OCR1A = TOP)
  OCR1A = 249;                            // 249: timer 1 interrupt every ms
  TIMSK1 |= (1<<OCIE1A);                  // timer 1 OC1A interrupt enabled

  // enabling pin change interrupts
  PCICR |= (1<<PCIE2) | (1<<PCIE0);					
  PCMSK2 |= (1<<PCINT20);                 // PD4 (PCINT20) overload at channel A (Bass)
  PCMSK0 |= (1<<PCINT0);                  // PB0 (PCINT0) overload at channel B (Middle)
  PCMSK0 |= (1<<PCINT4);                  // PB4 (PCINT4) overload at channel C (Treble)

  //ADCInit(ADC_REF_VCC);                 // just for testing purposes
  ADCInit(ADC_REF_EXT);                   // initialize AD converter, Uref = 4.096V
  //sei();                                // enable global interrupts - done in framework

  Serial.println(F("Program (13.04.2018) started:"));
	
  i = 0;
}

void loop() {
  // put your main code here, to run repeatedly:
  if (timerTemperature == 0) {            // 10 sec timeout ?
    timerTemperature = TIMER_TEMPERATURE;
    adc = ADCRead(6);                     // read temp sensor (0..100 deg celsius), 10mV per deg
                                          // at Uref 4.096V --> 4mV per digit --> divisor 2.5
    Serial.print(F("temp reading: "));
    Serial.print(adc);
    Serial.print(F(" -> "));
    Serial.print((uint16_t)((float)adc/2.5));
    Serial.println(F(" °C"));
    if (!(overload & (1<<7)) && (adc > 124))	{ // 0.5V/4.096V*1024
      overload |= (1<<7);                 // temp overload triggered at >50 deg
      Serial.println(F("thermal overload detected -> all channels off"));
    }
    else if ((overload & (1<<7)) && (adc < 100)) {	// 0.4V/4.096V*1024
      overload &= ~(1<<7);                // temp overload deleted at <40 deg
      Serial.println(F("thermal overload has ended"));
    }
  }
  if (overload & (1<<i)) {                // electr. overload on this channel ?
    valueNew = 0;                         // channel off
    if (!(overload & (1<<7))) PORTB |= (1<<PB2);  // LED permanently on if no overtemperature
    if (overloadMessage & (1<<i)) {       // message to be sent for this channel
      Serial.print(F("overload detected on channel "));
      Serial.println((char)('A'+i));
      overloadMessage &= ~(1<<i);	
    }
    if (timer[i] == 0) {                  // timer expired ?
      overload &= ~(1<<i);                // clear electr. overload for this channel
      Serial.print(F("overload might be gone on channel "));
      Serial.print((char)('A'+i));
      Serial.println(F(", we release it"));
    }
  }
  if (overload & (1<<7)) {                // thermal overload of system ?
    valueNew = 0;                         // channel off
    if (!timerBlink) {                    // thermal overload: LED at PB2 (D10) blinks (t=250ms)
      PORTB ^= (1<<PB2);
      timerBlink = 250;	
    }
  }
  if (!(overload & (1<<i)) && !(overload & (1<<7))) {
    // normal working condition, no overloads
    if (workingMode == 0) {			
      // read voltage level of each analog filter output on Filter-PCB
      adc = ADCRead(i);                   // ADCRead() returns 0...1023 (0...4.096V)
      // values for best visual impression obtained by practical tests
      static uint16_t lowLevel[NUM_CHANNELS] = {125,133,124};   // min level at which LEDs just switch off
      static uint16_t highLevel[NUM_CHANNELS] = {408,375,310};  // max level, at which LEDs are permanently on
      if (adc < lowLevel[i]) adc = lowLevel[i];
      if (adc > highLevel[i]) adc = highLevel[i];
      valueNew = map(adc,lowLevel[i],highLevel[i],0,255);       // new value 0..255 represents desired brightness
    }
    else if (workingMode == 1) {
      // switch to next channel when bass channel shows steep edge (rhythm mode)
      static int16_t adc0 = 0, adc1 = 0, adc2 = 0, adc3 = 0;
      _delay_ms(12);
      adc0 = adc1;
      adc1 = adc2;
      adc2 = adc3;
      adc3 = (uint16_t)ADCRead(0);        // read bass channel only
      if ((timerMode == 0) &&	
          (((adc2 > 0) && (adc3 > 0) && ((adc3 - adc2) > 107)) ||   // delta: 430mV
          ((adc1 > 0) && (adc3 > 0) && ((adc3 - adc1) > 125)) ||    // delta: 500mV
          ((adc0 > 0) && (adc3 > 0) && ((adc3 - adc0) > 144)))      // delta: 580mV
         ) {
        timerMode = 150;                  // next step in >=150ms at the earliest
        ii++;
        adc0 = adc1 = adc2 = 0;
      }
      // TEST TEST
      /*else {
        PORTB |= (1<<PB2);
        _delay_us(20);
        PORTB &= ~(1<<PB2);
      }*/
      if ((ii % NUM_CHANNELS) == i)
        valueNew = 255;
      else
        valueNew = 0;
    }
    else {  // cyclic mode
      if (timerMode == 0) {
        timerMode = 300;
        ii++;
      }
      if ((ii % NUM_CHANNELS) == i)
        valueNew = 255;
      else
        valueNew = 0;
    }
  }
	
  if (valueNew != valueOld[i]) {
    if (i != 2) {
      pwm = myExp[valueNew];            // translating PWM value to brightness (only for bass & middle frequ)
    }  
    else {
      pwm = valueNew;                   // no transaltion for treble (proved to be best solution visually)
    }  
    // write PWM value into register
    if (i == 0) {
      OCR0A = pwm;
    }  
    else if (i == 1) {
      OCR0B = pwm;
    }
    else {
      OCR2A = pwm;
    }  
    valueOld[i] = valueNew;
  }
	
  if (!overload) {                      // overload gone ?
    PORTB &= ~(1<<PB2);                 // overload LED at PB2 (D10) off
  }
	
  if (i == 0 && switchCheck()) {        // switch pressed ?
    workingMode =  (workingMode + 1) % NUM_WORKING_MODES;
    Serial.print(F("new working mode: "));
    Serial.println(workingMode);
  }

  i++;
  i %= NUM_CHANNELS;
}