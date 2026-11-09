#include <SPI.h>
#include <M5Stack.h>
#include <RH_RF95.h>

#define RFM95_CS 5   
#define RFM95_DIO0 36 

RH_RF95 rf95(RFM95_CS, RFM95_DIO0);

uint8_t rxbuf[RH_RF95_MAX_MESSAGE_LEN];
uint8_t txbuf[RH_RF95_MAX_MESSAGE_LEN];
uint8_t rxlen = RH_RF95_MAX_MESSAGE_LEN;
uint8_t state, TxSeq, credit;
uint32_t attente;

#define E0 0
#define E1 1
#define E2 2
#define E3 3
#define E4 4
#define E5 5

#define TYPE_DATA 1
#define TYPE_ACK 2
#define TIMEOUT_ACK 1500

void setup() {
  M5.begin();
  M5.Power.begin();
  M5.Lcd.setTextSize(2);
  M5.Lcd.println("Emetteur - Test 9");

  if (!rf95.init()) {
    M5.Lcd.println("RF95 init failed");
  } else {
    M5.Lcd.println("RF95 init OK");
  }

  rf95.setFrequency(867.7);
  rf95.setModemConfig(RH_RF95::Bw125Cr45Sf128);

  state = E0;
  delay(2000);

  TxSeq = 0;
  credit = 5;

  M5.Lcd.clear();
  M5.Lcd.setCursor(0, 0);
  M5.Lcd.println("--- TEST 9: TRAMES LONGUES ---");
}

void loop() {
  M5.update();

  switch (state) {
    case E0:
      M5.Lcd.printf("\nEMISSION [%d] (Cred:%d): ", TxSeq, credit);

      // --- MODIFICATION POUR LE TEST 9 ---
      txbuf[0] = TYPE_DATA; // 1 octet de Type
      txbuf[1] = TxSeq;     // 1 octet de N° Séquence
      
      // 5 octets de charge utile (PLOAD) au lieu de 2
      txbuf[2] = 0xAA;
      txbuf[3] = 0x55;
      txbuf[4] = 0x11;
      txbuf[5] = 0x22;
      txbuf[6] = 0x33;

      // On indique qu'on envoie 7 octets au total
      rf95.send(txbuf, 7);
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
        rxlen = RH_RF95_MAX_MESSAGE_LEN;
        if (rf95.recv(rxbuf, &rxlen)) {
          if ((rxbuf[0] == TYPE_ACK) && (rxbuf[1] == TxSeq)) {
            state = E4;
          } else {
            state = E2;
          }
        }
      }
      break;

    case E4:
      M5.Lcd.println("ACK RECU !");
      TxSeq++;
      credit = 5;
      state = E0;
      delay(1000);
      break;

    case E5:
      M5.Lcd.println("TIMEOUT !");
      if (credit == 0) {
        M5.Lcd.println("ECHEC : Credit epuise !");
        TxSeq++;
        credit = 5;
        state = E0;
      } else {
        M5.Lcd.printf("Reessai n°%d...\n", 5 - credit);
        state = E0;
      }
      delay(1000);
      break;

    default:
      state = E0;
      break;
  }
}
