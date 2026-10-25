/*
 * PROGRAMME : Émetteur Multi-nœuds + MAC ALOHA (Couche 2 - Capsule 4)
 * MODIFICATIONS APPORTÉES POUR M5STACK (cf. Diapo 5/7 SAE32)[cite: 4] :
 * 1. #include <RH_RF95.h> : Remplacement de la librairie RH_RF22 (WiNo) par RH_RF95 (LoRa M5Stack).
 * 2. #include <M5Stack.h> : Remplacement de <LowPower_Teensy3.h> par la librairie M5Stack[cite: 2, 4].
 * 3. #define RFM95_CS 5 / DIO0 36 : Définition des broches SPI spécifiques au module LoRa du M5Stack[cite: 4].
 * 4. randomSeed(analogRead(35)) : Sur ESP32/M5Stack, la broche analogique en l'air utilisée est la 35 (au lieu de A3 sur Teensy).
 * 5. M5.begin() / M5.Power.begin() : Initialisation de l'écran LCD et de l'alimentation du M5Stack[cite: 4].
 * 6. rf95.setModemConfig / setFrequency(867.7) : Configuration du canal LoRa 867.7 MHz (au lieu de 433 MHz GFSK)[cite: 2, 4].
 * 7. Remplacement des Serial.print/printf par M5.Lcd.print/printf pour afficher les traces d'exécution sur l'écran[cite: 2, 4].
 * 8. Adaptation des durées (TIMEOUT_ACK, EIT, backoff) à l'échelle du temps de transmission LoRa (quelques centaines de ms).
 */

#include <SPI.h>
#include <RH_RF95.h> // MODIF M5Stack : Librairie RadioHead pour le transceiver LoRa RF95[cite: 4]
#include <M5Stack.h> // MODIF M5Stack : Librairie matérielle M5Stack (écran LCD, boutons, alimentation)[cite: 4]

#define RFM95_CS 5   // MODIF M5Stack : Broche Chip Select du module LoRa[cite: 4]
#define RFM95_DIO0 36 // MODIF M5Stack : Broche d'interruption DIO0 du module LoRa[cite: 4]
RH_RF95 rf95(RFM95_CS, RFM95_DIO0); // MODIF M5Stack : Instanciation de rf95 au lieu de rf22[cite: 2, 4]

uint8_t rxbuf[RH_RF95_MAX_MESSAGE_LEN], rxbuflen = RH_RF95_MAX_MESSAGE_LEN;
uint8_t txbuf[RH_RF95_MAX_MESSAGE_LEN], txbuflen = RH_RF95_MAX_MESSAGE_LEN;
uint8_t rxlen = RH_RF95_MAX_MESSAGE_LEN;
uint8_t state, RxSeq, TxSeq, credit, NewFrame;
uint32_t attente, backoff, EIT; // MODIF M5Stack : uint32_t pour permettre des délais en centaines de ms en LoRa

#define E0 0
#define E1 1
#define E2 2
#define E3 3
#define E4 4
#define E5 5

#define canal 1
#define TYPE_DATA 1
#define TYPE_ACK 2
#define TIMEOUT_ACK 600 // MODIF M5Stack : 600 ms (au lieu de 40 ms sur RF22) car une trame LoRa SF7 + ACK prend ~120 ms

uint8_t MyAdr = 1; // Adresse locale du noeud Tx1 ou Tx2 ou ... Tx9 (modifiable par #define ou Bouton B au lancement)

void setup()
{
  // MODIF M5Stack : Initialisation du M5Stack et de son écran LCD[cite: 4]
  M5.begin();
  M5.Power.begin();
  Serial.begin(115200);
  M5.Lcd.setTextSize(1);
  M5.Lcd.printf("=== TX ALOHA M5Stack (Adresse @S = %d) ===\n", MyAdr);
  M5.Lcd.println("Appuyez sur BtnB dans les 3s pour changer d'adresse @S");

  if (!rf95.init())
    M5.Lcd.println("RF95 init failed");
  else
    M5.Lcd.println("RF95 init OK");

  // MODIF M5Stack : Configuration du modem LoRa sur 867.7 MHz[cite: 4]
  rf95.setModemConfig(RH_RF95::Bw125Cr45Sf128); // 128=SF7[cite: 4]
  rf95.setFrequency(867.7); //[cite: 4]

  // Petit menu pendant les 3s de démarrage pour choisir l'adresse @S (1 à 9) sans recompiler !
  uint32_t t_init = millis();
  while (millis() - t_init < 3000) {
    M5.update();
    if (M5.BtnB.wasPressed()) {
      MyAdr = (MyAdr % 9) + 1;
      M5.Lcd.printf("Nouvelle adresse locale MyAdr = %d\n", MyAdr);
    }
  }

  state = E0;
  TxSeq = 0; credit = 5;
  NewFrame = 1; // La 1ère trame émise sera une nouvelle trame
  
  // MODIF M5Stack : Initialisation du générateur aléatoire sur la broche analogique 35 de l'ESP32 (+ MyAdr pour différencier les M5)
  randomSeed(analogRead(35) + MyAdr * 100); 

  M5.Lcd.printf("Boucle principale (@S = %d)\n", MyAdr);
}

void loop() {
  switch (state)
  {
    case E0:
      // Nettoyage automatique de l'écran M5Stack quand il est plein
      if (M5.Lcd.getCursorY() > 220) {
        M5.Lcd.fillScreen(BLACK);
        M5.Lcd.setCursor(0, 0);
      }

      if (NewFrame == 1) // Loi d'arrivée aléatoire du flux de trames à envoyer
      {
        EIT = random(500, 2000); // MODIF M5Stack : entre 500 et 1999 ms pour respecter le rapport cyclique LoRa 1%[cite: 4]
        delay(EIT); // La nouvelle trame à envoyer arrive après EIT ms
        M5.Lcd.printf("EIT : %d ms | ", EIT);
      }

      M5.Lcd.printf("EMISSION @%d->@0 Seq %d\n", MyAdr, TxSeq);
      txbuf[0] = MyAdr;     // @S : moi (1 à 9)
      txbuf[1] = 0;         // @D : le puits R0 (adresse 0)
      txbuf[2] = TYPE_DATA; // type de message
      txbuf[3] = TxSeq;     // numéro de séquence
      txbuf[4] = 0xAA;      // 1er octet de payload
      txbuf[5] = 0x55;      // 2ème octet de payload

      rf95.send(txbuf, 6);  // envoi de données (une trame de 6 octets)[cite: 2, 4]
      rf95.waitPacketSent(); //[cite: 2, 4]
      credit--;             // on vient d'émettre une fois de + la trame[cite: 2]
      state = E1;
      break;

    case E1:
      attente = millis() + TIMEOUT_ACK; // Armement du chien de garde (CdG)[cite: 2]
      state = E2;
      break;

    case E2:
      rf95.setModeRx(); // Activer le récepteur radio pour écouter l'ACK[cite: 2, 4]
      state = E3;
      break;

    case E3:
      if (millis() > attente) // Si le chien de garde a expiré (collision ou perte)[cite: 2]
      {
        state = E5;
      }
      else
      {
        rxlen = sizeof(rxbuf); //[cite: 4]
        if (rf95.recv(rxbuf, &rxlen)) // Si une trame est reçue[cite: 4]
        {
          // Vérification : Source = R0 (0), Destination = MyAdr, Type = TYPE_ACK, NumSeq = TxSeq[cite: 2]
          if ((rxbuf[0] == 0) && (rxbuf[1] == MyAdr) && (rxbuf[2] == TYPE_ACK) && (rxbuf[3] == TxSeq))
          {
            state = E4; // ACK valide reçu ![cite: 2]
          }
          else
          {
            state = E2; // Mauvaise trame (ex: DATA d'un autre noeud ou ACK pour un autre noeud), on retourne écouter[cite: 2]
          }
        }
      }
      break;

    case E4:
      M5.Lcd.println(" -> ACK_RECU !");
      state = E0;
      TxSeq++;       // Trame suivante[cite: 2]
      credit = 5;    // Réinitialisation du crédit[cite: 2]
      NewFrame = 1;  // La prochaine trame sera une nouvelle trame (tirage d'un nouvel EIT)[cite: 2]
      break;

    case E5: // Échec de réception de l'ACK (collision potentielle)[cite: 2]
      if (credit == 0) // Si le crédit de répétition est épuisé[cite: 2]
      {
        M5.Lcd.println(" -> ECHEC DEFINITIF (Credit epuise)");
        state = E0;
        credit = 5;
        TxSeq++;      // On abandonne cette trame et on passe à la suivante[cite: 2]
        NewFrame = 1; // Pas de backoff car c'est une nouvelle trame qui subira un EIT[cite: 2]
      }
      else // S'il reste du crédit : attente d'un délai aléatoire (Backoff ALOHA) avant réémission[cite: 2]
      {
        NewFrame = 0; // Ce n'est PAS une nouvelle trame, mais une réémission (pas d'EIT en E0)[cite: 2]
        backoff = random(200, 1500); // Tirage du délai aléatoire de backoff pour désynchroniser les concurrents[cite: 2]
        M5.Lcd.printf(" -> Collision/Timeout ! Essai %d, Backoff = %d ms\n", 5 - credit, backoff);
        delay(backoff);
        state = E0;
      }
      break;

    default:
      state = E0;
      break;
  }
}