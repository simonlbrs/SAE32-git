#include <SPI.h>
#include <RH_RF95.h>
#include <M5Stack.h>

#define RFM95_CS 5
#define RFM95_DIO0 36
RH_RF95 rf95(RFM95_CS, RFM95_DIO0);

uint8_t state;
uint8_t rxbuf[RH_RF95_MAX_MESSAGE_LEN];
uint8_t rxlen = sizeof(rxbuf);
int rxFrames = 0;
int i, erreur, rang;
uint16_t Sr, SPr, Sc, SPc;

#define ECOUTE    1
#define TEST_RX   2
#define AFFICHAGE 3

void setup() {
  M5.begin();
  M5.Power.begin();
  M5.Lcd.setTextSize(2);
  M5.Lcd.println("== RX RS - CTRL ==");
  M5.Lcd.setTextSize(1);

  if (!rf95.init()) {
    M5.Lcd.println("Init RF95 failed !");
    while (1);
  }

  rf95.setModemConfig(RH_RF95::Bw125Cr45Sf128);
  rf95.setFrequency(867.7);

  state = ECOUTE;
  delay(2000);
  M5.Lcd.println("En attente de trames...");
}

void loop() {
  switch (state) {
    case ECOUTE:
      rf95.setModeRx();
      state = TEST_RX;
      break;

    case TEST_RX:
      if (rf95.available()) {
        state = AFFICHAGE;
      }
      break;

    case AFFICHAGE:
      rxlen = sizeof(rxbuf);
      if (rf95.recv(rxbuf, &rxlen)) {
        rxFrames++;
        if (M5.Lcd.getCursorY() > 200) {
          M5.Lcd.fillScreen(BLACK);
          M5.Lcd.setCursor(0, 0);
        }

        if (rxlen == 24) {
          // Reconstitution des sommes reçues (Little Endian)
          Sr = rxbuf[20] + rxbuf[21] * 256;
          SPr = rxbuf[22] + rxbuf[23] * 256;

          // Calcul des sommes locales sur les données
          Sc = 0; 
          SPc = 0;
          for (i = 0; i < 20; i++) {
            Sc = Sc + rxbuf[i];
            SPc = SPc + rxbuf[i] * (i + 1);
          }

          // Analyse de cohérence
          if (Sr != Sc) {
            erreur = Sc - Sr;
            rang = (SPc - SPr) / erreur;

            // Une erreur sur le champ de contrôle donne un rang aberrant (hors intervalle 1..20)
            if (rang >= 1 && rang <= 20) {
              rxbuf[rang - 1] = rxbuf[rang - 1] - erreur;
              M5.Lcd.printf("[%d] Correction au rang %d\n", rxFrames, rang);
            } else {
              M5.Lcd.printf("[%d] Erreur sur redondance (DATA intacte)\n", rxFrames);
              M5.Lcd.printf("    Sr=%d Sc=%d (Rang hors limites: %d)\n", Sr, Sc, rang);
            }
          } 
          else if (SPr != SPc) {
            M5.Lcd.printf("[%d] Erreur sur redondance SP (DATA intacte)\n", rxFrames);
          } 
          else {
            M5.Lcd.printf("[%d] Trame OK sans erreur\n", rxFrames);
          }
        }
        state = ECOUTE;
      }
      break;
  }
}