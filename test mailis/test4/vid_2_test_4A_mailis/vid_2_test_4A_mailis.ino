#include <SPI.h>
#include <M5Stack.h>
#include <RH_RF95.h>

#define RFM95_CS 5   
#define RFM95_DIO0 36 

RH_RF95 rf95(RFM95_CS, RFM95_DIO0);

uint8_t state;
uint8_t rxbuf[RH_RF95_MAX_MESSAGE_LEN];
uint8_t txbuf[RH_RF95_MAX_MESSAGE_LEN];
uint8_t rxlen = RH_RF95_MAX_MESSAGE_LEN;
uint8_t RxSeq;

#define E0 0
#define E1 1
#define E2 2
#define E3 3

#define TYPE_DATA 1
#define TYPE_ACK 2

void setup() {
  M5.begin();
  M5.Power.begin();
  M5.Lcd.setTextSize(2);
  M5.Lcd.println("Recepteur - Test 4");

  if (!rf95.init()) {
    M5.Lcd.println("RF95 init failed");
  } else {
    M5.Lcd.println("RF95 init OK");
  }

  rf95.setFrequency(867.7);
  rf95.setModemConfig(RH_RF95::Bw125Cr45Sf128);

  state = E0;
  delay(2000);

  M5.Lcd.clear();
  M5.Lcd.setCursor(0, 0);
  M5.Lcd.println(" TEST 4: FAUX ACK SEQ ");

  RxSeq = 255;
}

void loop() {
  M5.update();

  switch (state) {
    case E0:
      rf95.setModeRx();
      state = E1;
      break;

    case E1:
      rxlen = RH_RF95_MAX_MESSAGE_LEN;
      if (rf95.recv(rxbuf, &rxlen)) {
        if (rxbuf[0] == TYPE_DATA) {
          state = E2;
        } else {
          state = E0;
        }
      }
      break;

    case E2:
      if (RxSeq != rxbuf[1]) {
        RxSeq = rxbuf[1];
        M5.Lcd.printf("\n[%d] DATA (%d octets):\n", RxSeq, rxlen);
        for (int j = 0; j < rxlen; j++) {
          M5.Lcd.printf("%02X ", rxbuf[j]);
        }
        M5.Lcd.println();
      } else {
        M5.Lcd.printf("\n[%d] DUPLICATION filtree !\n", rxbuf[1]);
      }
      state = E3;
      break;

    case E3:
      txbuf[0] = TYPE_ACK;

      //  MODIFICATION POUR LE TEST 4 
      // On envoie un ACK avec un mauvais numéro de séquence (N° reçu + 1)
      txbuf[1] = rxbuf[1] + 1; 

      M5.Lcd.printf("-> Envoi FAUX ACK [%d]...\n", txbuf[1]);
      rf95.send(txbuf, 2);
      rf95.waitPacketSent();

      state = E0;
      break;

    default:
      state = E0;
      break;
  }
}
