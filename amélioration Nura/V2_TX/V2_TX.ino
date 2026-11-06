/* =====================================================================
   SAE32 - Vidéo 2 - ÉMETTEUR DATA + ACK (LLC3) - version améliorée PROPOSÉE
   Base : Vidéo 2/vid-2-test-1A-mailis.ino (original conservé)
   Les repères [V2-x] renvoient à SAE32-Ameliorations-Code.md

   Boutons à MAINTENIR pendant l'échange :
     A = simule la perte de la DATA (rien n'est émis)
     B = simule un ACK avec un mauvais numéro
     C = simule un ACK avec un mauvais type
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

// [V2-1] Valeur de départ : à remplacer par "RTT max mesuré + marge".
// Le RTT est affiché à l'écran pendant les essais (voir [V2-2]).
#define TIMEOUT_ACK 500
#define CREDIT_MAX  5
#define PAUSE_MS    1000         // Temps entre deux trames (rapport cyclique)

// Mettre 253 pour le test du passage de 255 à 0, puis 0 pour le reste.
#define SEQ_DEPART  253

#define E0 0                     // Émettre la DATA
#define E1 1                     // Armer le chien de garde
#define E2 2                     // Se mettre en réception
#define E3 3                     // Surveiller l'ACK et le chien de garde
#define E4 4                     // Succès
#define E5 5                     // Échec de cette tentative

uint8_t txbuf[RH_RF95_MAX_MESSAGE_LEN];
uint8_t rxbuf[RH_RF95_MAX_MESSAGE_LEN];
uint8_t rxlen;
uint8_t state, TxSeq, credit;

uint32_t debutAttente;           // [V2-3] L'instant de départ, pas l'heure limite

// [V2-2] Mesures affichées à l'écran : elles servent de jeu d'essai.
uint32_t rttMin = 0xFFFFFFFF, rttMax = 0;
uint32_t nbSucces = 0, nbReemissions = 0, nbEchecs = 0;

void preparerEcran() {           // [V2-4] Le texte reste visible pendant la démo
  if (M5.Lcd.getCursorY() > LIGNE_MAX_ECRAN) {
    M5.Lcd.fillScreen(BLACK);
    M5.Lcd.setCursor(0, 0);
  }
}

void setup() {
  M5.begin();
  M5.Power.begin();
  M5.Lcd.setTextSize(2);
  M5.Lcd.println("V2 - TX DATA+ACK");

  if (!rf95.init()) {            // [V2-5] Arrêt clair si la radio ne répond pas
    M5.Lcd.println("Init radio ECHEC");
    while (true) delay(1000);
  }
  rf95.setModemConfig(RH_RF95::Bw125Cr45Sf128);
  rf95.setFrequency(FREQ_NORMALE);
  rf95.setTxPower(PUISSANCE_DBM);

  TxSeq = SEQ_DEPART;
  credit = CREDIT_MAX;
  state = E0;

  // [V2-6] Le numéro va de 0 à 255 : il y a 256 valeurs, donc c'est un MODULO 256.
  // L'original affiche "MODULO 255", ce qui est faux.
  M5.Lcd.println("Sequence modulo 256");
  delay(2000);
}

void loop() {
  M5.update();

  switch (state) {
    case E0:
      preparerEcran();
      M5.Lcd.printf("DATA %u c%u ", TxSeq, credit);

      txbuf[0] = TYPE_DATA;
      txbuf[1] = TxSeq;
      txbuf[2] = 0xAA;
      txbuf[3] = 0x55;

      // [V2-7] Perte de DATA simulée, pour une démo fiable sans éloigner les nœuds.
      if (M5.BtnA.isPressed()) {
        M5.Lcd.print("[PERDUE] ");
      } else {
        rf95.send(txbuf, TAILLE_DATA);
        rf95.waitPacketSent();
      }

      if (credit < CREDIT_MAX) nbReemissions++;
      credit--;
      state = E1;
      break;

    case E1:
      debutAttente = millis();   // [V2-3] Point de départ du chien de garde et du RTT
      state = E2;
      break;

    case E2:
      rf95.setModeRx();
      state = E3;
      break;

    case E3:
      // [V2-3] Soustraction non signée : reste juste quand millis() repasse par 0.
      if (millis() - debutAttente > TIMEOUT_ACK) {
        M5.Lcd.println("CdG expire");
        state = E5;
      } else {
        rxlen = RH_RF95_MAX_MESSAGE_LEN;
        if (rf95.recv(rxbuf, &rxlen)) {
          if (M5.BtnB.isPressed()) rxbuf[1]++;       // [V2-8] Mauvais numéro simulé
          if (M5.BtnC.isPressed()) rxbuf[0] = 0x7F;  // [V2-8] Mauvais type simulé

          // [V2-9] On vérifie aussi la longueur : trame de 2 octets attendue.
          if ((rxlen == TAILLE_ACK) && (rxbuf[0] == TYPE_ACK) && (rxbuf[1] == TxSeq)) {
            state = E4;
          } else {
            M5.Lcd.print("[ACK refuse] ");
            state = E2;          // Le chien de garde continue : il n'est pas réarmé
          }
        }
      }
      break;

    case E4: {
      uint32_t rtt = millis() - debutAttente;
      if (rtt < rttMin) rttMin = rtt;
      if (rtt > rttMax) rttMax = rtt;
      nbSucces++;

      M5.Lcd.printf("ACK %lums\n", (unsigned long)rtt);
      preparerEcran();
      M5.Lcd.printf("RTT %lu-%lu OK%lu R%lu X%lu\n",
                    (unsigned long)rttMin, (unsigned long)rttMax,
                    (unsigned long)nbSucces, (unsigned long)nbReemissions,
                    (unsigned long)nbEchecs);

      TxSeq++;                   // Sur 8 bits : 255 + 1 revient à 0
      credit = CREDIT_MAX;
      delay(PAUSE_MS);
      state = E0;
      break;
    }

    case E5:
      if (credit == 0) {
        M5.Lcd.println("ECHEC: abandon");
        nbEchecs++;
        TxSeq++;                 // On abandonne cette trame
        credit = CREDIT_MAX;
      } else {
        M5.Lcd.printf("Reessai %u\n", CREDIT_MAX - credit);  // [V2-10] "n°" illisible sur LCD
      }
      delay(PAUSE_MS);
      state = E0;
      break;

    default:
      state = E0;
      break;
  }
}
