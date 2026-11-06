/* =====================================================================
   SAE32 - Vidéo 3 - ÉMETTEUR avec détection d'erreurs - version PROPOSÉE
   Base : Vidéo 3/video-3-Test-Prog-1-A.ino (original conservé)
   Les repères [V3-x] renvoient à SAE32-Ameliorations-Code.md

   Boutons à MAINTENIR :
     A = 1 octet de DATA abîmé         -> R refuse, E réémet
     B = ACK abîmé à la réception      -> E réémet tout de suite (E5)
     C = même bit inversé dans 2 octets -> le XOR ne voit RIEN, le CRC-16 si

   Mettre CONTROLE_CRC16 à 1 ICI ET dans V3_XOR_RX pour comparer les deux codes.
   ===================================================================== */
#include <SPI.h>
#include <M5Stack.h>
#include <RH_RF95.h>

#define RFM95_CS   5
#define RFM95_DIO0 36
RH_RF95 rf95(RFM95_CS, RFM95_DIO0);

// [V3-1] Choix du contrôle à la compilation : 0 = XOR (capsule), 1 = CRC-16.
#define CONTROLE_CRC16 0

// [V3-17] true = la radio jette seule les trames abîmées. false = elles arrivent au
// programme (test 5). À régler pareil des deux côtés : avec le CRC actif, RadioHead
// refuse à la réception une trame émise sans CRC.
#define CRC_RADIO true

#if CONTROLE_CRC16
  #define TAILLE_FCS 2
#else
  #define TAILLE_FCS 1
#endif

#define FREQ_NORMALE 867.7
#define PUISSANCE_DBM 8
#define LIGNE_MAX_ECRAN 220
#define TYPE_DATA 1
#define TYPE_ACK  2
#define TAILLE_UTILE 20                        // Type + NumSeq + 18 octets
#define TAILLE_DATA  (TAILLE_UTILE + TAILLE_FCS)
#define TAILLE_ACK   3
#define TIMEOUT_ACK  500                       // À ajuster avec le RTT mesuré en V2
#define CREDIT_MAX   5
#define PAUSE_MS     1000

#define E0 0
#define E1 1
#define E2 2
#define E3 3
#define E4 4
#define E5 5

uint8_t txbuf[RH_RF95_MAX_MESSAGE_LEN];
uint8_t rxbuf[RH_RF95_MAX_MESSAGE_LEN];
uint8_t rxlen;
uint8_t state = E0, TxSeq = 0, credit = CREDIT_MAX;
uint32_t debutAttente = 0;
uint32_t nbSucces = 0, nbReemissions = 0, nbEchecs = 0;

// [V3-2] Le calcul est dans une fonction : le code de E0 reste lisible et le
// même calcul est réutilisé à l'identique dans le récepteur.
uint16_t calculerFCS(const uint8_t *data, uint8_t taille) {
#if CONTROLE_CRC16
  // CRC-16 CCITT (polynôme 0x1021, valeur de départ 0xFFFF), vu dans la capsule.
  uint16_t crc = 0xFFFF;
  for (uint8_t i = 0; i < taille; i++) {
    crc ^= (uint16_t)data[i] << 8;
    for (uint8_t bit = 0; bit < 8; bit++) {
      crc = (crc & 0x8000) ? (uint16_t)((crc << 1) ^ 0x1021) : (uint16_t)(crc << 1);
    }
  }
  return crc;
#else
  uint8_t fcs = 0;                             // 0 ^ x = x : bon point de départ
  for (uint8_t i = 0; i < taille; i++) fcs ^= data[i];
  return fcs;
#endif
}

void ecrireFCS(uint8_t *data, uint16_t fcs) {
#if CONTROLE_CRC16
  data[TAILLE_UTILE]     = fcs >> 8;           // Octet de poids fort d'abord
  data[TAILLE_UTILE + 1] = fcs & 0xFF;
#else
  data[TAILLE_UTILE] = fcs;
#endif
}

void preparerEcran() {
  if (M5.Lcd.getCursorY() > LIGNE_MAX_ECRAN) {
    M5.Lcd.fillScreen(BLACK);
    M5.Lcd.setCursor(0, 0);
  }
}

void setup() {
  M5.begin();
  M5.Power.begin();
  Serial.begin(115200);
  M5.Lcd.setTextSize(2);
#if CONTROLE_CRC16
  M5.Lcd.println("V3 - TX CRC-16");
#else
  M5.Lcd.println("V3 - TX XOR");
#endif
  M5.Lcd.setTextSize(1);
  M5.Lcd.println("A: 1 octet faux  B: ACK faux");
  M5.Lcd.println("C: 2 erreurs meme bit\n");

  if (!rf95.init()) {
    M5.Lcd.println("Init radio ECHEC");
    while (true) delay(1000);
  }
  rf95.setModemConfig(RH_RF95::Bw125Cr45Sf128);
  rf95.setFrequency(FREQ_NORMALE);
  rf95.setTxPower(PUISSANCE_DBM);
  rf95.setPayloadCRC(CRC_RADIO);   // [V3-17] Corrige le test 5 : voir ci-dessus

  delay(2000);
}

void loop() {
  M5.update();

  switch (state) {
    case E0:
      preparerEcran();
      M5.Lcd.printf("DATA %u c%u ", TxSeq, credit);

      txbuf[0] = TYPE_DATA;
      txbuf[1] = TxSeq;
      // [V3-3] Données qui changent à chaque trame. L'original envoie 18 fois 0xFF :
      // leur XOR vaut 0, donc le FCS ne dépend plus du tout du contenu utile.
      for (uint8_t i = 2; i < TAILLE_UTILE; i++) txbuf[i] = TxSeq + i;

      ecrireFCS(txbuf, calculerFCS(txbuf, TAILLE_UTILE));

      // Les erreurs sont injectées APRÈS le calcul du FCS, comme un parasite radio.
      if (M5.BtnA.isPressed()) {
        txbuf[5] ^= 0xFF;
        M5.Lcd.print("[ERR 1] ");
      }
      // [V3-4] Limite du XOR, à montrer en direct : le même bit inversé dans
      // deux octets s'annule dans le calcul.
      if (M5.BtnC.isPressed()) {
        txbuf[5] ^= 0x01;
        txbuf[9] ^= 0x01;
        M5.Lcd.print("[ERR 2] ");
      }

      for (uint8_t i = 0; i < TAILLE_DATA; i++) Serial.printf("%02X ", txbuf[i]);
      Serial.println();

      rf95.send(txbuf, TAILLE_DATA);
      rf95.waitPacketSent();
      if (credit < CREDIT_MAX) nbReemissions++;
      credit--;
      state = E1;
      break;

    case E1:
      debutAttente = millis();
      state = E2;
      break;

    case E2:
      rf95.setModeRx();
      state = E3;
      break;

    case E3:
      if (millis() - debutAttente > TIMEOUT_ACK) {
        M5.Lcd.println("CdG expire");
        state = E5;
      } else {
        rxlen = RH_RF95_MAX_MESSAGE_LEN;
        if (rf95.recv(rxbuf, &rxlen)) {
          if (M5.BtnB.isPressed()) {
            rxbuf[2]++;
            M5.Lcd.print("[ERR ACK] ");
          }

          // [V3-5] Deux noms clairs au lieu de deux gros if qui répètent les tests.
          bool monACK   = (rxlen == TAILLE_ACK) && (rxbuf[0] == TYPE_ACK) && (rxbuf[1] == TxSeq);
          bool xorJuste = ((rxbuf[0] ^ rxbuf[1]) == rxbuf[2]);

          if (monACK && xorJuste) {
            state = E4;
          } else if (monACK) {
            M5.Lcd.println("ACK corrompu");
            state = E5;          // Pas besoin d'attendre la fin du chien de garde
          } else {
            state = E2;          // Trame sans rapport : on réécoute
          }
        }
      }
      break;

    case E4:
      nbSucces++;
      M5.Lcd.println("ACK OK");
      preparerEcran();
      M5.Lcd.printf("OK%lu R%lu X%lu\n", (unsigned long)nbSucces,
                    (unsigned long)nbReemissions, (unsigned long)nbEchecs);
      TxSeq++;
      credit = CREDIT_MAX;
      delay(PAUSE_MS);           // [V3-6] 1 s ne garantit PAS 1 % de rapport cyclique
      state = E0;
      break;

    case E5:
      if (credit == 0) {
        M5.Lcd.println("ECHEC: abandon");
        nbEchecs++;
        TxSeq++;
        credit = CREDIT_MAX;
      } else {
        M5.Lcd.printf("Reessai %u\n", CREDIT_MAX - credit);  // [V3-7] Compteur unique
      }
      delay(PAUSE_MS);
      state = E0;
      break;

    default:
      state = E0;
      break;
  }
}
