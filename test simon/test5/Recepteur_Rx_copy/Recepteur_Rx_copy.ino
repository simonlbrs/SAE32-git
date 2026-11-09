#include <SPI.h>
#include <RH_RF95.h>
#include <M5Stack.h>

#define RFM95_CS 5
#define RFM95_DIO0 36
RH_RF95 rf95(RFM95_CS, RFM95_DIO0);

uint8_t rxbuf[RH_RF95_MAX_MESSAGE_LEN];
uint8_t rxlen;
uint8_t FCSc, FCSr;
int rxTotal = 0;
int rxCorrompues = 0;
int i;

void setup() {
  M5.begin();
  M5.Power.begin();
  M5.Lcd.setTextSize(2);
  M5.Lcd.println("RX - NO CRC HW");
  M5.Lcd.setTextSize(1);
  M5.Lcd.println("Ecoute sans filtre CRC physique");

  if (!rf95.init()) {
    M5.Lcd.println("Init RF95 failed !");
    while (1);
  }

  rf95.setModemConfig(RH_RF95::Bw125Cr45Sf128);
  rf95.setFrequency(867.7);

  delay(2000);
  rf95.setModeRx();
}

void loop() {
  rxlen = sizeof(rxbuf);

  // Avec le CRC matériel désactivé, rf95.recv renvoie TOUTES les trames,
  // y compris celles bruitées ou ayant subi des inversions de bits électromagnétiques
  if (rf95.recv(rxbuf, &rxlen)) {
    rxTotal++;

    if (M5.Lcd.getCursorY() > 200) {
      M5.Lcd.fillScreen(BLACK);
      M5.Lcd.setCursor(0, 0);
    }

    if (rxlen == 21 && rxbuf[0] == 1) { // Trame DATA de 21 octets
      FCSr = rxbuf[20];
      FCSc = 0;
      for (i = 0; i < 20; i++) FCSc = FCSc ^ rxbuf[i];

      if (FCSc == FCSr) {
        M5.Lcd.printf("[%d] Seq %d : TRAME INTACTE\n", rxTotal, rxbuf[1]);
      } else {
        rxCorrompues++;
        M5.Lcd.printf("[%d] Seq %d : BRUIT/ERREUR DETECTEE !\n", rxTotal, rxbuf[1]);
        M5.Lcd.printf("    FCS Recu: %02X != Calc: %02X\n", FCSr, FCSc);
        M5.Lcd.printf("    Total erreurs : %d/%d\n", rxCorrompues, rxTotal);
      }
    } else {
      // Trames parasites ambiantes captées car le filtrage est ouvert
      M5.Lcd.printf("[%d] Paquet parasite (len: %d)\n", rxTotal, rxlen);
    }

    rf95.setModeRx(); // Réactivation de l'écoute
  }
}