/* =====================================================================
   SAE32 - Vidéo 4 - ÉMETTEUR ALOHA - portage M5Stack LoRa (RH_RF95)
   Base : Vidéo 4/Test 7/test MultiRecepteurs Emetteur.ino (Gabin)
   Test 7 : plusieurs récepteurs. Lancer V4_RX avec l'adresse 0, 10 et 11.
   Le protocole et les états de Gabin sont conservés.
   Les repères [P-x] renvoient à SAE32-Ameliorations-Code.md

   Au démarrage (3 s) : maintenir A, B ou C pour choisir l'adresse 1, 2 ou 3.
   ===================================================================== */
#include <SPI.h>
#include <M5Stack.h>
#include <RH_RF95.h>                // [P-1] Radio du module LoRa, à la place de RH_RF22

// [P-2] Broches du module LoRa M5Stack (celles des diapos), plus de câblage RF22.
#define RFM95_CS   5
#define RFM95_DIO0 36
RH_RF95 rf95(RFM95_CS, RFM95_DIO0);

uint8_t rxbuf[RH_RF95_MAX_MESSAGE_LEN];
uint8_t txbuf[RH_RF95_MAX_MESSAGE_LEN];
uint8_t rxlen = RH_RF95_MAX_MESSAGE_LEN;
uint8_t state, credit, NewFrame;
#define NB_RX 3
const uint8_t RX_ADR[NB_RX] = {0, 10, 11};   // adresses des récepteurs
uint8_t TxSeq[NB_RX];                        // un NumSeq d'émission par récepteur
uint8_t dest;                                // indice du récepteur visé
uint16_t backoff, EIT;              // [P-3] 16 bits : les délais LoRa dépassent 255 ms
uint32_t debutAttente;              // [P-4] Instant de départ du chien de garde

#define E0 0
#define E1 1
#define E2 2
#define E3 3
#define E4 4
#define E5 5
#define TYPE_DATA 1
#define TYPE_ACK 2
#define TAILLE_DATA 6
#define TAILLE_ACK 4
#define CREDIT_MAX 5

// [P-5] 40 ms était prévu pour le RF22. En LoRa SF7, l'ACK dure déjà 36 ms.
// Valeur de départ, à remplacer par le RTT max mesuré + une marge.
#define TIMEOUT_ACK 200

#define EIT_MIN 500
#define EIT_MAX 2000

// [P-6] Doit dépasser largement DATA + ACK (environ 77 ms en SF7).
#define BACKOFF_MAX 1500

uint8_t MyAdr = 1;                  // [P-7] Choisie au démarrage, pas à la compilation

uint32_t nbEnvois = 0, nbSucces = 0, nbEchecs = 0;
uint32_t rttMax = 0;

// [P-8] Sur M5Stack, le résultat doit être lisible sans ordinateur.
void afficherTableau() {
  M5.Lcd.fillRect(0, 32, 320, 120, BLACK);
  M5.Lcd.setCursor(0, 32);
  M5.Lcd.printf("Envois : %lu\n", (unsigned long)nbEnvois);
  M5.Lcd.printf("Succes : %lu\n", (unsigned long)nbSucces);
  M5.Lcd.printf("Echecs : %lu\n", (unsigned long)nbEchecs);
  M5.Lcd.printf("RTT max: %lu ms\n", (unsigned long)rttMax);
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

void setup()
{
  M5.begin();                       // [P-9] Démarre aussi Serial à 115200 et l'écran
  M5.Power.begin();
  M5.Lcd.setTextSize(2);
  choisirAdresse();
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setCursor(0, 0);
  M5.Lcd.printf("V4 TX adresse %u\n", MyAdr);

  if (!rf95.init()) {               // [P-10] Arrêt clair : sinon l'écran semble figé
    M5.Lcd.println("Init radio ECHEC");
    while (true) delay(1000);
  }
  rf95.setModemConfig(RH_RF95::Bw125Cr45Sf128);   // [P-11] LoRa SF7 à la place de GFSK
  rf95.setFrequency(867.7);                       // Fréquence des diapos, pas 433 MHz
  rf95.setTxPower(8);

  state = E0;
  for (dest = 0; dest < NB_RX; dest++) TxSeq[dest] = 0;
  dest = 0; credit = CREDIT_MAX;
  NewFrame = 1;

  // [P-12] esp_random() de Gabin, plus l'adresse MAC propre à chaque carte.
  randomSeed(esp_random() ^ (uint32_t)(ESP.getEfuseMac() >> 24));
  afficherTableau();
}

void loop() {
switch (state)
{
  case E0:
    if (NewFrame == 1)      // loi d'arrivée aléatoire du flux de trames à envoyer
    {
      EIT = random(EIT_MIN, EIT_MAX);
      dest = random(NB_RX);   // récepteur choisi pour cette nouvelle trame
      delay(EIT);
      Serial.printf("EIT : %u\n", EIT);
    }
    Serial.printf("EMISSION vers R%u seq %u : ", RX_ADR[dest], TxSeq[dest]);
    txbuf[0] = MyAdr;       // @S : moi
    txbuf[1] = RX_ADR[dest]; // @D : le récepteur choisi
    txbuf[2] = TYPE_DATA;
    txbuf[3] = TxSeq[dest];
    txbuf[4] = 0xAA;
    txbuf[5] = 0x55;
    rf95.send(txbuf, TAILLE_DATA);
    rf95.waitPacketSent();
    nbEnvois++;
    credit--;               // on vient d'émettre une fois de + la trame
    state = E1;
    break;

  case E1:
    debutAttente = millis();
    rf95.setModeRx();
    state = E3;
    break;

  case E2:                  // trame reçue mais pas mon ACK : on continue d'attendre
    rf95.setModeRx();
    state = E3;
    break;

  case E3:                  // attente d'ACK non bloquante
    if (millis() - debutAttente > TIMEOUT_ACK)  // [P-4] Reste juste au retour à 0 de millis()
    { state = E5; }
    else
    {
      rxlen = RH_RF95_MAX_MESSAGE_LEN;
      if (rf95.recv(rxbuf, &rxlen))
      {
        // [P-13] Contrôle de longueur ajouté.
        if ((rxlen == TAILLE_ACK) && (rxbuf[2] == TYPE_ACK) && (rxbuf[3] == TxSeq[dest]) && (rxbuf[1] == MyAdr) && (rxbuf[0] == RX_ADR[dest]))
        { state = E4; }
        else state = E2;
      }
    }
    break;

  case E4: {
    uint32_t rtt = millis() - debutAttente;
    if (rtt > rttMax) rttMax = rtt;
    nbSucces++;
    Serial.printf("ACK_RECU %lu ms\n", (unsigned long)rtt);
    state = E0; TxSeq[dest]++; credit = CREDIT_MAX;
    NewFrame = 1;                    // Correction de Gabin : sinon plus d'EIT
    afficherTableau();
    break;
  }

  case E5:      // le chien de garde expire sans ACK
    if (credit == 0)
    {
      Serial.println("ECHEC");
      nbEchecs++;
      state = E0; NewFrame = 1;     // nouvelle trame
      credit = CREDIT_MAX; TxSeq[dest]++;
      afficherTableau();
    }
    else
    {
      state = E0; NewFrame = 0;     // toujours la même trame : pas d'EIT
      backoff = random(0, BACKOFF_MAX);
      Serial.printf("Collision ? Tentative %u, backoff %u\n", CREDIT_MAX - credit, backoff);
      delay(backoff);               // attente aléatoire ALOHA
    }
    break;

  default:
    state = E0;
    break;
}
}
