#include "HX711.h"
#include "DHT.h"

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>

// ===============================
// WIFI & TELEGRAM
// ===============================
const char* ssid = "";
const char* password = "";

#define BOT_TOKEN ""
#define CHAT_ID   ""

WiFiClientSecure client;
UniversalTelegramBot bot(BOT_TOKEN, client);

// ===============================
// PIN CONFIGURATION
// ===============================
#define DOUT 23
#define CLK 19

#define DHTPIN 26
#define DHTTYPE DHT11

#define PIRPIN 14
#define BUZZER 33

// ===============================
// OBJECTS
// ===============================
HX711 scale;
DHT dht(DHTPIN, DHTTYPE);

// ===============================
// SETTINGS
// ===============================
const float WEIGHT_THRESHOLD = 2.5;
const int BUZZER_FREQUENCY = 3000;

unsigned long previousBuzzerMillis = 0;
bool buzzerState = false;

// Telegram alert status
bool level1Sent = false;
bool level2Sent = false;

// ===============================
// SETUP
// ===============================
void setup() {

  Serial.begin(115200);

  // Load cell
  scale.begin(DOUT, CLK);
  scale.set_scale(211000);
  scale.tare();

  // DHT11
  dht.begin();

  // PIR
  pinMode(PIRPIN, INPUT);

  // Passive buzzer
  pinMode(BUZZER, OUTPUT);
  noTone(BUZZER);

  // ===============================
  // WIFI
  // ===============================
  WiFi.begin(ssid, password);

  Serial.print("Connecting to WiFi");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("WiFi Connected");

  client.setInsecure();

  Serial.println("------------------------------------------");
  Serial.println("CHILD PRESENCE DETECTION SYSTEM");
  Serial.println("------------------------------------------");

  bot.sendMessage(
    CHAT_ID,
    "Child Presence Detection System is ONLINE.",
    ""
  );
}

// ===============================
// WARNING LEVEL 1
// ===============================
void warningLevel1() {

  unsigned long currentMillis = millis();

  if (currentMillis - previousBuzzerMillis >= 1000) {

    previousBuzzerMillis = currentMillis;
    buzzerState = !buzzerState;

    if (buzzerState) {
      tone(BUZZER, BUZZER_FREQUENCY);
    } else {
      noTone(BUZZER);
    }
  }
}

// ===============================
// WARNING LEVEL 2
// ===============================
void warningLevel2() {

  unsigned long currentMillis = millis();

  if (currentMillis - previousBuzzerMillis >= 200) {

    previousBuzzerMillis = currentMillis;
    buzzerState = !buzzerState;

    if (buzzerState) {
      tone(BUZZER, BUZZER_FREQUENCY);
    } else {
      noTone(BUZZER);
    }
  }
}

// ===============================
// BUZZER OFF
// ===============================
void buzzerOff() {

  noTone(BUZZER);
  buzzerState = false;
}

// ===============================
// TELEGRAM LEVEL 1
// ===============================
void sendLevel1Alert(float weight, float temperature, float humidity) {

  String message = "";

  message += "WARNING LEVEL 1\n\n";
  message += "Possible child presence detected.\n";
  message += "No movement detected.\n\n";

  message += "Current Load: ";
  message += String(weight, 2);
  message += " kg\n";

  message += "Threshold: ";
  message += String(WEIGHT_THRESHOLD, 2);
  message += " kg\n";

  if (!isnan(temperature)) {
    message += "Temperature: ";
    message += String(temperature, 1);
    message += " C\n";
  }

  if (!isnan(humidity)) {
    message += "Humidity: ";
    message += String(humidity, 1);
    message += " %\n";
  }

  message += "\nPlease check the vehicle.";

  bot.sendMessage(CHAT_ID, message, "");
}

// ===============================
// TELEGRAM LEVEL 2
// ===============================
void sendLevel2Alert(float weight, float temperature, float humidity) {

  String message = "";

  message += "WARNING LEVEL 2 - URGENT\n\n";
  message += "Child presence and movement detected!\n\n";

  message += "Current Load: ";
  message += String(weight, 2);
  message += " kg\n";

  message += "Threshold: ";
  message += String(WEIGHT_THRESHOLD, 2);
  message += " kg\n";

  message += "PIR: MOVEMENT DETECTED\n";

  if (!isnan(temperature)) {
    message += "Temperature: ";
    message += String(temperature, 1);
    message += " C\n";
  }

  if (!isnan(humidity)) {
    message += "Humidity: ";
    message += String(humidity, 1);
    message += " %\n";
  }

  message += "\nURGENT: Please check the vehicle immediately.";

  bot.sendMessage(CHAT_ID, message, "");
}

// ===============================
// LOOP
// ===============================
void loop() {

  int statusPIR = digitalRead(PIRPIN);

  float humidity = dht.readHumidity();
  float temperature = dht.readTemperature();

  float weight = scale.get_units(5);

  if (weight < 0) {
    weight = 0;
  }

  // ===============================
  // SERIAL MONITOR
  // ===============================
  Serial.print("Current Load: ");
  Serial.print(weight, 2);
  Serial.print(" kg");

  Serial.print(" | Threshold: ");
  Serial.print(WEIGHT_THRESHOLD, 2);
  Serial.print(" kg");

  Serial.print(" | PIR: ");

  if (statusPIR == HIGH) {
    Serial.print("MOVEMENT");
  } else {
    Serial.print("NO MOVEMENT");
  }

  Serial.print(" | Temp: ");

  if (isnan(temperature)) {
    Serial.print("ERROR");
  } else {
    Serial.print(temperature, 1);
    Serial.print(" C");
  }

  Serial.print(" | Humidity: ");

  if (isnan(humidity)) {
    Serial.println("ERROR");
  } else {
    Serial.print(humidity, 1);
    Serial.println(" %");
  }

  // ===============================
  // NORMAL
  // ===============================
  if (weight <= WEIGHT_THRESHOLD) {

    buzzerOff();

    level1Sent = false;
    level2Sent = false;

    Serial.println("STATUS: NORMAL");
  }

  // ===============================
  // WARNING LEVEL 1
  // ===============================
  else if (weight > WEIGHT_THRESHOLD && statusPIR == LOW) {

    warningLevel1();

    Serial.println("STATUS: WARNING LEVEL 1");

    if (!level1Sent) {

      sendLevel1Alert(
        weight,
        temperature,
        humidity
      );

      Serial.println("Telegram Level 1 alert sent.");

      level1Sent = true;
    }
  }

  // ===============================
  // WARNING LEVEL 2
  // ===============================
  else if (weight > WEIGHT_THRESHOLD && statusPIR == HIGH) {

    warningLevel2();

    Serial.println("STATUS: WARNING LEVEL 2 - URGENT");

    if (!level2Sent) {

      sendLevel2Alert(
        weight,
        temperature,
        humidity
      );

      Serial.println("Telegram Level 2 alert sent.");

      level2Sent = true;
    }
  }

  Serial.println("------------------------------------------");

  delay(200);
}
