/* =====================================================================
   SAE32 - Vidéo 1 - ÉMETTEUR LLC1 - version améliorée PROPOSÉE
   Base : Vidéo 1/video_1_Prog_Test_1_B.ino (original conservé)
   Les repères [V1-x] renvoient à SAE32-Ameliorations-Code.md

   Boutons (tests en direct, sans re-téléverser) :
     A = trame courte (3 octets) <-> trame longue (32 octets)  -> test 4
     B = canal normal <-> autre canal                          -> test 2
     C = EIT 1 s -> 5 s -> 0 s -> 1 s                          -> test 3
   ===================================================================== */
#include <SPI.h>
#include <M5Stack.h>
#include <RH_RF95.h>

#define RFM95_CS   5
#define RFM95_DIO0 36            // [V1-1] DIO0 (lettre O) : l'original écrit DI00 (zéro)
RH_RF95 rf95(RFM95_CS, RFM95_DIO0);

// [V1-2] Tous les réglages sont au même endroit : un seul endroit à changer.
#define FREQ_NORMALE   867.7     // Fréquence de la diapo 5
#define FREQ_TEST      867.9     // Autre canal : R ne doit plus rien recevoir
#define PUISSANCE_DBM  8
#define TAILLE_COURTE  3
#define TAILLE_LONGUE  32
#define LIGNE_MAX_ECRAN 220      // Au-delà, le texte sort de l'écran (240 px)

// Automate : le même que dans le cours
#define EMISSION 0
#define DELAI    1
uint8_t state;

// [V1-3] Tampon dimensionné pour le test 4 : l'original (txbuf[2]) ne peut pas
// envoyer 32 octets.
uint8_t txbuf[TAILLE_LONGUE];
uint8_t tailleTrame = TAILLE_COURTE;

// [V1-4] Compteur dans la trame : R peut enfin compter les trames perdues.
uint8_t compteur = 0;

uint32_t eit = 1000;             // Espace inter-trame en ms
uint32_t debutDelai = 0;         // Instant d'entrée dans l'état DELAI
uint32_t dureeAir = 0;           // Temps d'émission mesuré d'une trame
bool     canalNormal = true;

// [V1-5] Évite le texte invisible : on efface avant de sortir de l'écran.
void preparerEcran() {
  if (M5.Lcd.getCursorY() > LIGNE_MAX_ECRAN) {
    M5.Lcd.fillScreen(BLACK);
    M5.Lcd.setCursor(0, 0);
  }
}

// [V1-6] Si la radio ne répond pas, on s'arrête avec un message clair.
// Sinon waitPacketSent() attendrait sans fin et l'écran semblerait figé.
void initialiserRadio() {
  if (!rf95.init()) {
    M5.Lcd.println("Init radio ECHEC");
    while (true) delay(1000);
  }
  M5.Lcd.println("Init radio OK");
  rf95.setModemConfig(RH_RF95::Bw125Cr45Sf128);   // SF7, 125 kHz
  rf95.setFrequency(FREQ_NORMALE);
  rf95.setTxPower(PUISSANCE_DBM);
}

void gererBoutons() {
  if (M5.BtnA.wasPressed()) {
    tailleTrame = (tailleTrame == TAILLE_COURTE) ? TAILLE_LONGUE : TAILLE_COURTE;
    M5.Lcd.printf("-> Taille %u octets\n", tailleTrame);
  }
  if (M5.BtnB.wasPressed()) {
    canalNormal = !canalNormal;
    rf95.setFrequency(canalNormal ? FREQ_NORMALE : FREQ_TEST);
    M5.Lcd.printf("-> Canal %.1f MHz\n", canalNormal ? FREQ_NORMALE : FREQ_TEST);
  }
  if (M5.BtnC.wasPressed()) {
    if (eit == 1000)      eit = 5000;
    else if (eit == 5000) eit = 0;
    else                  eit = 1000;
    M5.Lcd.printf("-> EIT %lu ms\n", (unsigned long)eit);
  }
}

void setup() {
  M5.begin();
  M5.Power.begin();
  M5.Lcd.clear(BLACK);
  M5.Lcd.setTextColor(WHITE);
  M5.Lcd.setTextSize(2);
  M5.Lcd.println("V1 - TX LLC1");

  delay(1000);
  initialiserRadio();

  state = EMISSION;
  delay(3000);                   // Laisse le temps d'allumer R (piège : E avant R)
}

void loop() {
  M5.update();                   // Lecture des boutons à chaque tour
  gererBoutons();

  switch (state) {
    case EMISSION: {
      preparerEcran();

      // Format personnel : [compteur][0xAA][0x55], complété jusqu'à 32 octets
      // pour le test de la trame longue.
      txbuf[0] = compteur;
      txbuf[1] = 0xAA;
      txbuf[2] = 0x55;
      for (uint8_t i = 3; i < tailleTrame; i++) txbuf[i] = i;

      // [V1-7] On mesure le temps d'antenne réel, au lieu de le supposer.
      uint32_t debut = millis();
      rf95.send(txbuf, tailleTrame);
      rf95.waitPacketSent();
      dureeAir = millis() - debut;

      // [V1-8] Rapport cyclique de la diapo 5 : temps d'émission / temps total.
      float rapport = 100.0 * dureeAir / (dureeAir + eit);
      M5.Lcd.printf("T%u %lums DC=%.1f%%\n", compteur,
                    (unsigned long)dureeAir, rapport);

      compteur++;                // Sur 8 bits : 255 + 1 revient à 0
      debutDelai = millis();
      state = DELAI;
      break;
    }

    case DELAI:
      // [V1-9] Attente NON bloquante : les boutons restent actifs.
      // Un delay(1000) rendrait les boutons presque inutilisables.
      if (millis() - debutDelai >= eit) {
        state = EMISSION;
      }
      break;

    default:
      state = EMISSION;
      break;
  }
}
