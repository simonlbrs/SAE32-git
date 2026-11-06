/* =====================================================================
   SAE32 - Vidéo 3 - RÉCEPTEUR Reed-Solomon simplifié RS(24,20) - PROPOSÉ
   [V3-10] Base : Vidéo 3/Autre (Simon)/video-3-Test-Prog-2-B.ino.
   La logique est reprise, avec des contrôles en plus.
   Pas d'ACK : R corrige seul à la volée, comme dans la capsule.
   ===================================================================== */
#include <SPI.h>
#include <M5Stack.h>
#include <RH_RF95.h>

#define RFM95_CS   5
#define RFM95_DIO0 36
RH_RF95 rf95(RFM95_CS, RFM95_DIO0);

#define FREQ_NORMALE 867.7
#define LIGNE_MAX_ECRAN 200
#define NB_DONNEES 20
#define TAILLE_TRAME 24

#define ECOUTE  0
#define TEST_RX 1
#define DECODE  2

uint8_t rxbuf[RH_RF95_MAX_MESSAGE_LEN];
uint8_t rxlen;
uint8_t state = ECOUTE;

uint32_t nbIntactes = 0, nbCorrigees = 0, nbRefusees = 0, nbFaussesCorrections = 0;

uint8_t motif(uint8_t i) {       // Identique à l'émetteur, sert à vérifier les essais
  return (uint8_t)(i * 13 + 7);
}

bool donneesJustes() {
  for (uint8_t i = 0; i < NB_DONNEES; i++) {
    if (rxbuf[i] != motif(i)) return false;
  }
  return true;
}

void preparerEcran() {
  if (M5.Lcd.getCursorY() > LIGNE_MAX_ECRAN) {
    M5.Lcd.fillScreen(BLACK);
    M5.Lcd.setCursor(0, 0);
  }
}

// Renvoie true si le décodage a produit des données utilisables.
bool decoder() {
  uint16_t Sr  = ((uint16_t)rxbuf[20] << 8) | rxbuf[21];
  uint16_t SPr = ((uint16_t)rxbuf[22] << 8) | rxbuf[23];

  uint16_t Sc = 0, SPc = 0;
  for (uint8_t i = 0; i < NB_DONNEES; i++) {
    Sc  += rxbuf[i];
    SPc += rxbuf[i] * (i + 1);
  }

  int32_t E = (int32_t)Sc - (int32_t)Sr;       // Signé : l'erreur peut être négative
  int32_t D = (int32_t)SPc - (int32_t)SPr;

  if ((E == 0) && (D == 0)) {
    M5.Lcd.println("OK : sans erreur");
    return true;
  }

  // [V3-14] L'original annonce "Data intacte" alors que ce n'est PAS prouvé.
  // S juste + SP faux : SP abîmé OU deux erreurs qui se compensent dans S.
  if (E == 0) {
    M5.Lcd.println("REFUS : S juste, SP faux");
    return false;
  }

  // D == 0 : l'erreur porte sur S, ce qui donnerait un rang R = 0.
  if (D == 0) {
    M5.Lcd.println("Controle S abime");
    return true;
  }

  // [V3-15] Le rang doit être un entier entre 1 et 20. Sinon il y a plusieurs
  // erreurs, et on n'a pas le droit de "corriger".
  if ((D % E) != 0) {
    M5.Lcd.println("REFUS : rang non entier");
    return false;
  }
  int32_t R = D / E;
  if ((R < 1) || (R > NB_DONNEES)) {
    M5.Lcd.printf("REFUS : rang %ld impossible\n", (long)R);
    return false;
  }

  // [V3-16] La valeur corrigée doit tenir sur un octet. L'original force le
  // résultat avec (uint8_t), ce qui cache une correction impossible.
  int32_t corrige = (int32_t)rxbuf[R - 1] - E;
  if ((corrige < 0) || (corrige > 255)) {
    M5.Lcd.println("REFUS : valeur impossible");
    return false;
  }

  M5.Lcd.printf("CORRECTION rang %ld : %u -> %ld\n", (long)R, rxbuf[R - 1], (long)corrige);
  rxbuf[R - 1] = (uint8_t)corrige;             // Piège 2 : on écrit vraiment la correction
  nbCorrigees++;
  return true;
}

void setup() {
  M5.begin();
  M5.Power.begin();
  M5.Lcd.setTextSize(2);
  M5.Lcd.println("V3 - RX RS(24,20)");
  M5.Lcd.setTextSize(1);

  if (!rf95.init()) {
    M5.Lcd.println("Init radio ECHEC");
    while (true) delay(1000);
  }
  rf95.setModemConfig(RH_RF95::Bw125Cr45Sf128);
  rf95.setFrequency(FREQ_NORMALE);
}

void loop() {
  switch (state) {
    case ECOUTE:
      rf95.setModeRx();
      state = TEST_RX;
      break;

    case TEST_RX:
      if (rf95.available()) state = DECODE;
      break;

    case DECODE:
      rxlen = RH_RF95_MAX_MESSAGE_LEN;
      if (rf95.recv(rxbuf, &rxlen)) {
        preparerEcran();
        if (rxlen != TAILLE_TRAME) {
          nbRefusees++;
          M5.Lcd.printf("REFUS : %u octets\n", rxlen);
        } else {
          uint32_t corrigeesAvant = nbCorrigees;
          if (decoder()) {
            if (!donneesJustes()) {
              // [V3-13] Possible seulement parce qu'on connaît le motif de test.
              nbFaussesCorrections++;
              M5.Lcd.setTextColor(RED, BLACK);
              M5.Lcd.println("!! DONNEES FAUSSES ACCEPTEES");
              M5.Lcd.setTextColor(WHITE, BLACK);
            } else if (nbCorrigees == corrigeesAvant) {
              nbIntactes++;
            }
          } else {
            nbRefusees++;
          }
        }
        M5.Lcd.printf("Int%lu Cor%lu Ref%lu Faux%lu\n\n",
                      (unsigned long)nbIntactes, (unsigned long)nbCorrigees,
                      (unsigned long)nbRefusees, (unsigned long)nbFaussesCorrections);
      }
      state = ECOUTE;
      break;

    default:
      state = ECOUTE;
      break;
  }
}
