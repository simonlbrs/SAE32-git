/* =====================================================================
   SAE32 - Vidéo 4 - ÉMETTEUR MAC ALOHA multi-nœuds - version PROPOSÉE
   [V4-1] Les programmes de Vidéo 4/Test 1 a 8 (Gabin) pilotent un RH_RF22.
   Ce programme reprend le protocole ALOHA pour la radio RH_RF95 du M5Stack,
   et remplace le chien de garde de 40 ms, trop court en LoRa.

   Au démarrage (3 s) : maintenir A, B ou C pour choisir l'adresse 1, 2 ou 3.
   Ensuite, en cours d'essai :
     A = EIT court (5-99 ms) <-> EIT long (500-2000 ms)   -> charge du réseau
     B = backoff : diapo -> plage large -> exponentiel    -> compare 3 réglages
     C = remet les statistiques à zéro
   ===================================================================== */
#include <SPI.h>
#include <M5Stack.h>
#include <RH_RF95.h>

#define RFM95_CS   5
#define RFM95_DIO0 36
RH_RF95 rf95(RFM95_CS, RFM95_DIO0);

#define FREQ_NORMALE 867.7
#define PUISSANCE_DBM 8

#define ADR_PUITS  0
#define TYPE_DATA  1
#define TYPE_ACK   2
#define TAILLE_DATA 6
#define TAILLE_ACK  4
#define CREDIT_MAX  5

// [V4-2] 40 ms est prévu pour le RF22. Ici, l'ACK dure à lui seul 36 ms.
// Point de départ à remplacer par le RTT mesuré avec V2_TX + une marge.
#define TIMEOUT_ACK 200

#define BACKOFF_DIAPO 0          // 0 à 99 ms : trop court pour LoRa, gardé pour comparer
#define BACKOFF_LARGE 1          // 0 à 1499 ms
#define BACKOFF_EXPO  2          // Double à chaque échec, comme en Wi-Fi

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
uint8_t MyAdr = 1;               // [V4-3] Choisie au démarrage, pas à la compilation
uint8_t NewFrame = 1;
uint8_t modeBackoff = BACKOFF_LARGE;
bool    eitLong = true;

uint32_t debutAttente = 0;

// [V4-4] Tailles élargies : en uint8_t, 1500 serait tronqué sans aucune erreur.
uint16_t EIT = 0;
uint16_t backoff = 0;

uint32_t nbEnvois = 0, nbSucces = 0, nbEchecs = 0;

void afficherTableau() {
  M5.Lcd.fillRect(0, 48, 320, 150, BLACK);
  M5.Lcd.setCursor(0, 48);
  float essais = nbSucces ? (float)nbEnvois / nbSucces : 0;
  M5.Lcd.printf("Envois  : %lu\n", (unsigned long)nbEnvois);
  M5.Lcd.printf("Succes  : %lu\n", (unsigned long)nbSucces);
  M5.Lcd.printf("Echecs  : %lu\n", (unsigned long)nbEchecs);
  M5.Lcd.printf("Env/OK  : %.2f\n", essais);
  M5.Lcd.printf("EIT %s\n", eitLong ? "500-2000" : "5-99");
  M5.Lcd.printf("Backoff %s\n", modeBackoff == BACKOFF_DIAPO ? "0-99"
                              : modeBackoff == BACKOFF_LARGE ? "0-1499" : "expo");
}

void gererBoutons() {
  if (M5.BtnA.wasPressed()) {
    eitLong = !eitLong;
    afficherTableau();
  }
  if (M5.BtnB.wasPressed()) {
    modeBackoff = (modeBackoff + 1) % 3;
    afficherTableau();
  }
  if (M5.BtnC.wasPressed()) {
    nbEnvois = 0;
    nbSucces = 0;
    nbEchecs = 0;
    afficherTableau();
  }
}

// [V4-5] Attente qui laisse les boutons actifs, contrairement à delay().
void attendre(uint32_t duree) {
  uint32_t debut = millis();
  while (millis() - debut < duree) {
    M5.update();
    gererBoutons();
    delay(5);
  }
}

uint16_t tirerBackoff() {
  if (modeBackoff == BACKOFF_DIAPO) return random(0, 100);
  if (modeBackoff == BACKOFF_LARGE) return random(0, 1500);

  // [V4-6] Plage de 400 ms après le 1er échec, puis 800, 1600, 3200 ms.
  uint8_t echecs = CREDIT_MAX - credit;        // Vaut au moins 1 ici
  uint32_t plage = 200UL << echecs;
  return random(0, plage);
}

void choisirAdresse() {
  M5.Lcd.println("A=1 B=2 C=3");
  uint32_t debut = millis();
  while (millis() - debut < 3000) {
    M5.update();
    if (M5.BtnA.isPressed()) MyAdr = 1;
    if (M5.BtnB.isPressed()) MyAdr = 2;
    if (M5.BtnC.isPressed()) MyAdr = 3;
    delay(10);
  }
}

void setup() {
  M5.begin();
  M5.Power.begin();
  M5.Lcd.setTextSize(2);
  M5.Lcd.println("V4 - TX ALOHA");

  choisirAdresse();
  M5.Lcd.clear(BLACK);
  M5.Lcd.setCursor(0, 0);
  M5.Lcd.printf("V4 TX - Adresse %u\n", MyAdr);

  if (!rf95.init()) {
    M5.Lcd.println("Init radio ECHEC");
    while (true) delay(1000);
  }
  rf95.setModemConfig(RH_RF95::Bw125Cr45Sf128);
  rf95.setFrequency(FREQ_NORMALE);
  rf95.setTxPower(PUISSANCE_DBM);

  // [V4-7] Graine propre à la carte : l'adresse MAC de l'ESP32 est unique, et
  // esp_random() vient du générateur matériel. A3 n'a pas de sens sur M5Stack.
  randomSeed(esp_random() ^ (uint32_t)(ESP.getEfuseMac() >> 24));

  // Contrôle visuel du piège 4 : deux nœuds doivent afficher des valeurs différentes.
  M5.Lcd.printf("Tirages: %ld %ld\n", random(0, 1000), random(0, 1000));
  afficherTableau();
}

void loop() {
  M5.update();
  gererBoutons();

  switch (state) {
    case E0:
      if (NewFrame == 1) {       // EIT seulement pour une NOUVELLE donnée
        EIT = eitLong ? random(500, 2000) : random(5, 100);
        attendre(EIT);
      }

      txbuf[0] = MyAdr;          // @S : moi
      txbuf[1] = ADR_PUITS;      // @D : le puits
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
        state = E5;
      } else {
        rxlen = RH_RF95_MAX_MESSAGE_LEN;          // [V4-8] Piège 8 de la fiche
        if (rf95.recv(rxbuf, &rxlen)) {
          // Trois conditions : c'est un ACK ET c'est le mien ET il est pour moi.
          if ((rxlen == TAILLE_ACK) && (rxbuf[2] == TYPE_ACK) &&
              (rxbuf[3] == TxSeq) && (rxbuf[1] == MyAdr)) {
            state = E4;
          } else {
            state = E2;          // Trame d'un autre nœud : on continue d'écouter
          }
        }
      }
      break;

    case E4:
      nbSucces++;
      TxSeq++;
      credit = CREDIT_MAX;
      NewFrame = 1;              // [V4-9] Oubli de la diapo : sans cette ligne, plus d'EIT
      afficherTableau();
      state = E0;
      break;

    case E5:
      if (credit == 0) {         // 5 envois sans ACK : abandon, sans backoff
        nbEchecs++;
        TxSeq++;
        credit = CREDIT_MAX;
        NewFrame = 1;
        afficherTableau();
      } else {
        NewFrame = 0;            // Même trame : pas d'EIT
        backoff = tirerBackoff();
        attendre(backoff);       // "Collision ?" : on ne peut pas en être sûr
      }
      state = E0;
      break;

    default:
      state = E0;
      break;
  }
}
