#include <SPI.h> // CORRECTION : Inclusion obligatoire du bus SPI
#include <M5Stack.h>
#include <RH_RF95.h>

// CORRECTION : Utilisation de DIO0 (avec un 'O' comme Oscar, et non '0')
#define RFM95_CS 5 
#define RFM95_DIO0 36 
RH_RF95 rf95(RFM95_CS, RFM95_DIO0);

uint8_t state; 
#define ECOUTE 1 
#define TEST_RX 2 
#define AFFICHAGE 3

uint8_t rxbuf[RH_RF95_MAX_MESSAGE_LEN]; 
uint8_t rxlen; 
int compteurTrames = 0;

void setup() { 
  M5.begin(); 
  M5.Power.begin(); 
  M5.Lcd.clear(BLACK); 
  M5.Lcd.setTextColor(WHITE); 
  M5.Lcd.setTextSize(2); 
  M5.Lcd.println("RX simple M5");
  delay(1000);

  if (!rf95.init()) { 
    M5.Lcd.println("Init radio failed"); 
  } else { 
    M5.Lcd.println("Init radio OK"); 
  }

  rf95.setModemConfig(RH_RF95::Bw125Cr45Sf128); 
  rf95.setFrequency(867.7);

  state = ECOUTE;
  delay(3000); 
  M5.Lcd.println("Boucle principale"); 
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
        compteurTrames++; 
        if (M5.Lcd.getCursorY() > 220) {
          M5.Lcd.fillScreen(BLACK);
          M5.Lcd.setCursor(0, 0);
        }
        M5.Lcd.printf("Trame %d (%d o): |", compteurTrames, rxlen); 
        for (int i = 0; i < rxlen; i++) { 
          M5.Lcd.printf("%02X|", rxbuf[i]); 
        } 
        M5.Lcd.println(); 
      } 
      state = ECOUTE; 
      break;

    default: 
      break; 
  } 
}