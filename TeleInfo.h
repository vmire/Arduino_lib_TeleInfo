#ifndef TeleInfo_h
#define TeleInfo_h

#define nodebug

#include "Arduino.h"
#include "SoftSerial.h"

#define START_FRAME 0x02				//Debut de trame
#define END_FRAME 0x03					//Fin de trame
#define END_OF_TEXT 0x04				//Interruption de la trame
#define START_GROUP 0x0A				//Debut de groupe
#define END_GROUP 0x0D					//Fin de groupe
#define CHAR_SEPARATEUR 0x20		//separateur des champs etiquette/donnees/checksum

#define STATE_IDLE 1
#define STATE_FRAME_STARTED 2
#define STATE_READ_ETIQUETTE 3
#define STATE_READ_DATA 4
#define STATE_READ_CHECKSUM 5
#define STATE_GROUP_END 6
#define STATE_FRAME_AVAILABLE 7
#define STATE_ERROR 8				 //Erreur rencontree (checksum)

class TeleInfo {
public:
	TeleInfo(byte rxPin);
	boolean readTeleInfo();
	boolean isFrameAvailable();
	void displayTeleInfo();
	void setDebug(boolean d);
	void resetData();
	
	/*
	 * Les données
	 */	
	char* ADCO;				// adresse compteur
	char* OPTARIF;				// option tarifaire : BASE / HC.. / EJP. / BBRx
	int ISOUSC;				// intensité souscrite (A)
	long BASE;				// compteur option Base (Wh)
	long HCHC;				// compteur HC  heure pleine (Wh)
	long HCHP;				// compteur HC  heure creuse (Wh)
	long EJPHN;				// compteur EJP heures normales (Wh)
	long EJPHPM;				// compteur EJP heures de pointe (Wh)
	long BBRHCJB;				// compteur TEMPO Heures Creuses Bleu (Wh)
	long BBRHPJB;				// compteur TEMPO Heures Pleines Bleu (Wh)
	long BBRHCJW;				// compteur TEMPO Heures Creuses Blanc (Wh)
	long BBRHPJW;				// compteur TEMPO Heures Pleines Blanc (Wh)
	long BBRHCJR;				// compteur TEMPO Heures Creuses Rouge (Wh)
	long BBRHPJR;				// compteur TEMPO Heures Pleines Rouge (Wh)
	int PEJP;				// préavis debut EJP (min, 30 max)
	char* PTEC;				// Période tarifaire en cours : HPJB, HCJB, HPJW, HCJW, HPJR, HCJR
	char* DEMAIN;				// Couleur du lendemain (BLEU,BLAN,ROUG)
	int IINST;				// intensité instantanée(A)
	int ADPS;				// Avertissement dépassement de puissance souscrite (A)
	int IMAX;				// intensité maxi appelée (A)
	char HHPHC;				// Horaire heure pleine heure creuse
	int PAPP;				// Puissance apparente (VA) (seulement sur compteurs evolution)

	//Pour les compteurs triphasés / trames longues
	int IINST2;				// intensité instantanée(A) phase 2
	int IINST3;				// intensité instantanée(A) phase 3
	int IMAX2;				// intensité maxi appelée (A) phase 2
	int IMAX3;				// intensité maxi appelée (A) phase 3
	char* PPOT;				// Présence des potentiels codage hexa (voir doc)

	//Triphasé / trames courtes : ADIR1 ADIR2 ADIR3 ADCO IINST1 IINST2 IINST3
	// Lorsqu'il y a un dépassement, 20 trames courtes sont émises pour une trame longue
	int ADIR1;				// Avertissement dépassement d'intensité de réglage phase 1 (A)
	int ADIR2;				// Avertissement dépassement d'intensité de réglage phase 2 (A)
	int ADIR3;				// Avertissement dépassement d'intensité de réglage phase 3 (A)
	
private :
	byte state;				//Etat courant
	boolean debug;

	void handleGroup();

	
};
#endif
