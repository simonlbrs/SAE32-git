/* =====================================================================
   SAE32 - Vidéo 3 - RÉCEPTEUR avec détection d'erreurs - version PROPOSÉE
   [V3-8] Base : "videos .docx" (Programme 1B) et Vidéo 3/Test 4/Emetteur_Tx.ino,
   qui est en réalité un récepteur XOR. Attention : video-3-Test-Prog-1-A et
   -1-B sont le même émetteur, ils ne peuvent pas dialoguer ensemble.

   CONTROLE_CRC16 doit avoir la même valeur que dans V3_XOR_TX.
   ===================================================================== */
#include <SPI.h>
#include <M5Stack.h>
#include <RH_RF95.h>

#define RFM95_CS   5
#define RFM95_DIO0 36
RH_RF95 rf95(RFM95_CS, RFM95_DIO0);

#define CONTROLE_CRC16 0
#define CRC_RADIO true           // [V3-17] Même valeur que dans V3_XOR_TX

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
#define TAILLE_UTILE 20
#define TAILLE_DATA  (TAILLE_UTILE + TAILLE_FCS)
#define TAILLE_ACK   3

#define E0 0
#define E1 1
#define E2 2
#define E3 3

uint8_t txbuf[RH_RF95_MAX_MESSAGE_LEN];
uint8_t rxbuf[RH_RF95_MAX_MESSAGE_LEN];
uint8_t rxlen;
uint8_t state = E0, RxSeq = 0;
bool premiereTrame = true;       // [V2-12] Même correction qu'en vidéo 2

uint32_t nbJustes = 0, nbRejets = 0, nbDoublons = 0;
uint32_t nbFauxAcceptes = 0;     // [V3-9] Trames fausses que le code n'a pas vues

uint16_t calculerFCS(const uint8_t *data, uint8_t taille) {
#if CONTROLE_CRC16
  uint16_t crc = 0xFFFF;
  for (uint8_t i = 0; i < taille; i++) {
    crc ^= (uint16_t)data[i] << 8;
    for (uint8_t bit = 0; bit < 8; bit++) {
      crc = (crc & 0x8000) ? (uint16_t)((crc << 1) ^ 0x1021) : (uint16_t)(crc << 1);
    }
  }
  return crc;
#else
  uint8_t fcs = 0;
  for (uint8_t i = 0; i < taille; i++) fcs ^= data[i];
  return fcs;
#endif
}

uint16_t lireFCS(const uint8_t *data) {
#if CONTROLE_CRC16
  return ((uint16_t)data[TAILLE_UTILE] << 8) | data[TAILLE_UTILE + 1];
#else
  return data[TAILLE_UTILE];
#endif
}

// [V3-9] Le récepteur connaît ici la règle de remplissage (NumSeq + rang).
// Cela sert uniquement aux essais : il peut prouver qu'un code s'est trompé.
bool contenuConforme() {
  for (uint8_t i = 2; i < TAILLE_UTILE; i++) {
    if (rxbuf[i] != (uint8_t)(rxbuf[1] + i)) return false;
  }
  return true;
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
  M5.Lcd.println("V3 - RX CRC-16");
#else
  M5.Lcd.println("V3 - RX XOR");
#endif

  if (!rf95.init()) {
    M5.Lcd.println("Init radio ECHEC");
    while (true) delay(1000);
  }
  rf95.setModemConfig(RH_RF95::Bw125Cr45Sf128);
  rf95.setFrequency(FREQ_NORMALE);
  rf95.setTxPower(PUISSANCE_DBM);
  rf95.setPayloadCRC(CRC_RADIO);   // [V3-17] Corrige le test 5
}

void loop() {
  switch (state) {
    case E0:
      rf95.setModeRx();
      state = E1;
      break;

    case E1:
      rxlen = RH_RF95_MAX_MESSAGE_LEN;
      if (rf95.recv(rxbuf, &rxlen)) {
        preparerEcran();
        if ((rxlen == TAILLE_DATA) && (rxbuf[0] == TYPE_DATA)) {
          uint16_t recu = lireFCS(rxbuf);
          uint16_t calcule = calculerFCS(rxbuf, TAILLE_UTILE);
          if (recu == calcule) {
            state = E2;
          } else {
            nbRejets++;
            // Silence : pas d'ACK ni de NACK, E réémettra à l'expiration du CdG.
            M5.Lcd.printf("REJET %04X!=%04X\n", recu, calcule);
            state = E0;
          }
        } else {
          nbRejets++;            // Mauvais type ou mauvaise longueur
          state = E0;
        }
      }
      break;

    case E2:
      if (premiereTrame || (RxSeq != rxbuf[1])) {
        premiereTrame = false;
        RxSeq = rxbuf[1];
        if (contenuConforme()) {
          nbJustes++;
          M5.Lcd.printf("DATA %u OK\n", RxSeq);
        } else {
          nbFauxAcceptes++;
          M5.Lcd.setTextColor(RED, BLACK);
          M5.Lcd.printf("DATA %u FAUX ACCEPTE\n", RxSeq);
          M5.Lcd.setTextColor(WHITE, BLACK);
        }
      } else {
        nbDoublons++;
        M5.Lcd.printf("DOUBLON %u\n", RxSeq);
      }
      preparerEcran();
      M5.Lcd.printf("J%lu Rej%lu D%lu F%lu\n", (unsigned long)nbJustes,
                    (unsigned long)nbRejets, (unsigned long)nbDoublons,
                    (unsigned long)nbFauxAcceptes);
      state = E3;
      break;

    case E3:
      txbuf[0] = TYPE_ACK;
      txbuf[1] = rxbuf[1];
      txbuf[2] = txbuf[0] ^ txbuf[1];  // L'ACK est protégé lui aussi
      rf95.send(txbuf, TAILLE_ACK);
      rf95.waitPacketSent();
      state = E0;
      break;

    default:
      state = E0;
      break;
  }
}
