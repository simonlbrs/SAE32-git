/* =====================================================================
   SAE32 - Vidéo 4 - PUITS (récepteur) multi-nœuds - portage RH_RF95
   Base : Vidéo 4/Test 1 a 3/Recep Wino.ino (Gabin). Sert à tous les tests.

   Au démarrage (3 s) : maintenir B pour l'adresse 10, C pour l'adresse 11.
   Sans bouton : adresse 0, le puits normal. Test 7 : un nœud par adresse.
   En cours d'essai, bouton C = remet les statistiques à zéro.
   ===================================================================== */
#include <SPI.h>
#include <M5Stack.h>
#include <RH_RF95.h>

#define RFM95_CS   5
#define RFM95_DIO0 36
RH_RF95 rf95(RFM95_CS, RFM95_DIO0);

#define FREQ_NORMALE 867.7
#define PUISSANCE_DBM 8

uint8_t MyAdr = 0;               // Le puits a l'adresse 0
#define NB_ADRESSES 16           // Adresses 0 à 15, comme RxSeq[16] de Gabin
#define TYPE_DATA  1
#define TYPE_ACK   2
#define TAILLE_DATA 6
#define TAILLE_ACK  4
#define DELAI_RETOURNEMENT 5     // ms, à ajuster sur le matériel

#define E0 0                     // Écouter
#define E1 1                     // Trier : DATA pour moi ?
#define E2 2                     // Nouvelle donnée ou doublon ?
#define E3 3                     // Envoyer l'ACK
#define E4 4                     // [V4-12] Afficher, après l'ACK

uint8_t txbuf[RH_RF95_MAX_MESSAGE_LEN];
uint8_t rxbuf[RH_RF95_MAX_MESSAGE_LEN];
uint8_t rxlen;
uint8_t state = E0;

// [V4-10] Une mémoire PAR émetteur. Avec un seul RxSeq comme sur la diapo, la
// trame 7 de E2 arrivant après la trame 7 de E1 serait prise pour un doublon :
// elle serait jetée ET acquittée, donc perdue sans que personne le sache.
uint8_t  RxSeq[NB_ADRESSES];
bool     dejaRecu[NB_ADRESSES];  // [V2-12] Remplace la valeur spéciale 255
uint32_t nbDonnees[NB_ADRESSES];
uint32_t nbDoublons[NB_ADRESSES];

void remettreAZero() {
  for (uint8_t i = 0; i < NB_ADRESSES; i++) {
    dejaRecu[i] = false;
    nbDonnees[i] = 0;
    nbDoublons[i] = 0;
  }
}

void afficherTableau() {
  M5.Lcd.setCursor(0, 24);
  for (uint8_t i = 1; i <= 4; i++) {           // Assez pour la démo à 3 ou 4 nœuds
    M5.Lcd.printf("E%u: %lu  dbl %lu   \n", i,
                  (unsigned long)nbDonnees[i], (unsigned long)nbDoublons[i]);
  }
  M5.Lcd.printf("RSSI %d SNR %d   \n", rf95.lastRssi(), rf95.lastSNR());
}

void choisirAdresse() {
  M5.Lcd.println("B=10 C=11 sinon 0");
  uint32_t debut = millis();
  while (millis() - debut < 3000) {
    M5.update();
    if (M5.BtnB.isPressed()) MyAdr = 10;
    if (M5.BtnC.isPressed()) MyAdr = 11;
    delay(10);
  }
}

void setup() {
  M5.begin();
  M5.Power.begin();
  M5.Lcd.setTextSize(2);
  M5.Lcd.setTextColor(WHITE, BLACK);           // Le fond efface l'ancien texte
  choisirAdresse();
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setCursor(0, 0);
  M5.Lcd.printf("V4 - PUITS adr %u\n", MyAdr);

  if (!rf95.init()) {
    M5.Lcd.println("Init radio ECHEC");
    while (true) delay(1000);
  }
  rf95.setModemConfig(RH_RF95::Bw125Cr45Sf128);
  rf95.setFrequency(FREQ_NORMALE);
  rf95.setTxPower(PUISSANCE_DBM);

  remettreAZero();
  afficherTableau();
}

void loop() {
  M5.update();
  if (M5.BtnC.wasPressed()) {
    remettreAZero();
    afficherTableau();
  }

  switch (state) {
    case E0:
      rf95.setModeRx();
      state = E1;
      break;

    case E1:
      rxlen = RH_RF95_MAX_MESSAGE_LEN;
      if (rf95.recv(rxbuf, &rxlen)) {
        // [V4-11] rxbuf[0] sert d'indice de tableau : on vérifie qu'il est valide,
        // sinon une trame anormale écrirait en dehors des tableaux.
        if ((rxlen == TAILLE_DATA) && (rxbuf[2] == TYPE_DATA) &&
            (rxbuf[1] == MyAdr) && (rxbuf[0] > 0) && (rxbuf[0] < NB_ADRESSES)) {
          state = E2;
        } else {
          state = E0;            // Pas une DATA pour moi : on l'ignore
        }
      }
      break;

    case E2: {
      uint8_t src = rxbuf[0];
      if (!dejaRecu[src] || (RxSeq[src] != rxbuf[3])) {
        dejaRecu[src] = true;
        RxSeq[src] = rxbuf[3];
        nbDonnees[src]++;
      } else {
        nbDoublons[src]++;
      }
      state = E3;
      break;
    }

    case E3:
      delay(DELAI_RETOURNEMENT); // [P-21] L'émetteur doit repasser en écoute
      txbuf[0] = MyAdr;          // @S : le puits
      txbuf[1] = rxbuf[0];       // @D : la source de la DATA
      txbuf[2] = TYPE_ACK;
      txbuf[3] = rxbuf[3];       // Même numéro que la DATA
      rf95.send(txbuf, TAILLE_ACK);
      rf95.waitPacketSent();
      state = E4;
      break;

    case E4:
      // [V4-12] L'écran passe après l'ACK : l'émetteur n'attend pas l'affichage.
      afficherTableau();
      state = E0;
      break;

    default:
      state = E0;
      break;
  }
}
