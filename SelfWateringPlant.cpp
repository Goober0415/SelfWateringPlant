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
Adafruit_MQTT_SPARK mqtt(&TheClient, AIO_SERVER, AIO_SERVERPORT, AIO_USERNAME, AIO_KEY);
// subscription
Adafruit_MQTT_Subscribe waterFeed = Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME "/feeds/waterFeed");
// publish
Adafruit_MQTT_Publish airQualFeed = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/airQualFeed");
Adafruit_MQTT_Publish humFeed = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/humFeed");
Adafruit_MQTT_Publish moistureFeed = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/moistureFeed");
Adafruit_MQTT_Publish tempFeed = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/tempFeed");

// constants
const int OLED_RESET = -1;
const int PUMP = D16;
const int HEXADDRESS = 0x76;
const int DUSTS = D4;
const int HOUR = 600000;

// class
String dateTime, timeOnly;

// objects
Adafruit_SSD1306 display(OLED_RESET);
Adafruit_BME280 bme;
AirQualitySensor airQualSens(A2);

// Variables
unsigned int startTime1, airValue, duration, lowPulse, currentQual = -1;
int soilMoist = A1, subValue, tempC, presPA, humRH, tempF, inHG;
float ratio = 0, concentration = 0;
bool status;

// functions
void MQTT_connect();
void bmeReads(int timeFrame);
void dustS(int timeFrame);
void airS(int timeFrame);
void soilReads(int pin, int timeFrame, int threshold);
void waterPump(int pump, int timeON);

void setup()
{
  WiFi.on();
  WiFi.connect();
  Serial.begin(9600);
  waitFor(Serial.isConnected, 1000);

  bme.begin(HEXADDRESS);
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.display();
  Time.zone(-7);
  Particle.syncTime();
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  Serial.printf("Ready to go\n");
  status = bme.begin(HEXADDRESS);
  if (status == false)
  {
    Serial.printf("BME280 at address 0x%02X failed to start", HEXADDRESS);
  }

  // subscription
  mqtt.subscribe(&waterFeed);

  // pinMode
  pinMode(soilMoist, INPUT);
  digitalWrite(PUMP, subValue);
}

void loop()
{
  MQTT_connect();
  bmeReads(10000);
  dustS(HOUR);
  airS(10000);
  soilReads(soilMoist, HOUR, 2700);
  Adafruit_MQTT_Subscribe *subscription;
  while ((subscription = mqtt.readSubscription(100)))
  {
    if (subscription == &waterFeed)
    {
      subValue = atof((char *)waterFeed.lastread);
      digitalWrite(PUMP, subValue);
      Serial.printf("Water the plant: %i\n", subValue);
    }
  }
}

void MQTT_connect()
{
  int8_t ret;

  // Return if already connected.
  if (mqtt.connected())
  {
    return;
  }
  Serial.printf("MQTT connected!\n");
  while ((ret = mqtt.connect()) != 0)
  { // connect will return 0 for connected
    Serial.printf("Error Code %s\n", mqtt.connectErrorString(ret));
    Serial.printf("Retrying MQTT connection in 5 seconds...\n");
    mqtt.disconnect();
    delay(5000); // wait 5 seconds and try again
  }
  Serial.printf("MQTT Connected!\n");
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
    presPA = bme.readPressure();
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

void dustS(int timeFrame)
{
  static unsigned int currentTime, lastTime;
  int startTime = millis();
  currentTime = millis();
  if ((currentTime - lastTime) > timeFrame)
  {
    lastTime = millis();
    duration = pulseIn(DUSTS, LOW);
    lowPulse = lowPulse + duration;
  }
  if ((millis() - startTime) > 30000)
  {
    ratio = lowPulse / (30000.0);
    concentration = 1.1 * pow(ratio, 3) - 3.8 * pow(ratio, 3) + 520 * ratio + 0.62;
    Serial.printf("Low pulse = %i, Ratio = %f\n", lowPulse, ratio);
    lowPulse = 0;
    startTime = millis();
  }
  return;
}

void airS(int timeFrame)
{
  static unsigned long lastTime = 0;
  unsigned long currentTime = millis();

  if ((currentTime - lastTime) >= timeFrame)
  {
    lastTime = currentTime;

    airValue = airQualSens.getValue();
    currentQual = airQualSens.slope();

    char qualityStr[50];

    switch (currentQual)
    {
    case 0:
      strcpy(qualityStr, "{\"quality\":\"High Pollution\",\"message\":\"Caution! High pollution detected!\"}");
      break;
    case 1:
      strcpy(qualityStr, "{\"quality\":\"Rising\",\"message\":\"Pollution rising!\"}");
      break;
    case 2:
      strcpy(qualityStr, "{\"quality\":\"Low\",\"message\":\"Low pollution.\"}");
      break;
    case 3:
      strcpy(qualityStr, "{\"quality\":\"Good\",\"message\":\"Fresh air!\"}");
      break;
    default:
      strcpy(qualityStr, "{\"quality\":\"Unknown\",\"message\":\"Unknown quality\"}");
      break;
    }

    Serial.printf("Air Quality: %s\n", qualityStr);
    Serial.printf("Quant Value = %i\n", airValue);

    // Check MQTT connection before publishing
    if (!mqtt.connected())
    {
      MQTT_connect();
    }

    // Publish air quality
    if (!airQualFeed.publish(qualityStr))
    {
      Serial.println("Failed to publish air quality");
    }
    else
    {
      Serial.println("Published air quality successfully");
    }

    delay(500); // Small delay after publishing
  }
}

void soilReads(int pin, int timeFrame, int threshold)
{
  static unsigned int currentTime, lastTime;
  int moisturereads;

  currentTime = millis();
  moisturereads = analogRead(pin);

  if ((currentTime - lastTime) > timeFrame)
  {
    lastTime - millis();
    moistureFeed.publish(moisturereads);
    if (moisturereads > timeFrame)
    {
      Serial.printf("soil is dry! Watering pump activated");
    }
    else
    {
      Serial.printf("Soil moisture is at the right levels");
    }
  }
}

void waterPump(int pump, int timeON)
{
  digitalWrite(pump, HIGH);
  delay(timeON);
  digitalWrite(pump, LOW);
}
