#include <SPI.h>
#include <RH_RF95.h>
#include <M5Stack.h>

#define RFM95_CS 5
#define RFM95_DIO0 36
RH_RF95 rf95(RFM95_CS, RFM95_DIO0);

uint8_t state;
uint8_t txbuf[RH_RF95_MAX_MESSAGE_LEN];

#define EMISSION 0
#define DELAI    1

int i;
uint16_t S, SP;

void setup() {
  M5.begin();
  M5.Power.begin();
  M5.Lcd.setTextSize(2);
  M5.Lcd.println("== TX RS - CTRL ==");
  M5.Lcd.setTextSize(1);
  M5.Lcd.println("Maintenir BtnB : Corrompre champ S\n");

  if (!rf95.init()) {
    M5.Lcd.println("Init RF95 failed !");
    while (1);
  }

  rf95.setModemConfig(RH_RF95::Bw125Cr45Sf128);
  rf95.setFrequency(867.7);

  state = EMISSION;
  delay(2000);
}

void loop() {
  M5.update(); // Lecture de l'état des boutons

  switch (state) {
    case EMISSION:
      if (M5.Lcd.getCursorY() > 220) {
        M5.Lcd.fillScreen(BLACK);
        M5.Lcd.setCursor(0, 0);
      }

      // 1. Calcul des sommes S et SP sur les 20 octets à 255
      S = 0; 
      SP = 0;
      for (i = 0; i < 20; i++) {
        txbuf[i] = 255;
        S = S + txbuf[i];             // S = 5100
        SP = SP + txbuf[i] * (i + 1); // SP = 53550
      }

      // 2. Découpage en Little Endian (octets 20 à 23)
      txbuf[20] = S & 0x00FF;
      txbuf[21] = (S & 0xFF00) >> 8;
      txbuf[22] = SP & 0x00FF;
      txbuf[23] = (SP & 0xFF00) >> 8;

      M5.Lcd.printf("Envoi S=%d, SP=%d ", S, SP);

      // 3. INJECTION D'ERREUR SUR LE CHAMP DE CONTRÔLE (Bouton B)
      if (M5.BtnB.isPressed()) {
        txbuf[21] ^= 0x0F; // Altération de l'octet de poids fort du champ S
        M5.Lcd.print("[ERR CHAMP S]");
      }
      M5.Lcd.println();

      rf95.send(txbuf, 24);
      rf95.waitPacketSent();
      state = DELAI;
      break;

    case DELAI:
      delay(1500);
      state = EMISSION;
      break;
  }
}