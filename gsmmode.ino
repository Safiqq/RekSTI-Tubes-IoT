#include <HTTPClient.h>
#include <SPI.h>
#include <MFRC522.h>
#include <Wire.h>
#include <TinyGPS++.h>
#include <SoftwareSerial.h>

#define RX0  3
#define TX0  1
#define RXD2 16
#define TXD2 17
#define RST_PIN 2
#define SS_PIN 5
#define MISO_PIN 19
#define MOSI_PIN 23
#define SCK_PIN 18

const char* API_KEY = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6ImxnYnR3emFycndzcXlpenprYnJvIiwicm9sZSI6ImFub24iLCJpYXQiOjE3MTc4NjI1NzMsImV4cCI6MjAzMzQzODU3M30.r62tgxjLCcib70nedDeLF4-k5EDjoVKEGipnjnLZv98";

#define SIM800_TX 26
#define SIM800_RX 27
SoftwareSerial sim800(SIM800_TX, SIM800_RX);

TinyGPSPlus gps;
String smsStatus, senderNumber, receivedDate, msg;
MFRC522 mfrc522(SS_PIN, RST_PIN);

struct Passenger {
  byte uid[4];
};

#define MAX_PASSENGERS 32
Passenger passengers[MAX_PASSENGERS];
int passengerCount = 0;

void setup() {
  Serial.begin(115200);
  SPI.begin(SCK_PIN, MISO_PIN, MOSI_PIN);
  mfrc522.PCD_Init();
  sim800.begin(9600);
  neogps.begin(9600, SERIAL_8N1, RXD2, TXD2);
}

void loop() {
  boolean newData = false;
  unsigned long start = millis();

  while (millis() - start < 1000) {
    while (neogps.available()) {
      char c = neogps.read();
      if (gps.encode(c)) {
        newData = true;
      }
      // Debugging: print raw GPS data
      Serial.write(c);
    }
  }

  // If newData is true
  if (newData == true) {
    newData = false;
    Serial.println("Satellites: " + String(gps.satellites.value()));
    print_speed();
  } else {
    Serial.println("No Data");
  }

  MFRC522::MIFARE_Key key;
  byte block;
  byte len;
  MFRC522::StatusCode status;

  if (!mfrc522.PICC_IsNewCardPresent()) {
      Serial.print("Waiting for tag scan...");
      delay(1000);
    } else {
      // select one of the cards
      if (!mfrc522.PICC_ReadCardSerial()) return;

      // dump some details about the card
      mfrc522.PICC_DumpDetailsToSerial(&(mfrc522.uid));

      // Manage the passengers list
      updatePassengers(mfrc522.uid.uidByte);

      // stop communication with current RFID tag
      mfrc522.PICC_HaltA();
      // stop encryption/decryption process
      mfrc522.PCD_StopCrypto1();
    }

  String jsonString = "{\"bus_id\":1,\"lat\":" + String(gps.location.lat(), 6) + ",\"long\":" + String(gps.location.lng(), 6) + ",\"capacity\":" + String(passengerCount) + "}";

  sendPostRequest(jsonString);
}

void sendPostRequest(String json) {
  sim800.println("AT+CIPSTART=\"TCP\",\"lgbtwzarrwsqyizzkbro.supabase.co\",\"80\"");
  delay(2000);

  String httpRequest = "POST /rest/v1/details HTTP/1.1\r\n";
  httpRequest += "Host: lgbtwzarrwsqyizzkbro.supabase.co\r\n";
  httpRequest += "Content-Type: application/json\r\n";
  httpRequest += "apikey: " + String(API_KEY) + "\r\n";
  httpRequest += "Content-Length: " + String(json.length()) + "\r\n";
  httpRequest += "\r\n";
  httpRequest += json;

  sim800.print("AT+CIPSEND=");
  sim800.println(httpRequest.length());
  delay(1000);
  sim800.print(httpRequest);
  delay(2000);

  sim800.println("AT+CIPCLOSE");
  delay(1000);
}

void updatePassengers(byte* uid) {
  for (int i = 0; i < passengerCount; i++) {
    if (memcmp(passengers[i].uid, uid, 4) == 0) {
      // Remove passenger
      for (int j = i; j < passengerCount - 1; j++) {
        memcpy(passengers[j].uid, passengers[j + 1].uid, 4);
      }
      passengerCount--;
      Serial.println("Passenger removed.");
      return;
    }
  }

  if (passengerCount < MAX_PASSENGERS) {
    memcpy(passengers[passengerCount].uid, uid, 4);
    passengerCount++;
    Serial.println("Passenger added.");
  } else {
    Serial.println("Passenger list is full.");
  }
}

void print_speed() {
  if (gps.location.isValid()) {
    Serial.print("Lat: ");
    Serial.println(gps.location.lat(), 6);

    Serial.print("Lng: ");
    Serial.println(gps.location.lng(), 6);

    Serial.print("Speed: ");
    Serial.println(gps.speed.kmph());

    Serial.print("SAT: ");
    Serial.println(gps.satellites.value());

    Serial.print("ALT: ");
    Serial.println(gps.altitude.meters(), 0);
  } else {
    Serial.println("No Valid Data");
  }
}
