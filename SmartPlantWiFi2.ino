#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
//#include <ESP8266WebServer.h>
#include <WiFiClient.h> 
#include <ArduinoJson.h>

#define ADC_REF_VOLTAGE 3300.0
#define LOG_SERIAL true
#define DEBUG_MODE false
#define MAX_JSON_POST_BUFFER 512
#define MAX_JSON_GET_BUFFER 256

int chipId = 1;
int subnetId = 1;

char ssid[32] = "Livebox-3758";
char pass[64] = "qwerty123";
const char* host = "http://atom8api-dev.eu-central-1.elasticbeanstalk.com/v1/AddPlants";
const int port = 443;
//const char fingerprint[] PROGMEM = "A6 5A 41 2C 0E DC FF C3 16 E8 57 E9 F2 C3 11 D2 71 58 DF D9";

StaticJsonBuffer<MAX_JSON_POST_BUFFER> jsonBuffer;
char jsonPostPayload[MAX_JSON_POST_BUFFER];

const int ledPin = D0;
const int pumpPin = D1;

const int s0 = D5;
const int s1 = D6;
const int s2 = D7;
const int s3 = D8;

const int waterLevelDrivePin = D3;
const int soilHumidityDrivePin = D4;

float waterLevelValue;
float soilHumidityValue;

const float waterLevelThresh = 200.0;
const float soilHumidityThresh = 800.0;

unsigned long lastMeasurementTime = 0UL;
const unsigned long timeBetweenMeasurements = 10000UL;

bool pumpState = LOW;
unsigned long pumpStartTime = 0UL;
const unsigned long pumpDuration = 4000UL;

bool waterLow = false;
bool soilHumidityLow = false;

unsigned long lastBlinkTime = 0;
const unsigned long timeBetweenBlinks = 800UL;
int ledState = LOW;

float readMiliVolts(long nSamples = 20L) {
  long sum = 0;
  for (int i = 0; i < nSamples; i++) {
    sum += analogRead(A0);
  }
  return sum / nSamples / 1023.0 * ADC_REF_VOLTAGE;
}

float readHumiditySensor() {
  digitalWrite(soilHumidityDrivePin, HIGH);
  delay(10);
  digitalWrite(s0, LOW);
  digitalWrite(s1, LOW);
  digitalWrite(s2, LOW);
  digitalWrite(s3, LOW);
  float value = readMiliVolts(5000L);
  // Invert the sensor value
  value = ADC_REF_VOLTAGE - value;
  digitalWrite(soilHumidityDrivePin, LOW);
  yield();
  return value;
}

float readWaterLevelSensor() {
  digitalWrite(waterLevelDrivePin, HIGH);
  delay(10);
  digitalWrite(s0, HIGH);
  digitalWrite(s1, LOW);
  digitalWrite(s2, LOW);
  digitalWrite(s3, LOW);
  float value = readMiliVolts(5000L);
  digitalWrite(waterLevelDrivePin, LOW);
  yield();
  return value;
}

void startupLedSequence() {
  for (int i = 0; i <= 4; i++) {
    digitalWrite(ledPin, HIGH);
    delay(300);
    digitalWrite(ledPin, LOW);
    delay(300);
  }
  delay(1000);

  digitalWrite(ledPin, HIGH);
}

void readSensorData() {
  waterLevelValue = readWaterLevelSensor();
  soilHumidityValue = readHumiditySensor();
}

void setPayload() {
  JsonObject& jsonEncoder = jsonBuffer.createObject();
  jsonEncoder["WrittenId"] = chipId;
  jsonEncoder["SubnetId"] = subnetId;
  jsonEncoder["Name"] = "Ghiveciul salii de forta Bistrita";
  JsonArray& jsonPinsArray = jsonEncoder.createNestedArray("Pins");
  
  JsonObject& jsonWaterLevel = jsonPinsArray.createNestedObject();
  jsonWaterLevel["PinId"] = 0;
  jsonWaterLevel["PinName"] = "Water level sensor";
  jsonWaterLevel["PinValue"] = waterLevelValue;
  jsonWaterLevel["Measurement"] = "mV";

  JsonObject& jsonSoilHumidity = jsonPinsArray.createNestedObject();
  jsonSoilHumidity["PinId"] = 1;
  jsonSoilHumidity["PinName"] = "Soil humidity sensor";
  jsonSoilHumidity["PinValue"] = soilHumidityValue;
  jsonSoilHumidity["Measurement"] = "mV";

  jsonEncoder.prettyPrintTo(jsonPostPayload, sizeof(jsonPostPayload));
  if (LOG_SERIAL)
    Serial.println(jsonPostPayload);
  yield();
}

void sendSensorData() {
  if (WiFi.status() == WL_CONNECTED) {
    setPayload(); 
    HTTPClient http;
    http.begin(host);
    http.addHeader("Content-Type", "application/json");
    int httpCode = http.POST(jsonPostPayload);
    if (LOG_SERIAL) {
      Serial.print("HTTP Code: ");
      Serial.println(httpCode);
      //Serial.println(http.getString);
    }
    http.end();
    jsonBuffer.clear();
  }
  else if(LOG_SERIAL)
    Serial.println("WiFi connection error");
}

void checkThresholds() {
  if (waterLevelValue < waterLevelThresh)
    waterLow = true;
  else
    waterLow = false;

  if (soilHumidityValue < waterLevelThresh)
    soilHumidityLow = true;
  else
    soilHumidityLow = false;
}

void printSensorData() {
  Serial.print("Water level voltage: ");
  Serial.println(waterLevelValue);
  Serial.print("Soil humidity voltage: ");
  Serial.println(soilHumidityValue);
}

void setup() {
  Serial.begin(9600);

  pinMode(ledPin, OUTPUT);
  pinMode(pumpPin, OUTPUT);
  pinMode(s0, OUTPUT);
  pinMode(s1, OUTPUT);
  pinMode(s2, OUTPUT);
  pinMode(s3, OUTPUT);
  pinMode(A0, INPUT);
  pinMode(soilHumidityDrivePin, OUTPUT);
  pinMode(waterLevelDrivePin, OUTPUT);
  digitalWrite(soilHumidityDrivePin, LOW);
  digitalWrite(waterLevelDrivePin, LOW);

  WiFi.mode(WIFI_OFF);
  delay(1000);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass);
  if (LOG_SERIAL)
    Serial.println("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    yield();
  }
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
    
  chipId = ESP.getChipId();
  startupLedSequence();
}

void loop() {
  unsigned long currentTime = millis();
  // Main program loop timer
  if (currentTime > lastMeasurementTime + timeBetweenMeasurements) {
    readSensorData();
    checkThresholds();
    sendSensorData();
    if (LOG_SERIAL)
      printSensorData();
    lastMeasurementTime = currentTime;
  }

  // LED Error blink timer
  if (waterLow || soilHumidityLow) {
    if (currentTime > lastBlinkTime + timeBetweenBlinks) {
      if (ledState == LOW)
        ledState = HIGH;
      else
        ledState = LOW;
      lastBlinkTime = currentTime;
    }
  }
  else
    ledState = HIGH;
  digitalWrite(ledPin, ledState);

  // Pump automation
  if (soilHumidityLow) {
    if (!waterLow || DEBUG_MODE) {
      if (pumpState == LOW) {
        pumpState = HIGH;
        pumpStartTime = millis();
        if (LOG_SERIAL)
          Serial.println("Pumping water...");
      }
    }
  }

  if (currentTime > pumpStartTime + pumpDuration) {
    if (pumpState == HIGH) {
      // Wait for next sensor reading before turning pump on again
      soilHumidityLow = false;
      pumpState = LOW;
    }
  }
  digitalWrite(pumpPin, pumpState);
}
