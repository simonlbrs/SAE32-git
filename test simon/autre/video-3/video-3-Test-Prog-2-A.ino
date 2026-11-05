#include <SPI.h>
#include <RH_RF95.h>
#include <M5Stack.h>

#define RFM95_CS 5
#define RFM95_DIO0 36
RH_RF95 rf95(RFM95_CS, RFM95_DIO0);

uint8_t txbuf[24];
uint16_t S = 0;  // Somme des octets Di utiles (sur 16 bits)
uint16_t SP = 0; // Somme pondérée par le rang i de 1 à 20 (sur 16 bits)

void setup() {
  M5.begin();
  M5.Power.begin();
  Serial.begin(115200);

  M5.Lcd.setTextSize(2);
  M5.Lcd.println("== TX Reed-Solomon ==");
  M5.Lcd.setTextSize(1);
  M5.Lcd.println("BtnA : Corrompre Octet de rang 5 (255 -> 200)");
  M5.Lcd.println("BtnB : Corrompre le champ de controle S\n");

  if (!rf95.init()) {
    M5.Lcd.println("Init RF95 failed !");
    while (1);
  }
  rf95.setModemConfig(RH_RF95::Bw125Cr45Sf128);
  rf95.setFrequency(867.7);
}

void loop() {
  M5.update();
  if (M5.Lcd.getCursorY() > 220) {
    M5.Lcd.fillScreen(BLACK);
    M5.Lcd.setCursor(0, 0);
  }

  // 1. Remplissage des 20 octets utiles à 255 (exemple de la vidéo à 23:47)
  S = 0;
  SP = 0;
  for (int i = 0; i < 20; i++) {
    txbuf[i] = 255;
    S += txbuf[i];             // S = Somme(Di) -> 5100
    SP += txbuf[i] * (i + 1);  // SP = Somme(Di * rang) où rang va de 1 à 20 -> 53550
  }

  // 2. Découpage de S (16 bits) et SP (16 bits) en 4 octets en fin de trame
  txbuf[20] = (S >> 8) & 0xFF;  // S poids fort (MSB)
  txbuf[21] = S & 0xFF;         // S poids faible (LSB)
  txbuf[22] = (SP >> 8) & 0xFF; // SP poids fort (MSB)
  txbuf[23] = SP & 0xFF;        // SP poids faible (LSB)

  M5.Lcd.printf("Envoi RS(24,20) | S=%d, SP=%d ", S, SP);

  // Simulations d'erreurs pour les tests
  if (M5.BtnA.isPressed()) {
    txbuf[4] = 200; // Index 4 = Rang R = 5 (Erreur E = 250 - 200 = -55)
    M5.Lcd.print("[ERR RANG 5]");
  }
  if (M5.BtnB.isPressed()) {
    txbuf[21] ^= 0x0F; // Altération du champ de contrôle S
    M5.Lcd.print("[ERR S]");
  }
  M5.Lcd.println();

  rf95.send(txbuf, 24);
  rf95.waitPacketSent();

  delay(2000);
}