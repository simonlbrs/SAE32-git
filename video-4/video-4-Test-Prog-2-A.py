/*
 * PROGRAMME : Récepteur Puits R0 Multi-nœuds (Couche 2 - Capsule 4)
 * MODIFICATIONS APPORTÉES POUR M5STACK (cf. Diapo 5/7 SAE32)[cite: 4] :
 * 1. #include <RH_RF95.h> et #include <M5Stack.h> : Adaptation pour l'environnement M5Stack LoRa[cite: 4].
 * 2. #define RFM95_CS 5 / DIO0 36 : Broches SPI du module LoRa M5Stack[cite: 4].
 * 3. M5.begin() / M5.Power.begin() : Initialisation de l'écran et de l'alimentation[cite: 4].
 * 4. rf95.setModemConfig / setFrequency(867.7) : Configuration radio LoRa[cite: 4].
 * 5. Tableau RxSeq[10] : Gestion d'un numéro de séquence par émetteur (adresses 1 à 9) pour détecter les duplications[cite: 2].
 * 6. Remplacement de Serial par M5.Lcd pour afficher les trames de tous les émetteurs[cite: 4].
 */

#include <SPI.h>
#include <RH_RF95.h> // MODIF M5Stack : Librairie RadioHead RF95[cite: 4]
#include <M5Stack.h> // MODIF M5Stack : Librairie M5Stack[cite: 4]

#define RFM95_CS 5   // MODIF M5Stack : Broche CS LoRa[cite: 4]
#define RFM95_DIO0 36 // MODIF M5Stack : Broche DIO0 LoRa[cite: 4]
RH_RF95 rf95(RFM95_CS, RFM95_DIO0); // MODIF M5Stack : Objet rf95[cite: 4]

uint8_t state;
uint8_t rxbuf[RH_RF95_MAX_MESSAGE_LEN], rxbuflen = RH_RF95_MAX_MESSAGE_LEN;
uint8_t txbuf[RH_RF95_MAX_MESSAGE_LEN], txbuflen = RH_RF95_MAX_MESSAGE_LEN;
uint8_t rxlen = RH_RF95_MAX_MESSAGE_LEN;

// Tableau mémorisant le dernier NumSeq reçu pour chacun des 9 émetteurs (indices 1 à 9)
uint8_t RxSeq[10]; 
int i, j;

#define E0 0
#define E1 1
#define E2 2
#define E3 3

#define canal 1
#define TYPE_DATA 1
#define TYPE_ACK 2
#define MyAdr 0 // Adresse locale du puits R0 = 0[cite: 2]

void setup() {
  // MODIF M5Stack : Initialisation du M5Stack[cite: 4]
  M5.begin();
  M5.Power.begin();
  Serial.begin(115200);
  M5.Lcd.setTextSize(1);
  M5.Lcd.println("=== RX PUITS R0 (@0) - MAC ALOHA M5Stack ===");

  delay(1000); //[cite: 4]

  if (!rf95.init())
    M5.Lcd.println("init M5 failed");
  else
    M5.Lcd.println("init M5 OK");

  // MODIF M5Stack : Configuration de la radio LoRa[cite: 4]
  rf95.setModemConfig(RH_RF95::Bw125Cr45Sf128); // 128=SF7[cite: 4]
  rf95.setFrequency(867.7); //[cite: 4]

  // Initialisation des numéros de séquence reçus à 255 pour tous les émetteurs
  for (i = 0; i < 10; i++) {
    RxSeq[i] = 255;
  }

  state = E0;
  delay(2000);
  M5.Lcd.println("En ecoute des emetteurs E1..E9...\n");
}

void loop() {
  switch (state) {
    case E0: // Mettre en réception des données
      rf95.setModeRx(); //[cite: 4]
      state = E1;
      break;

    case E1: // Vérifier si un message reçu est disponible et nous est destiné
      rxlen = sizeof(rxbuf); //[cite: 4]
      if (rf95.recv(rxbuf, &rxlen)) { //[cite: 4]
        // Vérification : Adresse destination @D == MyAdr (0) ET Type == TYPE_DATA[cite: 2]
        if ((rxbuf[1] == MyAdr) && (rxbuf[2] == TYPE_DATA)) {
          state = E2;
        } else {
          state = E0; // Trame ignorée si elle n'est pas de type DATA ou pas pour R0
        }
      }
      break;

    case E2: // Afficher la trame et filtrer les duplications par émetteur
      if (M5.Lcd.getCursorY() > 220) {
        M5.Lcd.fillScreen(BLACK);
        M5.Lcd.setCursor(0, 0);
      }

      uint8_t adrSource = rxbuf[0]; // @S de l'émetteur (1 à 9)[cite: 2]
      uint8_t numSeqRecu = rxbuf[3]; // NumSeq de la trame reçue[cite: 2]

      if (adrSource < 10 && RxSeq[adrSource] != numSeqRecu) // Nouvelle trame de cet émetteur
      {
        RxSeq[adrSource] = numSeqRecu;
        // MODIF M5Stack : Affichage sur l'écran LCD via M5.Lcd.printf[cite: 4]
        M5.Lcd.printf("De @%d -> @%d [Seq %d] (%d o): ", adrSource, rxbuf[1], numSeqRecu, rxlen);
        for (j = 0; j < rxlen; j++) {
          M5.Lcd.printf("%02X|", rxbuf[j]);
        }
        M5.Lcd.println();
      }
      else // Trame déjà reçue de cet émetteur (l'ACK précédent a subi une collision ou perte)
      {
        M5.Lcd.printf("De @%d [Seq %d] : Duplication (Renvoi ACK)\n", adrSource, numSeqRecu);
      }
      state = E3;
      break;

    case E3: // Envoyer l'ACK adressé à l'émetteur source[cite: 2]
      txbuf[0] = MyAdr;     // @S : 0 (adresse du puits R0)[cite: 2]
      txbuf[1] = rxbuf[0];  // @D : adresse source @S de la trame DATA reçue[cite: 2]
      txbuf[2] = TYPE_ACK;  // Type ACK[cite: 2]
      txbuf[3] = rxbuf[3];  // Même NumSeq que la trame DATA acquittée[cite: 2]

      rf95.send(txbuf, 4);  // Envoi des 4 octets de la trame ACK[cite: 2, 4]
      rf95.waitPacketSent(); //[cite: 4]
      state = E0;
      break;

    default:
      state = E0;
      break;
  }
}