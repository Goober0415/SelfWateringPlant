/*
 * Project SelfWateringPlantSystem
 * Author: Jamie Gavina
 * Date: 11/11/2024
 * For comprehensive documentation and examples, please visit:
 * https://docs.particle.io/firmware/best-practices/firmware-template/
 */

// Include Particle Device OS APIs
#include "Particle.h"
#include "Air_quality_Sensor.h"
#include <Adafruit_MQTT.h>
#include "Adafruit_MQTT/Adafruit_MQTT_SPARK.h"
#include "Adafruit_MQTT/Adafruit_MQTT.h"
#include "Credentials.h"
#include <math.h>
#include "Adafruit_SSD1306.h"
#include "Adafruit_BME280.h"

// Let Device OS manage the connection to the Particle Cloud
SYSTEM_MODE(AUTOMATIC);
TCPClient TheClient;

// Adafruit.io feeds

// constants
const int OLED_RESET = -1;
const int HEXADDRESS = 0x76;

// class
String dateTime, timeOnly;

// objects
Adafruit_SSD1306 display(OLED_RESET);
Adafruit_BME280 bme;

// Variables
unsigned int startTime1, airValue, duration, lowPulseOcc, currentQuality = -1;
int soilMoist = A2, subValue, tempC, presPA, humRH, tempF, inHG;
float dielectric;
bool status;

// functions
void bmeReads(int timeFrame);

void setup()
{
  WiFi.on();
  WiFi.connect();
  Serial.begin(9600);
  waitFor(Serial.isConnected, 1000);

  bme.begin(HEXADDRESS);
  Serial.printf("Ready to go\n");
  status = bme.begin(HEXADDRESS);
  if (status == false)
  {
    Serial.printf("BME280 at address 0x%02X failed to start", HEXADDRESS);
  }
  Time.zone(-7);
  Particle.syncTime();
  display.begin(SSD1306_SWITCHAPVCC, Ox3C);
  display.display();
  delay(2000);
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  MQTT.connect();

  // subscriptions

  // pinMode
  pinMode(soilMoist, INPUT);
}

void loop()
{
  bmeReads(10000);
}

void bmeReads(int timeFrame)
{
  static unsigned int currentTime, lastTime;
  currentTime = millis();

  if ((currentTime - lastTime) > timeFrame)
  {
    lastTime = millis();
    display.clearDisplay();
    tempC = bme.readTemperature();
    presPA = bme.readPresure();
    humRH = bme.readHumidity();

    tempF = map(tempC, 0, 38, 32, 100);
    inHG = map(presPA, 0, 135456, 0, 40);
    display.setRotation(3);
    display.setCursor(0, 0);
    display.printf("Temp F: %i", tempF);
    display.display();
    display.setCursor(0, 20);
    display.printf("Press: %i", inHG);
    display.display();
    display.setCursor(0, 40);
    display.printf("Hum: %i", humRH);
    display.display();
    humFeed.publish(humRH);
    tempFeed.publish(tempF);
  }
}