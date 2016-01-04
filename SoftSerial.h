#ifndef SoftSerial_h_
#define SoftSerial_h_
// This is an implementation of Software Serial
//
//  it is only designed for the 8N1 protocol
//
//  it can only Listen or Talk - not both at the same time
//
//  Rx : any pin
//  Tx : pin 4
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

#include "Arduino.h"


char sssBegin(int baudRate, byte rxPin);
void sssEnd();
int sssAvailable();
int sssRead();
char sssWrite( byte sssInByte);
char sssWrite( byte sssInByte[], byte sssNumInBytes);

// Enable Pin change interrupt for a pin, can be called multiple times
void pinChangeInterruptEnable(byte pin);
// Disable Pin change interrupt for a pin, can be called multiple times
void pinChangeInterruptDisable(byte pin);
void sssStopAll();




char sssPrepareToListen();
void sssPrepareToTalk();
void sssDbg( char dbgStr[], unsigned long dbgVal);
void handleStartBitInterrupt();

void sssTimerGetBitsISR();
void sssSendNextByteISR();
void sssSendBitsISR();

byte readRxPin();

#endif