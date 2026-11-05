#include <SPI.h>
#include <RH_RF95.h>
#include <M5Stack.h>

#define RFM95_CS 5
#define RFM95_DIO0 36
RH_RF95 rf95(RFM95_CS, RFM95_DIO0);

uint8_t txbuf[RH_RF95_MAX_MESSAGE_LEN];
uint8_t state, TxSeq;
uint8_t FCS;
int i;

#define EMISSION 0
#define DELAI    1

void setup() {
  M5.begin();
  M5.Power.begin();
  M5.Lcd.setTextSize(2);
  M5.Lcd.println("TX - NO CRC HW");
  M5.Lcd.setTextSize(1);
  M5.Lcd.println("Emission trames pour test bruit radio");

  if (!rf95.init()) {
    M5.Lcd.println("Init RF95 failed !");
    while (1);
  }

  rf95.setModemConfig(RH_RF95::Bw125Cr45Sf128);
  rf95.setFrequency(867.7);

  TxSeq = 0;
  state = EMISSION;
  delay(2000);
}

void loop() {
  switch (state) {
    case EMISSION:
      if (M5.Lcd.getCursorY() > 220) {
        M5.Lcd.fillScreen(BLACK);
        M5.Lcd.setCursor(0, 0);
      }

      M5.Lcd.printf("Emission Seq %d : ", TxSeq);

      txbuf[0] = 1;      // TYPE_DATA
      txbuf[1] = TxSeq;  // Numéro de séquence
      for (i = 2; i < 20; i++) {
        txbuf[i] = 255;  // Données de test
      }

      // Calcul du FCS logiciel (XOR)
      FCS = 0;
      for (i = 0; i < 20; i++) FCS = FCS ^ txbuf[i];
      txbuf[20] = FCS;

      rf95.send(txbuf, 21);
      rf95.waitPacketSent();
      M5.Lcd.println("OK");

      TxSeq++;
      state = DELAI;
      break;

    case DELAI:
      delay(1000); // Respect du rapport cyclique et cadence de test
      state = EMISSION;
      break;
  }
}