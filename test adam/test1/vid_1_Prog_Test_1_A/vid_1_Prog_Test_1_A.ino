#include <SPI.h>
#include <M5Stack.h>
#include <RH_RF95.h>

// Broches SPI spécifiques au module LoRa du M5Stack
#define RFM95_CS 5
#define RFM95_DI00 36
RH_RF95 rf95(RFM95_CS, RFM95_DI00);

// Déclaration de l'automate récepteur
uint8_t state;
#define ECOUTE 1
#define TEST_RX 2
#define AFFICHAGE 3

// Buffer de réception
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

  // Initialisation radio
  if (!rf95.init()) {
    M5.Lcd.println("Init radio failed");
  } else {
    M5.Lcd.println("Init radio OK");
  }

  // Configuration radio identique à l'émetteur
  rf95.setModeRx();
  rf95.setModemConfig(RH_RF95::Bw125Cr45Sf128);
  rf95.setFrequency(867.7);

  // État initial
  state = ECOUTE;

  delay(3000);
  M5.Lcd.println("Boucle principale");
}

void loop() {
  switch (state) {
    case ECOUTE:
      M5.Lcd.println("ECOUTE");
      rf95.setModeRx();
      state = TEST_RX;
      break;

    case TEST_RX:
      // Vérification si un message est disponible
      if (rf95.available()) {
        state = AFFICHAGE;
      }
      break;

    case AFFICHAGE:
      rxlen = sizeof(rxbuf);
      if (rf95.recv(rxbuf, &rxlen)) {
        compteurTrames++;
        M5.Lcd.printf("Trame %d (%d octets) : |", compteurTrames, rxlen);
        for (int i = 0; i < rxlen; i++) {
          M5.Lcd.printf("%02X|", rxbuf[i]);
        }
        M5.Lcd.println();
      }
      // Retour en écoute pour la trame suivante
      state = ECOUTE;
      break;

    default:
      break;
  }
}