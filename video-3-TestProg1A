#include <SPI.h>
#include <RH_RF95.h>
#include <M5Stack.h>

#define RFM95_CS 5   // Pin CS pour M5Stack LoRa
#define RFM95_DIO0 36 // Pin DIO0 pour M5Stack LoRa
RH_RF95 rf95(RFM95_CS, RFM95_DIO0);

#define E0 0
#define E1 1
#define E2 2
#define E3 3
#define E4 4
#define E5 5

#define TYPE_DATA 1
#define TYPE_ACK 2
#define TIMEOUT_ACK 2000 // 2000 ms (adapté au temps de transmission LoRa)

uint8_t txbuf[RH_RF95_MAX_MESSAGE_LEN];
uint8_t rxbuf[RH_RF95_MAX_MESSAGE_LEN];
uint8_t rxlen = RH_RF95_MAX_MESSAGE_LEN;

uint8_t state = E0;
uint8_t TxSeq = 0;
uint8_t credit = 5;
uint32_t attente = 0;
uint8_t FCS = 0;
int i;

void setup() {
  M5.begin();
  M5.Power.begin();
  Serial.begin(115200);

  M5.Lcd.setTextSize(2);
  M5.Lcd.println("=== TX XOR M5Stack ===");
  M5.Lcd.setTextSize(1);
  M5.Lcd.println("Maintenir BtnA : Simuler erreur DATA");
  M5.Lcd.println("Maintenir BtnB : Simuler erreur ACK\n");

  if (!rf95.init()) {
    M5.Lcd.println("Init RF95 failed !");
    Serial.println("Init RF95 failed !");
    while (1);
  }
  M5.Lcd.println("Init RF95 OK");

  // Configuration radio LoRa conforme SAE32
  rf95.setModemConfig(RH_RF95::Bw125Cr45Sf128);
  rf95.setFrequency(867.7);

  state = E0;
  TxSeq = 0;
  credit = 5;
  delay(2000);
}

void loop() {
  M5.update(); // Lecture des boutons du M5Stack

  switch (state) {
    case E0:
      if (M5.Lcd.getCursorY() > 220) {
        M5.Lcd.fillScreen(BLACK);
        M5.Lcd.setCursor(0, 0);
      }
      
      M5.Lcd.printf("Emission Seq %d : ", TxSeq);
      Serial.printf("Emission Seq %d : ", TxSeq);

      // Formatage des 20 octets utiles
      txbuf[0] = TYPE_DATA;
      txbuf[1] = TxSeq;
      for (i = 2; i < 20; i++) {
        txbuf[i] = 255; // 18 octets de payload à 0xFF
      }

      // Calcul du FCS (XOR des 20 octets)
      FCS = 0;
      for (i = 0; i < 20; i++) {
        FCS = FCS ^ txbuf[i];
      }
      txbuf[20] = FCS;

      // JEU D'ESSAI : Maintenir Bouton A corrompt un octet de DATA après le calcul du XOR
      if (M5.BtnA.isPressed()) {
        txbuf[5] = txbuf[5] ^ 0xFF; 
        M5.Lcd.print("[ERR DATA] ");
        Serial.print("[ERR DATA] ");
      }

      // Affichage debug sur le port série
      for (i = 0; i < 21; i++) Serial.printf("%02X ", txbuf[i]);
      Serial.println();

      // Envoi de la trame de 21 octets
      rf95.send(txbuf, 21);
      rf95.waitPacketSent();
      credit--;
      state = E1;
      break;

    case E1:
      attente = millis() + TIMEOUT_ACK; // Armement du chien de garde
      state = E2;
      break;

    case E2:
      rf95.setModeRx(); // Passage en écoute pour l'ACK
      state = E3;
      break;

    case E3:
      if (millis() > attente) {
        M5.Lcd.println("Timeout CdG !");
        Serial.println("Timeout CdG !");
        state = E5;
      } else {
        rxlen = RH_RF95_MAX_MESSAGE_LEN;
        if (rf95.recv(rxbuf, &rxlen)) {
          // JEU D'ESSAI : Maintenir Bouton B simule une erreur sur l'ACK reçu
          if (M5.BtnB.isPressed()) {
            rxbuf[2] = rxbuf[2] + 1;
            M5.Lcd.print("[ERR ACK] ");
            Serial.print("[ERR ACK] ");
          }

          // Vérification : Type ACK + Bon NumSeq + XOR juste
          if ((rxbuf[0] == TYPE_ACK) && (rxbuf[1] == TxSeq) && ((rxbuf[0] ^ rxbuf[1]) == rxbuf[2])) {
            state = E4;
          } else {
            state = E2; // Par défaut on retourne écouter
            // Si c'est notre ACK mais que le XOR est faux -> Échec immédiat sans attendre le CdG
            if ((rxbuf[0] == TYPE_ACK) && (rxbuf[1] == TxSeq) && ((rxbuf[0] ^ rxbuf[1]) != rxbuf[2])) {
              M5.Lcd.println("ACK corrompu !");
              Serial.println("ACK corrompu !");
              state = E5;
            }
          }
        }
      }
      break;

    case E4:
      M5.Lcd.println("ACK RECU OK !");
      Serial.println("ACK_RECU");
      TxSeq++;
      credit = 5;
      delay(1000); // Pause pour respecter le rapport cyclique LoRa
      state = E0;
      break;

    case E5:
      if (credit == 0) {
        M5.Lcd.println("-> ECHEC DEFINITIF (5 tentatives)");
        Serial.println("ECHEC");
        credit = 5;
        TxSeq++; // On passe à la trame suivante
        delay(1000);
        state = E0;
      } else {
        M5.Lcd.printf("-> Reemission (Essai %d/5)\n", 5 - credit + 1);
        Serial.printf("Nouvelle tentative n %d\n", 5 - credit);
        delay(500);
        state = E0;
      }
      break;

    default:
      state = E0;
      break;
  }
}