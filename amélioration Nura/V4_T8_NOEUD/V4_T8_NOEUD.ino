/* =====================================================================
   SAE32 - Vidéo 4 - TEST 8 : NOEUD ÉMETTEUR + RÉCEPTEUR - portage RH_RF95
   Base : Vidéo 4/Test 8/test Noeud Emetteur Recepteur.ino (Gabin)
   Sans donnée à émettre, le nœud reste en réception et acquitte (état E6).
   Les repères [P-x] renvoient à SAE32-Ameliorations-Code.md

   Au démarrage (3 s) : maintenir A pour le nœud 1 (vers 2), B pour le nœud 2
   (vers 1), C pour le nœud 3 (vers le puits 0).
   ===================================================================== */
#include <SPI.h>
#include <M5Stack.h>
#include <RH_RF95.h>

#define RFM95_CS   5
#define RFM95_DIO0 36
RH_RF95 rf95(RFM95_CS, RFM95_DIO0);

uint8_t rxbuf[RH_RF95_MAX_MESSAGE_LEN];
uint8_t txbuf[RH_RF95_MAX_MESSAGE_LEN];
uint8_t rxlen = RH_RF95_MAX_MESSAGE_LEN;
uint8_t state, TxSeq, credit, NewFrame;
uint16_t backoff, EIT;
uint8_t RxSeqSrc[16];               // dernier numéro reçu par émetteur
bool    dejaRecu[16];               // [P-14] Une première trame n°255 reste une trame neuve
uint32_t debutAttente, debutPause;
uint16_t dureePause;

#define E0 0
#define E1 1
#define E2 2
#define E3 3
#define E4 4
#define E5 5
#define E6 6                        // attente en réception, à la place de delay()
#define TYPE_DATA 1
#define TYPE_ACK 2
#define TAILLE_DATA 6
#define TAILLE_ACK 4
#define CREDIT_MAX 5
#define TIMEOUT_ACK 200
#define BACKOFF_MAX 1500
#define DELAI_RETOURNEMENT 5        // ms, à ajuster sur le matériel

// [P-15] Un nœud émet et reçoit : avec 5-99 ms, il n'écouterait presque jamais.
#define EIT_MIN 500
#define EIT_MAX 2000

uint8_t MyAdr = 1;
uint8_t DestAdr = 2;

uint32_t nbEnvois = 0, nbSucces = 0, nbEchecs = 0, nbRecues = 0, nbDoublons = 0;

void afficherTableau() {
  M5.Lcd.fillRect(0, 32, 320, 120, BLACK);
  M5.Lcd.setCursor(0, 32);
  M5.Lcd.printf("Envois : %lu\n", (unsigned long)nbEnvois);
  M5.Lcd.printf("Succes : %lu\n", (unsigned long)nbSucces);
  M5.Lcd.printf("Echecs : %lu\n", (unsigned long)nbEchecs);
  M5.Lcd.printf("Recues : %lu\n", (unsigned long)nbRecues);
  M5.Lcd.printf("Doublons: %lu\n", (unsigned long)nbDoublons);
}

void choisirAdresse() {
  M5.Lcd.println("A:1>2 B:2>1 C:3>0");
  uint32_t debut = millis();
  while (millis() - debut < 3000) {
    M5.update();
    if (M5.BtnA.isPressed()) { MyAdr = 1; DestAdr = 2; }
    if (M5.BtnB.isPressed()) { MyAdr = 2; DestAdr = 1; }
    if (M5.BtnC.isPressed()) { MyAdr = 3; DestAdr = 0; }
    delay(10);
  }
}

// Pause pendant laquelle le nœud reste capable de recevoir et d'acquitter.
void lancerPause(uint16_t duree) {
  debutPause = millis();
  dureePause = duree;
  rf95.setModeRx();
  state = E6;
}

void setup()
{
  M5.begin();
  M5.Power.begin();
  M5.Lcd.setTextSize(2);
  choisirAdresse();
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setCursor(0, 0);
  M5.Lcd.printf("V4 noeud %u vers %u\n", MyAdr, DestAdr);

  if (!rf95.init()) {
    M5.Lcd.println("Init radio ECHEC");
    while (true) delay(1000);
  }
  rf95.setModemConfig(RH_RF95::Bw125Cr45Sf128);
  rf95.setFrequency(867.7);
  rf95.setTxPower(8);

  state = E0;
  TxSeq = 0; credit = CREDIT_MAX;
  for (uint8_t i = 0; i < 16; i++) dejaRecu[i] = false;
  NewFrame = 1;
  randomSeed(esp_random() ^ (uint32_t)(ESP.getEfuseMac() >> 24));
  afficherTableau();
}

void loop() {
switch (state)
{
  case E0:
    if (NewFrame == 1)      // nouvelle donnée : elle arrivera après l'EIT
    {
      EIT = random(EIT_MIN, EIT_MAX);
      Serial.printf("EIT : %u\n", EIT);
      NewFrame = 0;
      lancerPause(EIT);
      break;
    }
    txbuf[0] = MyAdr;
    txbuf[1] = DestAdr;
    txbuf[2] = TYPE_DATA;
    txbuf[3] = TxSeq;
    txbuf[4] = 0xAA;
    txbuf[5] = 0x55;
    rf95.send(txbuf, TAILLE_DATA);
    rf95.waitPacketSent();
    nbEnvois++;
    credit--;
    state = E1;
    break;

  case E6:                  // réception tant qu'il n'y a rien à émettre
    if (millis() - debutPause >= dureePause) { state = E0; break; }
    rxlen = RH_RF95_MAX_MESSAGE_LEN;
    if (rf95.recv(rxbuf, &rxlen))
    {
      if ((rxlen == TAILLE_DATA) && (rxbuf[2] == TYPE_DATA) && (rxbuf[1] == MyAdr) && (rxbuf[0] < 16))
      {
        uint8_t src = rxbuf[0];
        if (!dejaRecu[src] || (RxSeqSrc[src] != rxbuf[3]))
        {
          dejaRecu[src] = true;
          RxSeqSrc[src] = rxbuf[3];
          nbRecues++;
        }
        else nbDoublons++;

        // [P-21] L'autre nœud vient de finir d'émettre : on le laisse repasser en écoute.
        delay(DELAI_RETOURNEMENT);
        txbuf[0] = MyAdr;    // ACK d'abord, écran ensuite
        txbuf[1] = rxbuf[0];
        txbuf[2] = TYPE_ACK;
        txbuf[3] = rxbuf[3];
        rf95.send(txbuf, TAILLE_ACK);
        rf95.waitPacketSent();
        afficherTableau();
      }
      rf95.setModeRx();
    }
    break;

  case E1:
    debutAttente = millis();
    rf95.setModeRx();
    state = E3;
    break;

  case E2:
    rf95.setModeRx();
    state = E3;
    break;

  case E3:
    // Limite à citer à l'oral : pendant cette attente, une DATA reçue n'est pas
    // acquittée. Son émetteur réémettra plus tard.
    if (millis() - debutAttente > TIMEOUT_ACK)
    { state = E5; }
    else
    {
      rxlen = RH_RF95_MAX_MESSAGE_LEN;
      if (rf95.recv(rxbuf, &rxlen))
      {
        if ((rxlen == TAILLE_ACK) && (rxbuf[2] == TYPE_ACK) && (rxbuf[3] == TxSeq) &&
            (rxbuf[1] == MyAdr) && (rxbuf[0] == DestAdr))
        { state = E4; }
        else state = E2;
      }
    }
    break;

  case E4:
    nbSucces++;
    state = E0; TxSeq++; credit = CREDIT_MAX;
    NewFrame = 1;
    afficherTableau();
    break;

  case E5:
    if (credit == 0)
    {
      nbEchecs++;
      state = E0; NewFrame = 1;
      credit = CREDIT_MAX; TxSeq++;
      afficherTableau();
    }
    else
    {
      // [P-16] delay() rendrait le nœud sourd : il n'acquitterait plus personne
      // pendant son backoff. On attend donc dans E6, radio en réception.
      backoff = random(0, BACKOFF_MAX);
      lancerPause(backoff);
    }
    break;

  default:
    state = E0;
    break;
}
}
