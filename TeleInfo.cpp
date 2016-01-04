#include "TeleInfo.h"

char etiquette[9];
char data[13];
char checksum;
int charIdx;

//=================================================================================================================
// Constructeur
//=================================================================================================================
TeleInfo::TeleInfo(byte rxPin){
	sssBegin(1200,rxPin);

	debug = false;

	state = STATE_IDLE;

	ADCO = new char[13];
	OPTARIF = new char[5];
	PTEC = new char[5];
	DEMAIN = new char[5];
	PPOT = new char[3];


	resetData();
}

/**
 * Réinitialise les données
 */
void TeleInfo::resetData(){
	ADCO[0] = '\0';		// adresse compteur
	OPTARIF[0] = '\0';	// option tarifaire
	ISOUSC = -1;		// intensité souscrite (A)
	BASE = -1;		// compteur option Base (Wh)
	HCHC = -1;		// compteur HC  heure pleine (Wh)
	HCHP = -1;		// compteur HC  heure creuse (Wh)
	EJPHN = -1;		// compteur EJP heures normales (Wh)
	EJPHPM = -1;		// compteur EJP heures de pointe (Wh)
	BBRHCJB = -1;		// compteur TEMPO Heures Creuses Bleu (Wh)
	BBRHPJB = -1;		// compteur TEMPO Heures Pleines Bleu (Wh)
	BBRHCJW = -1;		// compteur TEMPO Heures Creuses Blanc (Wh)
	BBRHPJW = -1;		// compteur TEMPO Heures Pleines Blanc (Wh)
	BBRHCJR = -1;		// compteur TEMPO Heures Creuses Rouge (Wh)
	BBRHPJR = -1;		// compteur TEMPO Heures Pleines Rouge (Wh)
	PEJP = -1;		// préavis debut EJP (min, 30 max)
	PTEC[0] = '\0';		// Période tarifaire en cours : HPJB, HCJB, HPJW, HCJW, HPJR, HCJR
	DEMAIN[0] = '\0';	// Couleur du lendemain (BLEU,BLAN,ROUG)
	IINST = -1;		// intensité instantanée(A)
	ADPS = -1;		// Avertissement dépassement de puissance souscrite (A)
	IMAX = -1;		// intensité maxi appelée (A)
	HHPHC = -1;		// Horaire heure pleine heure creuse			
	PAPP = -1;		// Puissance apparente
	//Triphasé
	IINST2 = -1;		// intensité instantanée(A) phase 2
	IINST3 = -1;		// intensité instantanée(A) phase 3
	IMAX2 = -1;		// intensité maxi appelée (A) phase 2
	IMAX3 = -1;		// intensité maxi appelée (A) phase 3
	PPOT[0] = '\0';		// Présence des potentiels codage hexa (voir doc)
	ADIR1 = -1;		// Avertissement dépassement d'intensité de réglage phase 1 (A)
	ADIR2 = -1;		// Avertissement dépassement d'intensité de réglage phase 2 (A)
	ADIR3 = -1;		// Avertissement dépassement d'intensité de réglage phase 3 (A)

	state = STATE_IDLE;
}

//=================================================================================================================
// Capture des trames de Teleinfo
//=================================================================================================================
boolean TeleInfo::readTeleInfo(){
	char charIn=0;

/*
	if(sssOverflow()){
		//SoftwareSerial overflow. On réinitialise au préalable
		resetData();
	}
*/
	
	// tant que des octets sont disponibles en lecture : on lit les caractères
	// Il est possible que la lecture ait commencé dans un appel précédent de la methode
	while (sssAvailable()) {
		charIn = sssRead() & 0x7F;
		if(state == STATE_IDLE && charIn != START_FRAME){
			//La frame n'est pas commencée
			continue;
		}

		switch(charIn){
			case START_FRAME:
				if(state != STATE_IDLE && state != STATE_FRAME_AVAILABLE){ Serial.print("WARNING: START_FRAME en etat "); Serial.println(state); }
				//On initialise les variables pour la Frame
				resetData();
				state = STATE_FRAME_STARTED;
				break;
			case START_GROUP:
				etiquette[0] = '\0';
				data[0] = '\0';
				checksum = 0;
				charIdx = 0;
				state = STATE_READ_ETIQUETTE;
				break;
			case CHAR_SEPARATEUR:
				switch(state){
					case STATE_READ_ETIQUETTE:
						state = STATE_READ_DATA;
						charIdx = 0;
						checksum += charIn;	//Le separateur fait partie du checksum
						break;
					case STATE_READ_DATA:
						state = STATE_READ_CHECKSUM;
						charIdx = 0;	
						break;
					default:
						Serial.print(F("WARNING: separateur en etat ")); Serial.println(state);
						break;
				}
				break;
			case END_GROUP:
				state = STATE_GROUP_END;
				handleGroup();
				Serial.print(F("Received group: ")); Serial.print(etiquette); Serial.print(":"); Serial.println(data);
				break;
			case END_FRAME:
				state = STATE_FRAME_AVAILABLE;
				//la trame est prete à être utilisée
				if(debug) Serial.println(F("\nFRAME END RECEIVED"));
				break;
			case END_OF_TEXT:
				Serial.println(F("FRAME INTERRUPTED"));
				state = STATE_IDLE;
				break;
			default:
				//C'est un autre caratere (etiquette, donne ou checksum)
				switch(state){
					case STATE_READ_ETIQUETTE:
						if(charIdx>=8){
							Serial.print(F("WARNING: etiquette overflow: ")); Serial.println(etiquette);
							continue;
						}
						etiquette[charIdx++] = charIn;
						etiquette[charIdx] = '\0';
						checksum += charIn;
						break;
					case STATE_READ_DATA:
						if(charIdx>=12){
							Serial.print(F("WARNING: data overflow: ")); Serial.println(etiquette);
							continue;
						}
						data[charIdx++] = charIn;
						data[charIdx] = '\0';
						checksum += charIn;
						break;
					case STATE_READ_CHECKSUM:
						//on vérifie le checksum
						checksum = (checksum & 0x03F) +0x20;	//On ne conserve que les 6 bits de poids faible dans le checksum
						if(checksum != charIn){
							Serial.print(F("Checksum ERROR ")); Serial.print(charIn); Serial.print("!="); Serial.println(checksum);
							state = STATE_ERROR;
						}
						break;
					default:
						//On ignore les caracteres recus dans les autres etats (mais ce n'est pas normal)
						break;
				}
				break;
		}
	}
	
	//En retour, on indique si une trame est prete
	return isFrameAvailable();
}

/*
 * Indique si une trame est prête à être utilisée
 */
boolean TeleInfo::isFrameAvailable(){
	return state == STATE_FRAME_AVAILABLE;
}

//=================================================================================================================
// Frame parsing
//=================================================================================================================
void TeleInfo::handleGroup(){
	if(strcmp(etiquette,"ADCO")    == 0) strcpy(ADCO,data);		// adresse compteur
	if(strcmp(etiquette,"OPTARIF") == 0) strcpy(OPTARIF,data);	// option tarifaire
	if(strcmp(etiquette,"ISOUSC")  == 0) ISOUSC  = atol(data);	// intensité souscrite (A)
	if(strcmp(etiquette,"BASE")    == 0) BASE    = atol(data);	// compteur option Base (Wh)
	if(strcmp(etiquette,"HCHC")    == 0) HCHC    = atol(data);	// compteur HC  heure pleine (Wh)
	if(strcmp(etiquette,"HCHP")    == 0) HCHP    = atol(data);	// compteur HC  heure creuse (Wh)
	if(strcmp(etiquette,"EJPHN")   == 0) EJPHN   = atol(data);	// compteur EJP heures normales (Wh)
	if(strcmp(etiquette,"EJPHPM")  == 0) EJPHPM  = atol(data);	// compteur EJP heures de pointe (Wh)
	if(strcmp(etiquette,"BBRHCJB") == 0) BBRHCJB = atol(data);	// compteur TEMPO Heures Creuses Bleu (Wh)
	if(strcmp(etiquette,"BBRHPJB") == 0) BBRHPJB = atol(data);	// compteur TEMPO Heures Pleines Bleu (Wh)
	if(strcmp(etiquette,"BBRHCJW") == 0) BBRHCJW = atol(data);	// compteur TEMPO Heures Creuses Blanc (Wh)
	if(strcmp(etiquette,"BBRHPJW") == 0) BBRHPJW = atol(data);	// compteur TEMPO Heures Pleines Blanc (Wh)
	if(strcmp(etiquette,"BBRHCJR") == 0) BBRHCJR = atol(data);	// compteur TEMPO Heures Creuses Rouge (Wh)
	if(strcmp(etiquette,"BBRHPJR") == 0) BBRHPJR = atol(data);	// compteur TEMPO Heures Pleines Rouge (Wh)
	if(strcmp(etiquette,"PEJP")    == 0) PEJP    = atol(data);	// préavis debut EJP (min, 30 max)
	if(strcmp(etiquette,"PTEC")    == 0) strcpy(PTEC,data);		// Période tarifaire en cours : HPJB, HCJB, HPJW, HCJW, HPJR, HCJR
	if(strcmp(etiquette,"DEMAIN")  == 0) strcpy(DEMAIN,data);	// Couleur du lendemain (BLEU,BLAN,ROUG)
	if(strcmp(etiquette,"IINST")   == 0) IINST   = atol(data);	// intensité instantanée(A)
	if(strcmp(etiquette,"ADPS")    == 0) ADPS    = atol(data);	// Avertissement dépassement de puissance souscrite (A)
	if(strcmp(etiquette,"IMAX")    == 0) IMAX    = atol(data);	// intensité maxi appelée (A)
	if(strcmp(etiquette,"HHPHC")   == 0) HHPHC   = data[0];		// Horaire heure pleine heure creuse		
	if(strcmp(etiquette,"PAPP")    == 0) PAPP    = atol(data);	// Puissance apparente
	/*
	IINST2 = -1;		// intensité instantanée(A) phase 2
	IINST3 = -1;		// intensité instantanée(A) phase 3
	IMAX2 = -1;		// intensité maxi appelée (A) phase 2
	IMAX3 = -1;		// intensité maxi appelée (A) phase 3
	PPOT = -1;		// Présence des potentiels codage hexa (voir doc)
	ADIR1 = -1;		// Avertissement dépassement d'intensité de réglage phase 1 (A)
	ADIR2 = -1;		// Avertissement dépassement d'intensité de réglage phase 2 (A)
	ADIR3 = -1;		// Avertissement dépassement d'intensité de réglage phase 3 (A)
	*/
}

void TeleInfo::setDebug(boolean d){
	debug = d;
}
