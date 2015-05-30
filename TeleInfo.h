#ifndef TeleInfo_h
#define TeleInfo_h

#define nodebug

#include "Arduino.h"
#include <SoftwareSerial.h>

class TeleInfo {
public:
  TeleInfo(byte rxPin, byte txPin);
  boolean readTeleInfo();
  void displayTeleInfo();
  
private :
  SoftwareSerial* mySerial;

  char HHPHC;
  
  int ISOUSC;             // intensité souscrite  
  int IINST;              // intensité instantanée en A
  int IMAX;               // intensité maxi en A
  int PAPP;               // puissance apparente en VA
  
  unsigned long BBRHCJB;  // compteur Heures Creuses Bleu  en W
  unsigned long BBRHPJB;  // compteur Heures Pleines Bleu  en W
  unsigned long BBRHCJW;  // compteur Heures Creuses Blanc en W
  unsigned long BBRHPJW;  // compteur Heures Pleines Blanc en W
  unsigned long BBRHCJR;  // compteur Heures Creuses Rouge en W
  unsigned long BBRHPJR;  // compteur Heures Pleines Rouge en W
  
  String PTEC;            // Régime actuel : HPJB, HCJB, HPJW, HCJW, HPJR, HCJR
  String DEMAIN;          // Régime demain ; ----, BLEU, BLAN, ROUG
  String ADCO;            // adresse compteur
  String OPTARIF;         // option tarifaire
  String MOTDETAT;        // status word

  char chksum(char *buff, uint8_t len);
  boolean handleBuffer(char *bufferTeleinfo, int sequenceNumnber);
};
#endif
