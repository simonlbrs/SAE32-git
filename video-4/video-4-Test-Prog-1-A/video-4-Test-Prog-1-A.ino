#include <SPI.h> // CORRECTION : Inclusion obligatoire du bus SPI
#include <M5Stack.h>
#include <RH_RF95.h>

// CORRECTION : Utilisation de DIO0 (avec un 'O')
#define RFM95_CS 5 
#define RFM95_DIO0 36 
RH_RF95 rf95(RFM95_CS, RFM95_DIO0);

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
  M5.Lcd.println("TX simple M5");
  delay(1000);

  if (!rf95.init()) { 
    M5.Lcd.println("Init radio failed"); 
  } else { 
    M5.Lcd.println("Init radio OK"); 
  }

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
      if (M5.Lcd.getCursorY() > 220) {
        M5.Lcd.fillScreen(BLACK);
        M5.Lcd.setCursor(0, 0);
      }
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