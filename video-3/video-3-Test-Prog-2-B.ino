#include <SPI.h>
#include <RH_RF95.h>
#include <M5Stack.h>

#define RFM95_CS 5
#define RFM95_DIO0 36
RH_RF95 rf95(RFM95_CS, RFM95_DIO0);

uint8_t rxbuf[RH_RF95_MAX_MESSAGE_LEN];
uint8_t rxlen = RH_RF95_MAX_MESSAGE_LEN;

void setup() {
  M5.begin();
  M5.Power.begin();
  Serial.begin(115200);

  M5.Lcd.setTextSize(2);
  M5.Lcd.println("== RX Reed-Solomon ==");
  M5.Lcd.setTextSize(1);

  if (!rf95.init()) {
    M5.Lcd.println("Init RF95 failed !");
    while (1);
  }
  M5.Lcd.println("Init RF95 OK - Pret a corriger...\n");

  rf95.setModemConfig(RH_RF95::Bw125Cr45Sf128);
  rf95.setFrequency(867.7);
  rf95.setModeRx();
}

void loop() {
  rxlen = RH_RF95_MAX_MESSAGE_LEN;
  if (rf95.recv(rxbuf, &rxlen)) {
    if (rxlen == 24) {
      if (M5.Lcd.getCursorY() > 200) {
        M5.Lcd.fillScreen(BLACK);
        M5.Lcd.setCursor(0, 0);
      }

      // 1. Extraction de S_reçu et SP_reçu (16 bits non signés)
      uint16_t S_recu  = ((uint16_t)rxbuf[20] << 8) | rxbuf[21];
      uint16_t SP_recu = ((uint16_t)rxbuf[22] << 8) | rxbuf[23];

      // 2. Calcul de S_calculé et SP_calculé sur les 20 octets reçus
      uint16_t S_calc = 0;
      uint16_t SP_calc = 0;
      for (int i = 0; i < 20; i++) {
        S_calc  += rxbuf[i];
        SP_calc += rxbuf[i] * (i + 1); // Rang i+1 (de 1 à 20)
      }

      // 3. Calcul de l'erreur E et détection (utilisation d'entiers signés !)
      int32_t E = (int32_t)S_calc - (int32_t)S_recu;
      int32_t diff_SP = (int32_t)SP_calc - (int32_t)SP_recu;

      if (E == 0 && diff_SP == 0) {
        M5.Lcd.printf("[OK] Aucune erreur (S=%d, SP=%d)\n", S_calc, SP_calc);
      } 
      else if (E != 0 && (diff_SP % E == 0)) {
        // Calcul du rang R de l'erreur : R = (SP_calculé - SP_reçu) / E
        int32_t R = diff_SP / E;

        if (R >= 1 && R <= 20) {
          uint8_t octet_faux = rxbuf[R - 1];
          // Correction de l'octet : O_corrigé = O_reçu - E
          rxbuf[R - 1] = (uint8_t)((int32_t)rxbuf[R - 1] - E);
          
          M5.Lcd.printf("[CORRECTION] Erreur E=%d au rang R=%d\n", E, R);
          M5.Lcd.printf(" -> Octet corrige : %d remplace par %d !\n", octet_faux, rxbuf[R - 1]);
        } else {
          M5.Lcd.println("[INFO] Erreur sur les champs de controle (Data intacte)");
        }
      } 
      else {
        M5.Lcd.println("[INFO] Erreur sur les champs de controle S/SP ou erreurs multiples");
      }
    }
    rf95.setModeRx();
  }
}