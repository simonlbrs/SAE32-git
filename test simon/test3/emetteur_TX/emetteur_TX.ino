#include <SPI.h>
#include <RH_RF95.h>
#include <M5Stack.h>

#define RFM95_CS 5
#define RFM95_DIO0 36
RH_RF95 rf95(RFM95_CS, RFM95_DIO0);

uint8_t txbuf[RH_RF95_MAX_MESSAGE_LEN];
uint8_t rxbuf[RH_RF95_MAX_MESSAGE_LEN];
uint8_t rxlen = RH_RF95_MAX_MESSAGE_LEN;

uint8_t state, RxSeq, TxSeq, credit;
uint32_t attente;
uint8_t FCS;
int i;

#define E0 0
#define E1 1
#define E2 2
#define E3 3
#define E4 4
#define E5 5

#define TYPE_DATA 1
#define TYPE_ACK  2
#define TIMEOUT_ACK 1000

void setup() {
  M5.begin();
  M5.Power.begin();
  M5.Lcd.setTextSize(2);
  M5.Lcd.println("TX XOR - Dble Err");
  M5.Lcd.setTextSize(1);
  M5.Lcd.println("Maintenir BtnA : Erreur double");

  if (!rf95.init()) {
    M5.Lcd.println("Init RF95 failed !");
    while (1);
  }

  rf95.setModemConfig(RH_RF95::Bw125Cr45Sf128);
  rf95.setFrequency(867.7);

  state = E0;
  TxSeq = 0;
  credit = 5;
  delay(2000);
}

void loop() {
  M5.update(); // Lecture du bouton A

  switch (state) {
    case E0:
      if (M5.Lcd.getCursorY() > 220) {
        M5.Lcd.fillScreen(BLACK);
        M5.Lcd.setCursor(0, 0);
      }

      M5.Lcd.printf("EMISSION Seq %d : ", TxSeq);

      txbuf[0] = TYPE_DATA;
      txbuf[1] = TxSeq;
      for (i = 2; i < 20; i++) {
        txbuf[i] = 255; // Octets nominaux à 0xFF
      }

      // 1. Calcul normal du FCS (XOR sur les 20 premiers octets)
      FCS = 0;
      for (i = 0; i < 20; i++) FCS = FCS ^ txbuf[i];
      txbuf[20] = FCS;

      // 2. INJECTION DE L'ERREUR DOUBLE COMPENSATOIRE
      if (M5.BtnA.isPressed()) {
        txbuf[2] ^= 0x01; // Inversion du bit 0 de l'octet 2 (255 -> 254)
        txbuf[3] ^= 0x01; // Inversion du bit 0 de l'octet 3 (255 -> 254)
        M5.Lcd.print("[DOUBLE ERR]");
      }
      M5.Lcd.println();

      rf95.send(txbuf, 21);
      rf95.waitPacketSent();
      credit--;
      state = E1;
      break;

    case E1:
      attente = millis() + TIMEOUT_ACK;
      state = E2;
      break;

    case E2:
      rf95.setModeRx();
      state = E3;
      break;

    case E3:
      if (millis() > attente) {
        state = E5;
      } else {
        rxlen = sizeof(rxbuf);
        if (rf95.recv(rxbuf, &rxlen)) {
          if ((rxbuf[0] == TYPE_ACK) && (rxbuf[1] == TxSeq) && ((rxbuf[0] ^ rxbuf[1]) == rxbuf[2])) {
            state = E4;
          } else {
            state = E2;
          }
        }
      }
      break;

    case E4:
      M5.Lcd.println("-> ACK RECU");
      TxSeq++;
      credit = 5;
      delay(1000);
      state = E0;
      break;

    case E5:
      if (credit == 0) {
        M5.Lcd.println("-> ECHEC DEFINITIF");
        credit = 5;
        TxSeq++;
        delay(1000);
        state = E0;
      } else {
        M5.Lcd.printf("-> Reemission (%d/5)\n", 5 - credit);
        state = E0;
      }
      break;
  }
}