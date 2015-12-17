// This file sss.ino is a very simple implementation of Software Serial intended for use on
//   an Arduino Uno.
//
// It is intended to be included in the same directory as the Arduino program that
//    uses it. It will then appear as a second tab in the Arduino IDE.
//
// This code is written for the Uno or an Atmega 328
//  it assumes a clock speed of 16MHz
//  it works at 9600 baud and probably also at 4800 and 19200
//  it is only designed for the 8N1 protocol
//
//  it can only Listen or Talk - not both at the same time
//
//  Rx is pin 3  (INT 1)
//  Tx is pin 4
//
//  The user functions are
//     char sssBegin(int baudRate)  
//     void sssEnd()
//     byte sssAvailable()             // returns number of bytes in buffer
//     int  sssRead()                  // returns next byte or -1 if buffer is empty
//     char sssWrite( byte sssInByte)  // sends 1 byte or returns -1 if it can't send
//                                     //   won't accept byte if previous send is not finished
//     char sssWrite( byte sssInByte[], byte sssNumInBytes)  // sends NumInBytes from byte array
//
//  Calling any other function will likely cause problems
//
//  Read and Write share the same buffer
//  A write will clear the buffer before writing
//  A first read after a write will clear the buffer before starting to listen
//
//  Arduino Resources used ...
//    Rx : Any pin
//    Tx : Pin 4
//    Timer2A
//    Timer2 prescaler


//=====================================
//    variables - not accessible by user

volatile int sssBaudRate;


const byte sssBufSize = 32;
const unsigned long sssIdleTimeoutMillis = 10;

const boolean sssdebugging = false; // controls printing of debug information

const byte sssRxPin = 8; // the pin for INT1
const byte sssTxPin = 4;

uint8_t receiveBitMask;
volatile uint8_t *receivePortRegister;
uint8_t transmitBitMask;
volatile uint8_t *transmitPortRegister;

volatile byte pcintPreviousValue = 0;  //La prcdente valeur du bit. ncessaire pour tester la changement dans l'interruption de type PCI (mutualisee)

byte sssBufTail = 0;
volatile byte sssBufHead = 0;
volatile byte sssBuffer[sssBufSize]; // use same buffer for send and receive
byte sssBaudTimerCount;
byte sssBaudTimerFirstCount;

volatile byte sssBitCount = 0; // counts bits as they are received
volatile byte sssRecvByte = 0; // holder for received byte

volatile boolean sssIsListening = false;
boolean sssReady = false;

byte sssNumBytesToSend = 0;
byte sssBufferPos = 0; // keeps track of byte to send within the buffer
volatile byte sssNextByteToSend;
volatile byte sssNextBit;
volatile byte sssBitPos = 0; // position of bit within byte being sent

void (*sssTimerFunctionP)(void); // function pointer for use with ISR(TIMER2_COMPA_vect)

volatile unsigned long sssCurIdleMicros;
volatile unsigned long sssPrevIdleMicros;
unsigned long sssIdleIntervalMicros;


//=====================================
//    user accessible functions
//===========
char sssBegin(int baudRate) {
  sssBaudRate = baudRate;
  
  sssDbg("Beginning  ", 0);

  sssStopAll();
  
  //  prepare Rx and Tx pins
  pinMode(sssRxPin, INPUT_PULLUP);
  pinMode(sssTxPin, OUTPUT);

  receiveBitMask = digitalPinToBitMask(sssRxPin);
  uint8_t port = digitalPinToPort(sssRxPin);
  receivePortRegister = portInputRegister(port);
  
  // prepare Timer2
  TCCR2A = B00000000; // set normal mode (not fast PWM) with fast PWM the clock resets when it matches and gives the wrong timing
  TCCR2B = B00000011;  // set the prescaler to 32 - this gives counts at 2usec intervals
  
  //Nombre de microsecondes par increment du compteur 2
  int usPerCount = 32*1000000/F_CPU;  //timer count duration (9600 : 2usecs)
  
  // set baud rate timing
  sssBaudTimerCount = 1000000UL / sssBaudRate / usPerCount; // number of counts per baud
  sssBaudTimerFirstCount = sssBaudTimerCount; // I thought a longer period might be needed to get into
                                              // the first bit after the start bit - but apparently not
  // normally an interrupt occurs after sssBaudTimerCount * 2 usecs (104usecs for 9600 baud)

  // length of required idle period to synchronize start bit detection
  sssIdleIntervalMicros = 1000000UL / sssBaudRate * 25 ; // 2.5 byte lengths
  
  sssDbg("idleCount ", sssIdleIntervalMicros);
  sssDbg("baudTimer ",sssBaudTimerCount);
  
  sssPrepareToListen();
  sssReady = true;

}

//============

void sssEnd() {
  sssStopAll();
  sssReady = false;
}

//============

int sssAvailable() {
  if (! sssReady) return -1;
  if (sssNumBytesToSend > 0) return -1; // only accept it if sender is idle
  
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

//============

int sssRead() {
  if (! sssReady) return -1;
  if (sssNumBytesToSend > 0)  return -1; // only accept it if sender is idle
  
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


//============

char sssWrite( byte sssInByte) { // User calls this
  if (! sssReady)  return -1;
  if (sssNumBytesToSend > 0)  return -1; // only accept it if sender is idle
   
  sssPrepareToTalk();
   
  sssBuffer[0] = sssInByte;
  sssBufferPos = 0;
  sssNumBytesToSend = 1; // this is the statement that triggers the ISR to send so it should be the last thing here
  return 0;
}

//============

char sssWrite( byte sssInByte[], byte sssNumInBytes) { 

  if (! sssReady)  return -1;
  if (sssNumBytesToSend > 0) return -1; // only accept it if sender is idle
  
  sssPrepareToTalk();

  if (sssNumInBytes > sssBufSize) return -1;
   
   for (byte n = 0; n < sssNumInBytes; n++) {
      sssBuffer[n] = sssInByte[n];
   }
   sssBufferPos = 0;
   sssNumBytesToSend = sssNumInBytes;
   
   return 0;
}


//============================
//     Internal Functions


// Enable Pin change interrupt for a pin, can be called multiple times
void pinChangeInterruptEnable(byte pin){
    bitSet(*digitalPinToPCMSK(pin) , digitalPinToPCMSKbit(pin));  // enable pin
    bitSet(PCIFR , digitalPinToPCICRbit(pin)); // clear any outstanding interrupt flag
    bitSet(PCICR , digitalPinToPCICRbit(pin)); // enable interrupt for the group
}
// Disable Pin change interrupt for a pin, can be called multiple times
void pinChangeInterruptDisable(byte pin){
    bitClear(*digitalPinToPCMSK(pin) , digitalPinToPCMSKbit(pin));  // disable pin in PinChangeMask
    bitSet(PCIFR , digitalPinToPCICRbit(pin)); // clear any outstanding interrupt flag
    bitClear(PCICR , digitalPinToPCICRbit(pin)); // disable interrupt for the group
}

void sssStopAll() {
  TIMSK2 &= B11111100;  // Disable Timer2 Compare Match A Interrupt
  
  if(digitalPinToInterrupt(sssRxPin) == 1){
    // Disable Interrupt 1
    EIMSK &=  B11111101;
  }
  else if(sssRxPin == 8){
    //Disable Pin Change Interrupt
    pinChangeInterruptDisable(sssRxPin);
  }
  sssIsListening = false;
}

//============



  
char sssPrepareToListen() {
  sssStopAll();
  
  // check for idle period
  unsigned long sssIdleDelay = 1000000UL / sssBaudRate / 2; // usecs between checks - half the baud interval
  unsigned long sssStartMillis = millis();
  byte sssIdleCount = 0;
  byte sssMinIdleCount = 44; // 20 bits checked at half the bit interval plus a little extra
  
  //sssDbg("idleDelay ", sssIdleDelay);
  //sssDbg("baudTimer ",sssBaudTimerCount);
  
  while (millis() - sssStartMillis < sssIdleTimeoutMillis) {
    sssIdleCount ++;
    if ( ! *receivePortRegister & receiveBitMask ) {
      sssIdleCount = 0; // start counting again
    }
    
    //  if a valid idle period is found
    if (sssIdleCount >= sssMinIdleCount) {
      // reset receive buffer
      sssBufHead = 0;
      sssBufTail = 0;
      
      // set startBit interrupt
      if(digitalPinToInterrupt(sssRxPin) == 1){
        // Enable Interrupt 1
        EICRA |= B00001000;  // External Interrupt 1 FALLING
        EIFR  |= B00000010; // clear interrupt 1 flag - prevents any spurious carry-over
        EIMSK |= B00000010;  // enable Interrupt 1
      }
      else if(sssRxPin == 8){
        //Enable Pin Change Interrupt
        pinChangeInterruptEnable(sssRxPin);
      }
      
      
      sssTimerFunctionP = &sssTimerGetBitsISR;

      sssIsListening = true;
    }
    delayMicroseconds(sssIdleDelay);
  }
  sssDbg("LISTENING:  ", sssIsListening);
  return sssIsListening;
}

//============

void sssPrepareToTalk() {
  sssDbg("Preparing to Talk", 0);

  sssStopAll();
  
  // get stuff ready
  digitalWrite(sssTxPin, HIGH); // high = idle
  
  // set the appropriate interrupt function
  sssTimerFunctionP = &sssSendNextByteISR;
  
  // start the bitInterval timer
  noInterrupts();
    OCR2A = TCNT2 + sssBaudTimerCount;
    TIMSK2 &= B11111100; // Disable compareMatchA and Overflow                                     
    TIFR2  |= B00000010;  // Clear  Timer2 Compare Match A Flag - not sure this is essential
    TIMSK2 |= B00000010;  // Enable Timer2 Compare Match A Interrupt
  interrupts();
  
}


//============

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
  // start the bitInterval timer
  OCR2A = TCNT2 + (sssBaudTimerFirstCount); // sets up Timer2A to run for the 1.5 times the baud period
                                       //   for the first sample after the start bit
                                       //   the idea is to take the samples in the middle of the bits
  TIMSK2 &= B11111100; // Disable compareMatchA and Overflow                                     
  TIFR2  |= B00000010;  // Clear  Timer2 Compare Match A Flag - not sure this is essential
  TIMSK2 |= B00000010;  // Enable Timer2 Compare Match A Interrupt
    
  // set counters and buffer index
  sssBitCount = 0;
  sssRecvByte = 0;
}


ISR(INT1_vect) {
  if(digitalPinToInterrupt(sssRxPin) == 1){
    // turn off the startBit interrupt
    EIMSK &=  B11111101;  // Disable Interrupt 1
    handleStartBitInterrupt();
  }
}

ISR(PCINT0_vect) //pin change interrupt for D8 to D13 here
{
  if(digitalPinToInterrupt(sssRxPin) >= 0) return; //l'interruption du les interruptions 0 et 1 sont traites par leurs interruptions respectives
  byte b = *receivePortRegister & receiveBitMask;
  if(b && !pcintPreviousValue || !b && pcintPreviousValue) return;  //Pas de changement
  if(!b){
    //FALLING
    // turn off the startBit interrupt
    pinChangeInterruptDisable(sssRxPin);
    handleStartBitInterrupt();
  }
  pcintPreviousValue = b;
}
ISR_ALIAS(PCINT1_vect,PCINT0_vect); //pin change interrupt for A0 to A5 here
ISR_ALIAS(PCINT2_vect,PCINT0_vect); //pin change interrupt for D0 to D7 here

//=============

ISR(TIMER2_COMPA_vect) {
  // this is called by the bitInterval timer
  sssTimerFunctionP(); // this is a function pointer and will call different functions
                       // Points to one of sssTimerGetBitsISR()
                       //                  sssSendNextByteISR()
                       //                  sssSendBitsISR()
}



//=============

void sssTimerGetBitsISR() {
  // This is one of the functions called by TimerFunctionP()
  // read the bit value first
  //byte sssNewBit = PIND & B00001000; // Arduino Pin 3 is pin3 in Port D
  byte sssNewBit = *receivePortRegister & receiveBitMask;
  
  // update the counter for the next bit interval
  OCR2A = TCNT2 + sssBaudTimerCount;
  
  
  if (sssBitCount == 8) {   // this is the stop bit
     // stop the bitIntervalTimer
     TIMSK2 &= B11111100;  // Disable Timer2 Compare Match A  and overflow Interrupts
  
    // enable Interrupt to detect the next start bit
    if(digitalPinToInterrupt(sssRxPin) == 0){
      // Enable Interrupt 0
      //TODO...
    }
    else if(digitalPinToInterrupt(sssRxPin) == 1){
      // Enable Interrupt 1
      EICRA |= B00001000; // External Interrupt 1 FALLING
      EIFR  |= B00000010; // clear interrupt 1 flag - prevents any spurious carry-over
      EIMSK |= B00000010; // enable Interrupt 1
    }
    else if(sssRxPin == 8){
      //Enable Pin Change Interrupt
      pinChangeInterruptEnable(sssRxPin);
    }
  }
  
  sssNewBit = (sssNewBit ? 1:0);
  sssNewBit = sssNewBit << sssBitCount; // moves it to the correct place
  sssRecvByte += sssNewBit;


  if (sssBitCount == 7) {
     // update the bufHead index
     // but prevent overwriting the tail of the buffer
     byte sssTestHead = (sssBufHead + 1) % sssBufSize;
     if (sssTestHead != sssBufTail) { // otherwise sssBufHead is unchanged
       sssBufHead = sssTestHead;
     }
     // and save the byte
     sssBuffer[sssBufHead] = sssRecvByte;
  }
  
  sssBitCount ++;

}

//============


void sssSendNextByteISR() {
    // this all happens while the line is idle

    OCR2A = TCNT2 + sssBaudTimerCount; 

    if (sssNumBytesToSend > 0) {
    
      sssNextByteToSend = sssBuffer[sssBufferPos]; 

      sssBitPos = 0;
      
      noInterrupts();
      sssTimerFunctionP = &sssSendBitsISR; // switch interrupt to the sendBits function
      interrupts();
    }
    // otherwise let interrupt keep checking if there are bytes to send
}

//===========

void sssSendBitsISR() {
     OCR2A = TCNT2 + sssBaudTimerCount;

     if (sssBitPos == 9) { // this is the  stop bit
       PORTD |= B00010000;
       
       noInterrupts();
       sssTimerFunctionP = &sssSendNextByteISR; // switch interrupt to the sendNextByte function
       interrupts();
       
       sssBufferPos ++; // update the byte index
       sssNumBytesToSend --;
       
     }
     else if (sssBitPos > 0) {
         sssNextBit = sssNextByteToSend & B00000001;
     
         if (sssNextBit) {
            PORTD |= B00010000;
         }
         else {
            PORTD &= B11101111;
         }

         sssNextByteToSend = sssNextByteToSend >> 1;
         sssBitPos ++;
     }
     else { // this is the start bit
        PORTD &= B11101111;
        sssBitPos ++;
     }
}


//========END=============
