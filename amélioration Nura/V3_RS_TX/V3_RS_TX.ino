/* =====================================================================
   SAE32 - Vidéo 3 - ÉMETTEUR Reed-Solomon simplifié RS(24,20) - PROPOSÉ
   [V3-10] Base : Vidéo 3/Autre (Simon)/video-3-Test-Prog-2-A.ino.
   Cette version en reprend le format (poids fort d'abord).

   Boutons à MAINTENIR :
     A = 1 octet de donnée abîmé        -> R corrige
     B = champ S abîmé                  -> R laisse les données intactes
     C = 2 octets abîmés                -> limite : R "corrige" le mauvais octet
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
#define NB_DONNEES 20
#define TAILLE_TRAME 24
#define PERIODE_MS 2000

#define EMISSION 0               // [V3-11] Même recette switch/état que les autres vidéos
#define DELAI    1

uint8_t txbuf[TAILLE_TRAME];
uint8_t state = EMISSION;
uint32_t debutDelai = 0;

// [V3-12] Données différentes selon le rang. Le récepteur connaît ce motif
// pour vérifier sa correction pendant les essais.
uint8_t motif(uint8_t i) {
  return (uint8_t)(i * 13 + 7);
}

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
  M5.Lcd.println("V3 - TX RS(24,20)");
  M5.Lcd.setTextSize(1);
  M5.Lcd.println("A: 1 erreur  B: S faux  C: 2 erreurs\n");

  if (!rf95.init()) {
    M5.Lcd.println("Init radio ECHEC");
    while (true) delay(1000);
  }
  rf95.setModemConfig(RH_RF95::Bw125Cr45Sf128);
  rf95.setFrequency(FREQ_NORMALE);
  rf95.setTxPower(PUISSANCE_DBM);
}

void loop() {
  M5.update();

  switch (state) {
    case EMISSION: {
      preparerEcran();

      uint16_t S = 0, SP = 0;    // Au plus 5100 et 53550 : 16 bits non signés suffisent
      for (uint8_t i = 0; i < NB_DONNEES; i++) {
        txbuf[i] = motif(i);
        S  += txbuf[i];
        SP += txbuf[i] * (i + 1);         // i part de 0, le rang part de 1
      }
      txbuf[20] = S >> 8;                 // Poids fort d'abord : choix de l'équipe
      txbuf[21] = S & 0xFF;               // (la capsule envoie le poids faible d'abord)
      txbuf[22] = SP >> 8;
      txbuf[23] = SP & 0xFF;

      M5.Lcd.printf("S=%u SP=%u ", S, SP);

      // Erreurs injectées après le calcul, comme un parasite pendant le trajet.
      if (M5.BtnA.isPressed()) {
        txbuf[4] ^= 0x37;                 // Rang 5
        M5.Lcd.print("[1 ERR]");
      }
      if (M5.BtnB.isPressed()) {
        txbuf[21] ^= 0x0F;                // Champ S
        M5.Lcd.print("[ERR S]");
      }
      if (M5.BtnC.isPressed()) {
        // [V3-13] +1 aux rangs 1 et 3 : E = 2 et SP change de 1 + 3 = 4.
        // Le calcul donne R = 4 / 2 = 2 : il accuse un octet qui était juste.
        txbuf[0]++;
        txbuf[2]++;
        M5.Lcd.print("[2 ERR]");
      }
      M5.Lcd.println();

      rf95.send(txbuf, TAILLE_TRAME);
      rf95.waitPacketSent();
      debutDelai = millis();
      state = DELAI;
      break;
    }

    case DELAI:
      if (millis() - debutDelai >= PERIODE_MS) {
        state = EMISSION;
      }
      break;

    default:
      state = EMISSION;
      break;
  }
}
