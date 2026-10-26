#include <M5Stack.h>
#include <RH_RF95.h>

// Broches SPI spécifiques au module LoRa du M5Stack
#define RFM95_CS 5
#define RFM95_DI00 36
RH_RF95 rf95(RFM95_CS, RFM95_DI00);

// Déclaration de l'automate (identique au cours du prof)
uint8_t state;
#define EMISSION 0
#define DELAI 1

// Buffer d'émission de 2 octets
uint8_t txbuf[2];

void setup() {
  M5.begin();
  M5.Power.begin();
  M5.Lcd.clear(BLACK);
  M5.Lcd.setTextColor(WHITE);
  M5.Lcd.setTextSize(2);
  M5.Lcd.println("TX simple M5");

  delay(1000);

  // Initialisation radio
  if (!rf95.init()) {
    M5.Lcd.println("Init radio failed");
  } else {
    M5.Lcd.println("Init radio OK");
  }

  // Configuration radio demandée
  rf95.setTxPower(8);
  rf95.setModemConfig(RH_RF95::Bw125Cr45Sf128);
  rf95.setFrequency(867.7);

  // État initial de l'automate
  state = EMISSION;

  // Délai de 3 secondes pour laisser le temps d'allumer le récepteur
  delay(3000);
  M5.Lcd.println("Boucle principale");
}

void loop() {
  switch (state) {
    case EMISSION:
      M5.Lcd.println("EMISSION");

      // Formatage de la trame : alternance 0xAA et 0x55
      txbuf[0] = 0xAA;
      txbuf[1] = 0x55;

      // Émission radio et attente de la fin physique d'envoi
      rf95.send(txbuf, 2);
      rf95.waitPacketSent();

      // Transition automatique vers l'état DELAI
      state = DELAI;
      break;

    case DELAI:
      M5.Lcd.println("DELAI");

      // Attente bloquante d'une seconde pour éviter l'overrun
      delay(1000);

      // Retour vers l'état EMISSION
      state = EMISSION;
      break;

    default:
      break;
  }
}