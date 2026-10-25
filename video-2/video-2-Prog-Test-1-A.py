/*
  PROGRAMME : Émetteur DATA-ACK Point à Point (Couche 2 - Capsule 2)
 * MODIFICATIONS APPORTÉES POUR M5STACK (cf. Diapo 5/7 SAE32) :
 * 1. #include <RH_RF95.h> : Remplacement de la librairie RH_RF22 (WiNo) par RH_RF95 (LoRa M5Stack).
 * 2. #include <M5Stack.h> : Remplacement de <LowPower_Teensy3.h> par la librairie M5Stack.
 * 3. #define RFM95_CS 5 / DIO0 36 : Configuration des broches SPI du module LoRa M5Stack.
 * 4. M5.begin() / M5.Power.begin() : Initialisation de l'écran LCD et de l'alimentation dans setup().
 * 5. rf95.setModemConfig / setFrequency(867.7) : Configuration de la radio LoRa sur 867.7 MHz (SF7).
 * 6. Remplacement de Serial.print/println/printf par M5.Lcd.print/println/printf pour l'affichage sur l'écran.
 * 7. TIMEOUT_ACK porté à 500 ms (au lieu de 40 ms sur RF22) pour laisser le temps à la trame LoRa d'être émise et acquittée.
 */

#include <SPI.h>
#include <RH_RF95.h> // MODIF M5Stack : Nouvelle librairie RadioHead pour RF95 (LoRa)
#include <M5Stack.h> // MODIF M5Stack : Librairie matérielle M5Stack

#define RFM95_CS 5   // MODIF M5Stack : Broche Chip Select du module LoRa
#define RFM95_DIO0 36 // MODIF M5Stack : Broche DIO0 du module LoRa
RH_RF95 rf95(RFM95_CS, RFM95_DIO0); // MODIF M5Stack : Instanciation de rf95 au lieu de rf22(SS,9)

uint8_t rxbuf[RH_RF95_MAX_MESSAGE_LEN], rxbuflen = RH_RF95_MAX_MESSAGE_LEN;
uint8_t txbuf[RH_RF95_MAX_MESSAGE_LEN], txbuflen = RH_RF95_MAX_MESSAGE_LEN;
uint8_t rxlen = RH_RF95_MAX_MESSAGE_LEN;
uint8_t state, RxSeq, TxSeq, credit;
uint32_t attente;

#define E0 0
#define E1 1
#define E2 2
#define E3 3
#define E4 4
#define E5 5

#define canal 1
#define TYPE_DATA 1
#define TYPE_ACK 2
#define TIMEOUT_ACK 500 // MODIF M5Stack : 500 ms (au lieu de 40 ms) adapté au temps de vol LoRa SF7

void setup()
{
  // MODIF M5Stack : Initialisation de l'écran et du module d'alimentation M5Stack
  M5.begin();
  M5.Power.begin(); // Init Power module.
  M5.Lcd.setTextSize(1);
  M5.Lcd.println("Tx DATA-ACK M5");

  delay(1000); // test... Attention au rapport cyclique 1%

  if (!rf95.init())
    M5.Lcd.println("init M5 failed");
  else
    M5.Lcd.println("init M5 OK");

  // MODIF M5Stack : Configuration de la radio LoRa (Modulation et Fréquence)
  rf95.setModemConfig(RH_RF95::Bw125Cr45Sf128); // 128=SF7
  rf95.setFrequency(867.7);

  state = E0;
  delay(3000);
  TxSeq = 0; credit = 5;
  M5.Lcd.println("Boucle principale");
}

void loop() {
  switch (state)
  {
    case E0:
      // Nettoyage automatique de l'écran M5Stack lorsqu'il est plein
      if (M5.Lcd.getCursorY() > 220) {
        M5.Lcd.fillScreen(BLACK);
        M5.Lcd.setCursor(0, 0);
      }

      // MODIF M5Stack : Remplacement de Serial.printf par M5.Lcd.printf
      M5.Lcd.printf("EMISSION %d : ", TxSeq);

      txbuf[0] = TYPE_DATA; // type de message
      txbuf[1] = TxSeq;     // numéro de trame DATA émise
      txbuf[2] = 0xAA;      // infos
      txbuf[3] = 0x55;      // ..
      rf95.send(txbuf, 4);  // envoie de données (une trame de 4 octets)
      rf95.waitPacketSent();

      credit--;             // on vient d'émettre une fois de + la trame

      state = E1;
      break;

    case E1:
      attente = millis() + TIMEOUT_ACK; // armement CgD
      state = E2;
      break;

    case E2:
      rf95.setModeRx(); // mettre la radio en mode réception pour l'ACK
      state = E3;
      break;

    case E3: // attente d'ACK non bloquante (car test trame reçue durant le CdG)
      if (millis() > attente) // vérification si le watchdog est expiré
      { state = E5; }         // si attente expirée : pb !
      else                    // si attente en cours (CdG non expiré)
      {
        rxlen = sizeof(rxbuf); // MODIF M5Stack : réinitialisation de la taille max du buffer
        if (rf95.recv(rxbuf, &rxlen)) // vérifier si une trame est disponible
        {
          if ((rxbuf[0] == TYPE_ACK) && (rxbuf[1] == TxSeq))
          { // si la trame reçue est de type ACK et même numéro que DATA émise
            state = E4; // si oui on affiche la trame reçue
          }
          else state = E2; // sinon on retourne à l'état E2
        }
      }
      break;

    case E4:
      M5.Lcd.println("ACK_RECU");
      state = E0; TxSeq++; credit = 5; // trame suivante
      delay(1000); // MODIF M5Stack : Pause de 1s pour respecter le rapport cyclique LoRa et lire l'écran
      break;

    case E5: // si le watchdog expire sans réception d'ACK, ECHEC si crédit épuisé
      M5.Lcd.println("E5");
      if (credit == 0) // 5 tentatives passées
      {
        M5.Lcd.println("ECHEC");
        state = E0;
        credit = 5; TxSeq++; // trame suivante
        delay(1000);
        break;
      }
      else
      {
        M5.Lcd.printf("Nouvelle tentative n %d", 5 - credit);
        state = E0;
        M5.Lcd.println();
        delay(500);
        break;
      }

    default:
      state = E0;
      break;
  }
}