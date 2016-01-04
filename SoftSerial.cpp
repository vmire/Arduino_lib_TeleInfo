#include "SoftSerial.h"

volatile int sssBaudRate;

unsigned long sssIdleTimeoutMillis = 10;

const boolean sssdebugging = true; // controls printing of debug information

//Pin
byte sssRxPin = 8;
byte receiveBitMask;
volatile uint8_t *receivePortRegister;
volatile byte pcintPreviousValue = 0;  //precedente valeur du bit. ncessaire pour tester la changement dans l'interruption de type PCI (mutualisee)

//Buffer
const byte sssBufSize = 32;
volatile byte sssBuffer[sssBufSize]; // use same buffer for send and receive
byte sssBufTail = 0;
volatile byte sssBufHead = 0;
volatile byte sssBaudTimerCount;
volatile byte sssBaudTimerFirstCount;

volatile byte sssBitCount = 0;    // counts bits as they are received
volatile byte sssParityCount = 0; // nombre de bit 1, pour la calcul de bit de parité
volatile byte sssParityBit = 0;   // bit de parité (enregistré au 8° bit)
volatile byte sssRecvByte = 0;    // holder for received byte

volatile boolean sssIsListening = false;
boolean sssReady = false;

int sssData;

char sssBegin(int baudRate, byte rxPin) {
  sssBaudRate = baudRate;
  sssRxPin = rxPin;

  sssStopAll();
  
  //  prepare Rx and Tx pins
  pinMode(sssRxPin, INPUT_PULLUP);

  receiveBitMask = digitalPinToBitMask(sssRxPin);
  uint8_t port = digitalPinToPort(sssRxPin);
  receivePortRegister = portInputRegister(port);
  
  // prepare Timer2
  // prescale TCCR2B:
  //	001 : no prescale
  //	010 : 8
  //	011 : 32
  //	100 : 64
  //	101 : 128
  //	110 : 256
  //	111 : 1024
  TCCR2A = B00000000; // set normal mode (not fast PWM) with fast PWM the clock resets when it matches and gives the wrong timing
  TCCR2B = B00000011;  // set the prescaler to 32 - this gives counts at 2usec intervals
  int prescaler = 32;
  
  if(baudRate<4800){	//TeleInfo en mode historique (par défaut est à 1200)
    TCCR2B = B00000110;  // set the prescaler to 256
    prescaler = 256;
    sssIdleTimeoutMillis = 30;
  }
    
  // set baud rate timing
  sssBaudTimerCount = F_CPU / prescaler / sssBaudRate ; // number of counts per baud
  sssBaudTimerFirstCount = (sssBaudTimerCount/2) * 3;	// number of counts after start bit (position au milieu du bit suivant) 
  
  sssDbg("F_CPU",F_CPU);
  sssDbg("prescaler ",prescaler);
  sssDbg("baudTimerCount ",sssBaudTimerCount);
  sssDbg("sssBaudTimerFirstCount ",sssBaudTimerFirstCount);
  sssDbg("receiveBitMask ",receiveBitMask);
  if(sssBaudTimerFirstCount>255) Serial.println("WARNING: prescaler is too low");
  
  sssPrepareToListen();
  sssReady = true;
}


void sssEnd() {
  sssStopAll();
  sssReady = false;
}


int sssAvailable() {
  if (! sssReady) return -1;
  
  if (! sssIsListening) {
    char ok = sssPrepareToListen();
    if (! ok) return -1;
  }

  char sssByteCount = sssBufHead - sssBufTail;
  if (sssByteCount < 0) {
    sssByteCount += sssBufSize;
  }
  
  return sssByteCount;
}


int sssRead() {
  if (! sssReady) return -1;
  
  if (! sssIsListening) {
    char ok = sssPrepareToListen();
    if (! ok)  return -1;
  }
  
  if (sssBufTail == sssBufHead) {
    return -1;
  }
  else {
    sssBufTail = (sssBufTail + 1) % sssBufSize;
    return sssBuffer[sssBufTail];
  }
}

//============================
//     Internal Functions

void sssStopAll() {
  TIMSK2 &= B11111100;  // Disable Timer2 Compare Match A Interrupt
  
  //Disable Pin Change Interrupt
  pinChangeInterruptDisable(sssRxPin);
  
  sssIsListening = false;
}



//TODO : il faudrait supprimer le delay() en utilisant le timer
char sssPrepareToListen() {
  sssStopAll();
  
  // check for idle period
  unsigned long sssIdleDelay = 1000000UL / sssBaudRate / 2; //intervalle de check (en usec) : 1/2 de bit
  unsigned long sssStartMillis = millis();
  byte sssIdleCount = 0;
  byte sssMinIdleCount = 44; // on considère que c'est bon lorsqu'on n'a rien pendant 20 bits (x2 pour le compteur, plus un peu plus)
  
  while (millis() - sssStartMillis < sssIdleTimeoutMillis) {
    sssIdleCount ++;
    if ( readRxPin() == 0 ) {
      //Rx n'est pas à 1 (pas au repos) : on recommence l'attente à zero
      sssIdleCount = 0; // start counting again
    }
    
    //  if a valid idle period is found
    if (sssIdleCount >= sssMinIdleCount) {
      // reset receive buffer
      sssBufHead = 0;
      sssBufTail = 0;
      
      // set startBit interrupt
      pinChangeInterruptEnable(sssRxPin);

      sssIsListening = true;
      break;
    }
    delayMicroseconds(sssIdleDelay);
  }
  sssDbg("LISTENING:  ", sssIsListening);
  return sssIsListening;
}

void sssDbg( char dbgStr[], unsigned long dbgVal) {
  if (sssdebugging) {
    Serial.print(dbgStr);
    Serial.println(dbgVal);
  }
}

//=====================================
//       Interrupt Routines

//called when a start bit is detected
void handleStartBitInterrupt(){
  OCR2A = TCNT2 + sssBaudTimerFirstCount; // sets up Timer2A to run for the 1.5 times the baud period
                                       //   for the first sample after the start bit
                                       //   the idea is to take the samples in the middle of the bits
                                       
  TIMSK2 &= B11111100;  // Disable compareMatchA and Overflow                                     
  TIFR2  |= B00000010;  // Clear  Timer2 Compare Match A Flag - not sure this is essential
  TIMSK2 |= B00000010;  // Enable Timer2 Compare Match A Interrupt
    
  // set counters and buffer index
  sssBitCount = 0;
  sssParityCount = 0;
  sssRecvByte = 0;
  
  sssData = 0;
  
  //Serial.print(TCNT2);Serial.print("/");Serial.println(OCR2A);
}

// Enable Pin change interrupt for a pin, can be called multiple times
void pinChangeInterruptEnable(byte pin){
	pcintPreviousValue = readRxPin();
	
    bitSet(*digitalPinToPCMSK(pin) , digitalPinToPCMSKbit(pin));  // enable pin
    bitSet(PCIFR , digitalPinToPCICRbit(pin)); // clear any outstanding interrupt flag
    bitSet(PCICR , digitalPinToPCICRbit(pin)); // enable interrupt for the group
}
// Disable Pin change interrupt for a pin, can be called multiple times
void pinChangeInterruptDisable(byte pin){
    bitClear(*digitalPinToPCMSK(pin) , digitalPinToPCMSKbit(pin));  // disable pin in PinChangeMask
    bitSet(PCIFR , digitalPinToPCICRbit(pin)); // clear any outstanding interrupt flag
    bitClear(PCICR , digitalPinToPCICRbit(pin)); // disable interrupt for the group
    PCICR &= ! (bit (digitalPinToPCICRbit(pin))); 
}

void handlePinChangeInterrupt(){
  byte b = readRxPin();
  
  if(b==0 && pcintPreviousValue==1){
    //FALLING edge on RX pin : start bit : turn off the startBit interrupt
    pinChangeInterruptDisable(sssRxPin);
    handleStartBitInterrupt();
  }
  pcintPreviousValue = b;
}

ISR(PCINT1_vect){ //pin change interrupt for A0 to A5
	handlePinChangeInterrupt();
}
//ISR_ALIAS(PCINT0_vect,PCINT1_vect); //pin change interrupt for D8 to D13
//ISR_ALIAS(PCINT2_vect,PCINT1_vect); //pin change interrupt for D0 to D7


ISR(TIMER2_COMPA_vect) {
  // this is called by the bitInterval timer
  sssTimerGetBitsISR();
}

/**
 * Protocole série TéléInfo : 
 *   un bit de start correspondant à un "0" logique,
 *   7 bits pour représenter le caractère en ASCII, LSB en premier
 *   1 bit de parité: parité paire
 *   un bit de stop correspondant à un "1" logique.
 */
void sssTimerGetBitsISR() {
  // read the bit value first
  byte sssNewBit = readRxPin();
  sssData |= sssNewBit << sssBitCount;
  sssBitCount++;
  
  // update the counter for the next bit interval
  //OCR2A = TCNT2 + sssBaudTimerCount;
  byte tmp = TCNT2;
  OCR2A += sssBaudTimerCount; // sets up Timer2A to run for the 1.5 times the baud period
                                       //   for the first sample after the start bit
                                       //   the idea is to take the samples in the middle of the bits
  //Serial.print(sssBitCount);Serial.print(" ");Serial.print(sssNewBit);Serial.print(" /");Serial.println(tmp);
  
  if (sssBitCount == 9) {   // 9° bit : stop bit
    if(sssNewBit != 1){
    	//Serial.println(" <stop is 0>");
    	//sssStopAll(); return;
    }
    
    //parity check
    if(sssParityBit == (sssParityCount & B00000001)){  //Parité paire : pas bon si le bit est égal à la parité impaire
      //Erreur de parité
      //Serial.println(F(" Erreur de parité"));
    }
	
     // stop the bitIntervalTimer
     TIMSK2 &= B11111100;  // Disable Timer2 Compare Match A  and overflow Interrupts
  
    //Enable Interrupt to detect the next start bit
    pinChangeInterruptEnable(sssRxPin);
  }
  
  else if (sssBitCount == 8) {  // 8° bit : bit de parité
    sssParityBit = sssNewBit;
  }

  else if(sssBitCount<=7){
    //C'est un bit de data
    //Pour le calcul de parité
    if(sssNewBit==1) sssParityCount += 1;  
    //on le positionne dans le byte
    sssNewBit = sssNewBit << (sssBitCount-1); // moves it to the correct place
    sssRecvByte |= sssNewBit;
    
    if (sssBitCount == 7) {  // 7° bit : dernier bit de data
	  // update the bufHead index but prevent overwriting the tail of the buffer
	  byte sssTestHead = (sssBufHead + 1) % sssBufSize;
	  if (sssTestHead != sssBufTail) { // otherwise sssBufHead is unchanged
	    sssBufHead = sssTestHead;
	  }
	  // and save the byte
	  sssBuffer[sssBufHead] = sssRecvByte;
    }
  }
}

byte readRxPin(){
	//return (PINC & B00000010) == 0 ? 0 : 1;	//A1
	return (*receivePortRegister & receiveBitMask) == 0 ? 0 : 1;
}



