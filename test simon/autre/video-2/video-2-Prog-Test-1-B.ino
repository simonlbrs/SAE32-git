/*
 * PROGRAMME : Récepteur DATA-ACK Point à Point (Couche 2 - Capsule 2)
 * MODIFICATIONS APPORTÉES POUR M5STACK (cf. Diapo 5/7 SAE32) :
 * 1. #include <RH_RF95.h> et #include <M5Stack.h> : Adaptation pour le module LoRa du M5Stack.
 * 2. #define RFM95_CS 5 / DIO0 36 : Broches SPI du transceiver RF95 sur M5Stack.
 * 3. M5.begin() / M5.Power.begin() : Initialisation du M5Stack dans setup().
 * 4. rf95.setModemConfig / setFrequency(867.7) : Configuration radio identique à l'émetteur.
 * 5. Remplacement de Serial.print/println/printf par M5.Lcd.print/println/printf.
 */

#include <SPI.h>
#include <RH_RF95.h> // MODIF M5Stack : Librairie RadioHead RF95
#include <M5Stack.h> // MODIF M5Stack : Librairie M5Stack

#define RFM95_CS 5   // MODIF M5Stack : Broche CS LoRa
#define RFM95_DIO0 36 // MODIF M5Stack : Broche DIO0 LoRa
RH_RF95 rf95(RFM95_CS, RFM95_DIO0); // MODIF M5Stack : Objet rf95

uint8_t state, i;
uint8_t rxbuf[RH_RF95_MAX_MESSAGE_LEN], rxbuflen = RH_RF95_MAX_MESSAGE_LEN;
uint8_t txbuf[RH_RF95_MAX_MESSAGE_LEN], txbuflen = RH_RF95_MAX_MESSAGE_LEN;
uint8_t rxlen = RH_RF95_MAX_MESSAGE_LEN;
uint8_t RxSeq;
uint32_t attente; // pour plus tard si full-duplex

#define E0 0
#define E1 1
#define E2 2
#define E3 3

#define canal 1
#define TYPE_DATA 1
#define TYPE_ACK 2

void setup() {
  // MODIF M5Stack : Initialisation de l'écran et de l'alimentation du M5Stack
  M5.begin();
  M5.Power.begin(); // Init Power module.
  M5.Lcd.setTextSize(1);
  M5.Lcd.println("Rx DATA-ACK M5");

  delay(1000); // test... Attention au rapport cyclique 1%

  if (!rf95.init())
    M5.Lcd.println("init M5 failed");
  else
    M5.Lcd.println("init M5 OK");

  // MODIF M5Stack : Configuration de la radio LoRa
  rf95.setModemConfig(RH_RF95::Bw125Cr45Sf128); // 128=SF7
  rf95.setFrequency(867.7);

  state = E0;
  delay(3000);
  M5.Lcd.println("Boucle principale");
  RxSeq = 255; // pas encore reçu 1ère trame n°0. 0-1 = 255 sur 8 bits non signés
}

void loop() {
  int j; // index pour parcourir les octets de la trame reçue à afficher
  switch (state)
  {
    case E0: // se mettre en réception des données
      M5.Lcd.println("E0");
      rf95.setModeRx();
      state = E1;
      break;

    case E1: // vérifier si un message reçu est disponible
      rxlen = RH_RF95_MAX_MESSAGE_LEN; // taille max en réception
      if (rf95.recv(rxbuf, &rxlen))
      {
        if (rxbuf[0] == TYPE_DATA)
        { // si le premier octet du message est de type DATA
          state = E2;
        }
        else
          state = E0;
      }
      break;

    case E2: // afficher la trame
      // Nettoyage automatique de l'écran M5Stack lorsqu'il est plein
      if (M5.Lcd.getCursorY() > 220) {
        M5.Lcd.fillScreen(BLACK);
        M5.Lcd.setCursor(0, 0);
      }

      if (RxSeq != rxbuf[1]) // si 1ère fois que l'on reçoit cette trame
      {
        RxSeq = rxbuf[1];
        M5.Lcd.printf("[%d] ", RxSeq);
        M5.Lcd.printf("DATA de %d octets :", rxlen);
        M5.Lcd.println();
        for (j = 0; j < rxlen; j++)
        {
          M5.Lcd.printf("%02x|", rxbuf[j]);
        }
        M5.Lcd.println();
      }
      else // si on a déjà reçu cette même trame
      {
        M5.Lcd.println("Duplication");
      }
      state = E3;
      break;

    case E3: // envoyer ACK
      M5.Lcd.println("E3");
      txbuf[0] = TYPE_ACK; // octet indiquant le type message ACK
      txbuf[1] = rxbuf[1]; // ACK de même numéro que DATA
      rf95.send(txbuf, 2); // envoi des 2 octets de la trame ACK
      rf95.waitPacketSent();
      state = E0;
      break;

    default:
      state = E0;
      break;
  }
}