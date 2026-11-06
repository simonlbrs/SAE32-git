/* =====================================================================
   SAE32 - Vidéo 1 - RÉCEPTEUR LLC1 - version améliorée PROPOSÉE
   Base : Vidéo 1/vid_1_Prog_Test_1_A.ino (original conservé)
   Les repères [V1-x] renvoient à SAE32-Ameliorations-Code.md

   Bouton A = remet les statistiques à zéro.
   ===================================================================== */
#include <SPI.h>
#include <M5Stack.h>
#include <RH_RF95.h>

#define RFM95_CS   5
#define RFM95_DIO0 36            // [V1-1] DIO0, pas DI00
RH_RF95 rf95(RFM95_CS, RFM95_DIO0);

#define FREQ_NORMALE    867.7    // Doit être identique à celle de l'émetteur
#define LIGNE_MAX_ECRAN 220

#define ECOUTE    1
#define TEST_RX   2
#define AFFICHAGE 3
uint8_t state;

uint8_t rxbuf[RH_RF95_MAX_MESSAGE_LEN];
uint8_t rxlen;

uint32_t nbRecues = 0;
uint32_t nbPerdues = 0;          // [V1-4] Mesure possible grâce au compteur de trame
uint8_t  attendu = 0;            // Numéro de la prochaine trame attendue
bool     synchronise = false;    // Faux tant qu'aucune trame n'a été reçue

void preparerEcran() {           // [V1-5] Le texte reste visible
  if (M5.Lcd.getCursorY() > LIGNE_MAX_ECRAN) {
    M5.Lcd.fillScreen(BLACK);
    M5.Lcd.setCursor(0, 0);
  }
}

void setup() {
  M5.begin();
  M5.Power.begin();
  M5.Lcd.clear(BLACK);
  M5.Lcd.setTextColor(WHITE);
  M5.Lcd.setTextSize(2);
  M5.Lcd.println("V1 - RX LLC1");

  delay(1000);

  if (!rf95.init()) {            // [V1-6] Arrêt clair si la radio ne répond pas
    M5.Lcd.println("Init radio ECHEC");
    while (true) delay(1000);
  }
  M5.Lcd.println("Init radio OK");

  // [V1-10] On règle la radio D'ABORD, on écoute ENSUITE.
  // L'original appelle setModeRx() avant de changer la fréquence.
  rf95.setModemConfig(RH_RF95::Bw125Cr45Sf128);
  rf95.setFrequency(FREQ_NORMALE);

  state = ECOUTE;
}

void loop() {
  M5.update();
  if (M5.BtnA.wasPressed()) {
    nbRecues = 0;
    nbPerdues = 0;
    synchronise = false;
    M5.Lcd.println("-> Stats a zero");
  }

  switch (state) {
    case ECOUTE:
      // [V1-11] Plus d'affichage "ECOUTE" ici : il remplissait l'écran pour rien.
      rf95.setModeRx();
      state = TEST_RX;
      break;

    case TEST_RX:
      if (rf95.available()) {
        state = AFFICHAGE;
      }
      break;

    case AFFICHAGE:
      rxlen = sizeof(rxbuf);     // Piège 1 de la fiche V1 : à refaire avant CHAQUE recv
      if (rf95.recv(rxbuf, &rxlen)) {
        nbRecues++;

        // [V1-4] Écart entre le numéro reçu et le numéro attendu = trames perdues.
        // Le cast en uint8_t gère le passage de 255 à 0.
        if (synchronise) {
          nbPerdues += (uint8_t)(rxbuf[0] - attendu);
        }
        attendu = rxbuf[0] + 1;
        synchronise = true;

        preparerEcran();
        // [V1-12] La qualité du lien est mesurée (RSSI en dBm, SNR en dB).
        M5.Lcd.printf("#%u %uo R%d S%d P%lu\n", rxbuf[0], rxlen,
                      rf95.lastRssi(), rf95.lastSNR(),
                      (unsigned long)nbPerdues);

        // Détail complet sur le port série, pour les jeux d'essai du rapport.
        Serial.printf("Trame %lu (%u octets) :", (unsigned long)nbRecues, rxlen);
        for (uint8_t i = 0; i < rxlen; i++) Serial.printf(" %02X", rxbuf[i]);
        Serial.println();
      }
      state = ECOUTE;
      break;

    default:
      state = ECOUTE;            // [V1-13] Sortie sûre si state prend une valeur imprévue
      break;
  }
}
