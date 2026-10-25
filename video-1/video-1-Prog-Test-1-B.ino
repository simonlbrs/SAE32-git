/*
 * PROGRAMME : Récepteur Point à Point Simple LLC1 (Capsule 1)
 * MODIFICATIONS M5STACK :
 * - Adaptations identiques à l'émetteur (RadioHead RF95, M5Stack.h, 867.7 MHz)[cite: 4, 6].
 * - Affichage dynamique du nombre de trames et de leur contenu (0xAA et 0x55) sur l'écran[cite: 4, 6].
 */

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

#define ECOUTE    1
#define TEST_RX   2
#define AFFICHAGE 3

void setup() {
  M5.begin();
  M5.Power.begin(); 
  M5.Lcd.setTextSize(2);
  M5.Lcd.println("Rx Simple M5");
  M5.Lcd.setTextSize(1);

  delay(1000); 

  if (!rf95.init()) M5.Lcd.println("Init RF95 failed");
  else M5.Lcd.println("Init RF95 OK");

  rf95.setModemConfig(RH_RF95::Bw125Cr45Sf128); 
  rf95.setFrequency(867.7);

  state = ECOUTE; 
  delay(3000);    
  M5.Lcd.println("Boucle principale (Ecoute)");
}

void loop() {
  switch (state) {
    case ECOUTE:
      rf95.setModeRx(); // Active l'écoute radio
      state = TEST_RX;  
      break;

    case TEST_RX:
      if (rf95.available()) // Dès qu'une trame est détectée
      {
        state = AFFICHAGE;
      }
      break;

    case AFFICHAGE:
      rxlen = sizeof(rxbuf); // Réinitialisation impérative de la taille max
      if (rf95.recv(rxbuf, &rxlen)) 
      {
        if (M5.Lcd.getCursorY() > 220) {
          M5.Lcd.fillScreen(BLACK);
          M5.Lcd.setCursor(0, 0);
        }

        rxFrames++; 
        M5.Lcd.printf("[%d] Trame de %d octets reçue :\n   |", rxFrames, rxlen);
        for (int j = 0; j < rxlen; j++)
        {
          M5.Lcd.printf("%02X|", rxbuf[j]); // Affiche AA|55|
        }
        M5.Lcd.println();

        state = ECOUTE; 
      }
      break;
  }
}