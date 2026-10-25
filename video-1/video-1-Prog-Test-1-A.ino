/*
 * PROGRAMME : Émetteur Point à Point Simple LLC1 (Capsule 1)
 * MODIFICATIONS M5STACK :
 * - Remplacement de RF22 par RH_RF95 (LoRa) et inclusion de <M5Stack.h>[cite: 4, 6].
 * - Broches CS=5 et DIO0=36 pour le module LoRa interne du M5Stack[cite: 6].
 * - Fréquence fixée à 867.7 MHz (SF7)[cite: 6].
 * - Affichage de l'automate sur l'écran LCD via M5.Lcd au lieu de Serial[cite: 6].
 */

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

void setup() {
  M5.begin();
  M5.Power.begin(); 
  M5.Lcd.setTextSize(2);
  M5.Lcd.println("Tx Simple M5");
  M5.Lcd.setTextSize(1);

  delay(1000); 

  if (!rf95.init()) M5.Lcd.println("Init RF95 failed");
  else M5.Lcd.println("Init RF95 OK");

  rf95.setModemConfig(RH_RF95::Bw125Cr45Sf128); // LoRa SF7
  rf95.setFrequency(867.7);

  state = EMISSION; 
  delay(3000); 
  M5.Lcd.println("Boucle principale (Envoi de 0xAA 0x55)");
}

void loop() {
  switch (state) {
    case EMISSION: 
      if (M5.Lcd.getCursorY() > 220) {
        M5.Lcd.fillScreen(BLACK);
        M5.Lcd.setCursor(0, 0);
      }

      M5.Lcd.println("-> EMISSION");
      
      txbuf[0] = 0xAA; // 10101010 en binaire
      txbuf[1] = 0x55; // 01010101 en binaire

      rf95.send(txbuf, 2);   
      rf95.waitPacketSent(); 

      state = DELAI; 
      break;

    case DELAI: 
      M5.Lcd.println("-> DELAI (1 sec)");
      delay(1000); // 1s avant d'émettre la trame suivante
      state = EMISSION; 
      break;
  }
}