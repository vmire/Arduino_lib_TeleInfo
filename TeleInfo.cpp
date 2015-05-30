//=================================================================================================================
// The TeleInfo is a class that stores the data retrieved from the teleinfo frames, and displays them on a LCD
// Various displays are available : 
//     + Instant values : Actual color, actual mode, instant current, actual color counter, instant power
//     + To morrow color : Actual color and to morrow color when known
//     + A display for each color (blue, white, red) and both modes (HC, HP)
//
// The various displays can be directly selected by pressing the buttons placed below the LCD, so button handling
// routine that generates interrupts is also part of this class
//
//=================================================================================================================
#include "TeleInfo.h"

//=================================================================================================================
// Basic constructor
//=================================================================================================================
TeleInfo::TeleInfo(byte rxPin, byte txPin)
{
 //  Serial.begin(1200,SERIAL_7E1);
  mySerial = new SoftwareSerial(rxPin,txPin); // RX, TX
  mySerial->begin(1200);

  // variables initializations
  ADCO = "031328141543";
  OPTARIF = "----";
  ISOUSC = 0;
  BBRHCJB = 0L;  // compteur Heures Creuses Bleu  en W
  BBRHPJB = 0L;  // compteur Heures Pleines Bleu  en W
  BBRHCJW = 0L;  // compteur Heures Creuses Blanc en W
  BBRHPJW = 0L;  // compteur Heures Pleines Blanc en W
  BBRHCJR = 0L;  // compteur Heures Creuses Rouge en W
  BBRHPJR = 0L;  // compteur Heures Pleines Rouge en W
  PTEC = "----";    // Régime actuel : HPJB, HCJB, HPJW, HCJW, HPJR, HCJR
  DEMAIN = "----";  // Régime demain ; ----, BLEU, BLAN, ROUG
  HHPHC = '-';
  IINST = 0;        // intensité instantanée en A
  IMAX = 0;         // intensité maxi en A
  PAPP = 0;         // puissance apparente en VA
  MOTDETAT = "------";
}

//=================================================================================================================
// Capture des trames de Teleinfo
//=================================================================================================================
boolean TeleInfo::readTeleInfo()
{
#define startFrame 0x02
#define endFrame 0x03
#define startLine 0x0A
#define endLine 0x0D
#define maxFrameLen 280

  int comptChar=0; // variable de comptage des caractères reçus 
  char charIn=0; // variable de mémorisation du caractère courant en réception

  char bufferTeleinfo[21] = "";
  int bufferLen = 0;
  int checkSum;

  //--- wait for starting frame character 
  while (charIn != startFrame){ 
    // "Start Text" STX (002 h) is the beginning of the frame
    if (mySerial->available())
      charIn = mySerial->read() & 0x7F; // Serial.read() vide buffer au fur et à mesure
  }
  
  int sequenceNumnber= 0;    // number of information group

  //  while (charIn != endFrame and comptChar<=maxFrameLen)
  while (charIn != endFrame) {
    // tant que des octets sont disponibles en lecture : on lit les caractères
    if (mySerial->available()) {
      charIn = mySerial->read()& 0x7F;
      Serial.print(charIn);
      // incrémente le compteur de caractère reçus
      comptChar++;
      if (comptChar > maxFrameLen)
        return false;
      if (charIn == startLine)
        bufferLen = 0;
      bufferTeleinfo[bufferLen] = charIn;
      // on utilise une limite max pour éviter String trop long en cas erreur réception
      // ajoute le caractère reçu au String pour les N premiers caractères
      if (charIn == endLine) {
        checkSum = bufferTeleinfo[bufferLen -1];
        if (chksum(bufferTeleinfo, bufferLen) == checkSum) {
          // we clear the 1st character
          strncpy(&bufferTeleinfo[0], &bufferTeleinfo[1], bufferLen -3);
          bufferTeleinfo[bufferLen -3] =  0x00;
          sequenceNumnber++;
          if (! handleBuffer(bufferTeleinfo, sequenceNumnber))
            return false;
        }
        else
          return false;
      }
      else
        bufferLen++;
    }
  }
  return true;
}

//=================================================================================================================
// Frame parsing
//=================================================================================================================
//void handleBuffer(char *bufferTeleinfo, uint8_t len)
boolean TeleInfo::handleBuffer(char *bufferTeleinfo, int sequenceNumnber)
{
  // create a pointer to the first char after the space
  char* resultString = strchr(bufferTeleinfo,' ') + 1;
  boolean sequenceIsOK;

  switch(sequenceNumnber)
  {
  case 1:
    //  ADCO 031328141543 :
    if (sequenceIsOK = bufferTeleinfo[0]=='A')
      ADCO = String(resultString);
    break;
  case 2:
    // OPTARIF BBR( S
    // OPTARIF HC.. <
    if (sequenceIsOK = bufferTeleinfo[0]=='O')
      OPTARIF = String(resultString);
    break;
  case 3:
    // ISOUSC 45 ?
    if (sequenceIsOK = bufferTeleinfo[1]=='S')
      ISOUSC = atol(resultString);
    break;
  case 4:
    // BBRHCJB 000010828 0
    // HCHC 014460852 $
    if (sequenceIsOK = bufferTeleinfo[6]=='B')
      BBRHCJB = atol(resultString);
    break;
  case 5:
    // BBRHPJB 000007345 =
    // HCHP 012506372 -
    if (sequenceIsOK = bufferTeleinfo[4]=='P')
      BBRHPJB = atol(resultString);
    break;
  case 6:
    // BBRHCJW 000000000 2
    // *
    if (sequenceIsOK = bufferTeleinfo[6]=='W')
      BBRHCJW = atol(resultString);
    break;
  case 7:
    // BBRHPJW 000000000 ?
    // *
    if (sequenceIsOK = bufferTeleinfo[4]=='P')
      BBRHPJW = atol(resultString);
    break;
  case 8:
    // BBRHCJR 000000000 -
    // *
    if (sequenceIsOK = bufferTeleinfo[6]=='R')
      BBRHCJR = atol(resultString);
    break;
  case 9:
    // BBRHPJR 000000000 :
    // *
    if (sequenceIsOK = bufferTeleinfo[4]=='P')
      BBRHPJR = atol(resultString);
    break;
  case 10:
    // PTEC HPJB P
    if (sequenceIsOK = bufferTeleinfo[1]=='T')
      PTEC = String(resultString);
    break;
  case 11:
    // DEMAIN ---- "
    // *
    if (sequenceIsOK = bufferTeleinfo[1]=='E')
      DEMAIN = String(resultString);
    break;
  case 12:
    // IINST 002 Y
    if (sequenceIsOK = bufferTeleinfo[1]=='I')
      IINST =atol(resultString);
    break;
  case 13:
    // IMAX 030 B
    if (sequenceIsOK = bufferTeleinfo[1]=='M')
      IMAX =atol(resultString);
    break;
  case 14:
    // PAPP 00430 (
    if (sequenceIsOK = bufferTeleinfo[1]=='A')
      PAPP =atol(resultString);
    break;
  case 15:
    // HHPHC Y D
    if (sequenceIsOK = bufferTeleinfo[1]=='H')
      HHPHC = resultString[0];
    break;
  case 16:
    // MOTDETAT 000000 B
    if (sequenceIsOK = bufferTeleinfo[1]=='O')
      MOTDETAT = String(resultString);
    break;
  }
#ifdef debug
  if(!sequenceIsOK)
  {
    Serial.print(F("Out of sequence ..."));
    Serial.println(bufferTeleinfo);
    Serial.print(F("\n"));
  }
#endif
  return sequenceIsOK;
}

//=================================================================================================================
// Calculates teleinfo Checksum
//=================================================================================================================
char TeleInfo::chksum(char *buff, uint8_t len)
{
  int i;
  char sum = 0;
  for (i=1; i<(len-2); i++) 
    sum = sum + buff[i];
  sum = (sum & 0x3F) + 0x20;
  return(sum);
}

//=================================================================================================================
// This function displays the TeleInfo Internal counters
// It's usefull for debug purpose
//=================================================================================================================
void TeleInfo::displayTeleInfo()
{
#ifdef debug
  Serial.print(F(" "));
  Serial.println();
  Serial.print(F("ADCO "));
  Serial.println(ADCO);
  Serial.print(F("OPTARIF "));
  Serial.println(OPTARIF);
  Serial.print(F("ISOUSC "));
  Serial.println(ISOUSC);
  Serial.print(F("BBRHCJB "));
  Serial.println(BBRHCJB);
  Serial.print(F("BBRHPJB "));
  Serial.println(BBRHPJB);
  Serial.print(F("BBRHCJW "));
  Serial.println(BBRHCJW);
  Serial.print(F("BBRHPJW "));
  Serial.println(BBRHPJW);
  Serial.print(F("BBRHCJR "));
  Serial.println(BBRHCJR);
  Serial.print(F("BBRHPJR "));
  Serial.println(BBRHPJR);
  Serial.print(F("PTEC "));
  Serial.println(PTEC);
  Serial.print(F("DEMAIN "));
  Serial.println(DEMAIN);
  Serial.print(F("IINST "));
  Serial.println(IINST);
  Serial.print(F("IMAX "));
  Serial.println(IMAX);
  Serial.print(F("PAPP "));
  Serial.println(PAPP);
  Serial.print(F("HHPHC "));
  Serial.println(HHPHC);
  Serial.print(F("MOTDETAT "));
  Serial.println(MOTDETAT);
#endif
}

