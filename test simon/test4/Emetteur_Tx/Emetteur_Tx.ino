#include <SPI.h>
#include <RH_RF95.h>
#include <M5Stack.h>

#define RFM95_CS 5
#define RFM95_DIO0 36
RH_RF95 rf95(RFM95_CS, RFM95_DIO0);

uint8_t state, RxSeq;
uint8_t rxbuf[RH_RF95_MAX_MESSAGE_LEN];
uint8_t txbuf[RH_RF95_MAX_MESSAGE_LEN];
uint8_t rxlen = RH_RF95_MAX_MESSAGE_LEN;
uint8_t FCSc, FCSr;
int i, j;

#define E0 0
#define E1 1
#define E2 2
#define E3 3

#define TYPE_DATA 1
#define TYPE_ACK  2

void setup() {
  M5.begin();
  M5.Power.begin();
  M5.Lcd.setTextSize(2);
  M5.Lcd.println("RX XOR - Dble Err");
  M5.Lcd.setTextSize(1);

  if (!rf95.init()) {
    M5.Lcd.println("Init RF95 failed !");
    while (1);
  }

  rf95.setModemConfig(RH_RF95::Bw125Cr45Sf128);
  rf95.setFrequency(867.7);

  state = E0;
  RxSeq = 255;
  delay(2000);
  M5.Lcd.println("En attente de trames...");
}

void loop() {
  switch (state) {
    case E0:
      rf95.setModeRx();
      state = E1;
      break;

    case E1:
      rxlen = sizeof(rxbuf);
      if (rf95.recv(rxbuf, &rxlen) && rxbuf[0] == TYPE_DATA && rxlen == 21) {
        FCSr = rxbuf[20];
        FCSc = 0;
        for (i = 0; i < 20; i++) FCSc = FCSc ^ rxbuf[i];

        if (FCSc == FCSr) {
          state = E2; // Trame jugée valide par le calcul
        } else {
          if (M5.Lcd.getCursorY() > 220) { M5.Lcd.fillScreen(BLACK); M5.Lcd.setCursor(0, 0); }
          M5.Lcd.printf("Trame corrompue rejetee (FCS: %02X != %02X)\n", FCSr, FCSc);
          state = E0;
        }
      }
      break;

    case E2: {
      if (M5.Lcd.getCursorY() > 200) {
        M5.Lcd.fillScreen(BLACK);
        M5.Lcd.setCursor(0, 0);
      }

      if (RxSeq != rxbuf[1]) {
        RxSeq = rxbuf[1];
        M5.Lcd.printf("[Seq %d] Trame jugee JUSTE !\n", RxSeq);
        // Affichage des deux premiers octets de payload pour prouver l'erreur non vue
        M5.Lcd.printf("D0=%02X D1=%02X (Devrait etre FF FF)\n", rxbuf[2], rxbuf[3]);
        if (rxbuf[2] != 0xFF || rxbuf[3] != 0xFF) {
          M5.Lcd.println("-> ANOMALIE : Erreur non detectee !");
        }
      } else {
        M5.Lcd.printf("[Seq %d] Duplication\n", rxbuf[1]);
      }
      state = E3;
    } break;

    case E3:
      txbuf[0] = TYPE_ACK;
      txbuf[1] = rxbuf[1];
      txbuf[2] = txbuf[0] ^ txbuf[1];
      rf95.send(txbuf, 3);
      rf95.waitPacketSent();
      state = E0;
      break;
  }
}