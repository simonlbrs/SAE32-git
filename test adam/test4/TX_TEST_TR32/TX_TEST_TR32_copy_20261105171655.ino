#include <SPI.h>
#include <M5Stack.h>
#include <RH_RF95.h>

#define RFM95_CS 5
#define RFM95_DI00 36
RH_RF95 rf95(RFM95_CS, RFM95_DI00);

uint8_t state;
#define EMISSION 0
#define DELAI 1

//Buffer agrandi à 32 octets
uint8_t txbuf[32];
uint8_t i;

void setup() {
  M5.begin();
  M5.Power.begin();
  M5.Lcd.clear(BLACK);
  M5.Lcd.setTextColor(WHITE);
  M5.Lcd.setTextSize(2);
  M5.Lcd.println("TX 32 Octets");

  delay(1000);
  rf95.init();
  rf95.setTxPower(8);
  rf95.setModemConfig(RH_RF95::Bw125Cr45Sf128);
  rf95.setFrequency(867.7);

  state = EMISSION;
  delay(3000);
  M5.Lcd.println("Boucle principale");
}

void loop() {
  switch (state) {
    case EMISSION:
      M5.Lcd.println("EMISSION");
      // Remplir le tableau de 0x00 à 0x1F
      for (i = 0; i < 32; i++) {
        txbuf[i] = i;
      }
      //Envoie 32 octets au lieu de 2
      rf95.send(txbuf, 32);
      rf95.waitPacketSent();
      state = DELAI;
      break;

    case DELAI:
      M5.Lcd.println("DELAI");
      delay(1000);
      state = EMISSION;
      break;

    default:
      break;
  }
}