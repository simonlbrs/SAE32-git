#include <SPI.h>
#include <M5Stack.h>
#include <RH_RF95.h>

#define RFM95_CS 5
#define RFM95_DI00 36
RH_RF95 rf95(RFM95_CS, RFM95_DI00);

uint8_t state;
#define EMISSION 0
#define DELAI 1
uint8_t txbuf[2];

void setup() {
  M5.begin();
  M5.Power.begin();
  M5.Lcd.clear(BLACK);
  M5.Lcd.setTextColor(WHITE);
  M5.Lcd.setTextSize(2);
  M5.Lcd.println("TX Test Canal");

  delay(1000);
  rf95.init();
  rf95.setTxPower(8);
  rf95.setModemConfig(RH_RF95::Bw125Cr45Sf128);
  //Changement de fréquence 
  rf95.setFrequency(868.1);

  state = EMISSION;
  delay(3000);
  M5.Lcd.println("Boucle principale");
}

void loop() {
  switch (state) {
    case EMISSION:
      M5.Lcd.println("EMISSION");
      txbuf[0] = 0xAA;
      txbuf[1] = 0x55;
      rf95.send(txbuf, 2);
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