/* =====================================================================
   SAE32 - Vidéo 2 - RÉCEPTEUR DATA + ACK (LLC3) - version PROPOSÉE
   [V2-11] Base : Vidéo 2/Autre (Simon)/video-2-Prog-Test-1-B.ino.
   Attention : vid-2-test-1B-mailis.ino est le récepteur de la vidéo 1. Il
   n'envoie aucun ACK, donc l'émetteur V2 ne peut pas réussir un échange avec lui.

   Bouton A à MAINTENIR = simule la perte de l'ACK.
   R reçoit la DATA mais ne répond pas. E réémet et R affiche "DOUBLON".
   ===================================================================== */
#include <SPI.h>
#include <M5Stack.h>
#include <RH_RF95.h>

#define RFM95_CS   5
#define RFM95_DIO0 36
RH_RF95 rf95(RFM95_CS, RFM95_DIO0);

#define FREQ_NORMALE 867.7
#define PUISSANCE_DBM 8
#define LIGNE_MAX_ECRAN 220

#define TYPE_DATA 1
#define TYPE_ACK  2
#define TAILLE_DATA 4
#define TAILLE_ACK  2

#define E0 0                     // Se mettre en réception
#define E1 1                     // Attendre une trame
#define E2 2                     // Nouvelle donnée ou doublon ?
#define E3 3                     // Envoyer l'ACK

uint8_t txbuf[RH_RF95_MAX_MESSAGE_LEN];
uint8_t rxbuf[RH_RF95_MAX_MESSAGE_LEN];
uint8_t rxlen;
uint8_t state, RxSeq;

// [V2-12] Le cours initialise RxSeq à 255 pour que la trame 0 soit nouvelle.
// Mais une première trame numérotée 255 serait alors prise pour un doublon.
// Ce booléen règle tous les cas, y compris un test qui démarre à 255.
bool premiereTrame = true;

uint32_t nbDonnees = 0, nbDoublons = 0;

void preparerEcran() {
  if (M5.Lcd.getCursorY() > LIGNE_MAX_ECRAN) {
    M5.Lcd.fillScreen(BLACK);
    M5.Lcd.setCursor(0, 0);
  }
}

void setup() {
  M5.begin();
  M5.Power.begin();
  M5.Lcd.setTextSize(2);
  M5.Lcd.println("V2 - RX DATA+ACK");

  if (!rf95.init()) {
    M5.Lcd.println("Init radio ECHEC");
    while (true) delay(1000);
  }
  rf95.setModemConfig(RH_RF95::Bw125Cr45Sf128);
  rf95.setFrequency(FREQ_NORMALE);
  rf95.setTxPower(PUISSANCE_DBM);

  state = E0;
}

void loop() {
  M5.update();

  switch (state) {
    case E0:
      rf95.setModeRx();
      state = E1;
      break;

    case E1:
      rxlen = RH_RF95_MAX_MESSAGE_LEN;
      if (rf95.recv(rxbuf, &rxlen)) {
        // On n'acquitte qu'une DATA de la bonne longueur.
        if ((rxlen == TAILLE_DATA) && (rxbuf[0] == TYPE_DATA)) {
          state = E2;
        } else {
          state = E0;
        }
      }
      break;

    case E2:
      preparerEcran();
      if (premiereTrame || (RxSeq != rxbuf[1])) {
        premiereTrame = false;
        RxSeq = rxbuf[1];
        nbDonnees++;
        // [V2-13] Affichage court : il retarde l'ACK (piège 5 de la fiche V2).
        M5.Lcd.printf("DATA %u R%d\n", RxSeq, rf95.lastRssi());
      } else {
        nbDoublons++;
        M5.Lcd.printf("DOUBLON %u\n", RxSeq);   // Donnée absorbée
      }
      state = E3;                // Nouvelle ou doublon : on acquitte dans les deux cas
      break;

    case E3:
      if (M5.BtnA.isPressed()) {
        M5.Lcd.println("[ACK perdu]");          // [V2-14] Perte simulée
      } else {
        txbuf[0] = TYPE_ACK;
        txbuf[1] = rxbuf[1];     // Même numéro que la DATA reçue
        rf95.send(txbuf, TAILLE_ACK);
        rf95.waitPacketSent();
      }
      state = E0;                // Pas de chien de garde : on n'acquitte pas un ACK
      break;

    default:
      state = E0;
      break;
  }
}
