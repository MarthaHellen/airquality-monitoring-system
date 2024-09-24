#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

// Declaration for SSD1306 display connected using I2C
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);


// DHT11 Setup
#include <dht.h>        // Include library
#define outPin 8        // Defines pin number to which the sensor is connected
dht DHT;                // Creates a DHT object

float temperature;
float humidity;
int readData;
char* AIRQUALITY = "Good";

// MQ2 Setup
#define UpperThreshold 400
#define LowerThreshold 200
#define MQ2pin 0
float gasValue;  //variable to store sensor value

// LED AND BUZZER SETUP
#define led_red 9
#define led_green 6
const int buzzerPin = 7;

// GSM SETUP
#include <SoftwareSerial.h>
#define rxPin 52
#define txPin 53

#define DELAY_MS 1000
#define GSM_DELAY_MS 2000
#define RESPONSE_TIMEOUT_MS 2000
#define GPRS_RESPONSE_TIMEOUT_MS 30000
#define DISCONNECT_TIMEOUT_MS 60000

SoftwareSerial SIM800(rxPin, txPin);

// THINGSPEAK SETUP
const char* APN = "internet";
const char* USER = "";
const char* PASS = "";
const char* THINGSPEAK_HOST = "api.thingspeak.com";
const int THINGSPEAK_PORT = 80;
const char* API_KEY = "SP6RAVTX6EDCHRA1";

// Prototypes
void initializeGSM();
void establishGPRSConnection();
boolean endGPRSConnection();
boolean isGPRSConnected();
boolean waitForExpectedResponse(const char* expectedAnswer = "OK", unsigned int timeout = RESPONSE_TIMEOUT_MS);
void connectToThingSpeak();
void sendDataToThingSpeak(float temperature, float humidity, int gasValue);
void disconnectFromThingSpeak();
void handleThingSpeak(float temperature, float humidity, int gasValue);

void setup()
{
  Serial.begin(9600);
  
  // initialize the OLED object
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }

  // LED AND BUZZER
  pinMode(led_red, OUTPUT);
  pinMode(led_green, OUTPUT);
  // Set the buzzer pin as an output
  pinMode(buzzerPin, OUTPUT);

  // Clear the buffer.
  display.clearDisplay();

  // Prompt warming up session on OLED
  display.setTextColor(WHITE);
  display.setCursor(0,24);
  display.setTextSize(2);
  display.print("MQ2 warming up!");
  display.display();
	delay(20000); // allow the MQ2 to warm up
  display.clearDisplay();

  // INITIALIZE GSM
  SIM800.begin(9600);
  Serial.println("Initializing SIM800...");
  initializeGSM();
}

void loop() {
  // put your main code here, to run repeatedly:
  lightLEDS();
  getTempHumidity();
  getGasValues();
  displayOLEDValues();
  handleThingSpeak(temperature, humidity, gasValue);
}

// ************************ LEDS *****************************************************

void lightLEDS(){
  digitalWrite(led_red, LOW);
  digitalWrite(led_green, HIGH);
}

// ************************ OLED *****************************************************
void displayOLEDValues(){
  display.setTextColor(WHITE);
  display.setCursor(0,20);
  display.setTextSize(1);
  display.print("Temperature = ");
	display.print(temperature);
	//Serial.print((t*9.0)/5.0+32.0);        // Convert celsius to fahrenheit
	//Serial.println("°F ");
  display.display();
  // HUMIDITY
  display.setTextColor(WHITE);
  display.setCursor(0,30);
  display.setTextSize(1);
	display.print("Humidity = ");
	display.print(humidity);
  display.display();
  // GAS SENSOR
  display.setTextColor(WHITE);
  display.setCursor(0,40);
  display.setTextSize(1);
  display.print("Gas values = ");
	display.print(gasValue);
  display.display();
  // AIRQUALITY STATUS
  display.setTextColor(WHITE);
  display.setCursor(0,50);
  display.setTextSize(1);
  display.print("AQ= ");
	display.print(AIRQUALITY);
  display.display();

  // Clear the buffer.
  display.clearDisplay();
}


// ************************ TEMPERATURE AND HUMIDITY *****************************************************

void getTempHumidity(){
  readData = DHT.read11(outPin);

	temperature = DHT.temperature;        // Read temperature
	humidity = DHT.humidity;           // Read humidity

	Serial.print("Temperature = ");
	Serial.print(temperature);
	Serial.print("°C | ");
	Serial.print((temperature*9.0)/5.0+32.0);        // Convert celsius to fahrenheit
	Serial.println("°F ");
	Serial.print("Humidity = ");
	Serial.print(humidity);
	Serial.println("% ");
	Serial.println("");

	delay(2000); // wait two seconds
}

// ************************ MQ2 GAS SENSOR *****************************************************
void getGasValues(){
  gasValue = analogRead(MQ2pin); // read analog input pin 0
  
  Serial.print("Sensor Value: ");
  Serial.print(gasValue);

  if(gasValue < LowerThreshold)
  {
    AIRQUALITY = "Good";
    Serial.print(" | Air: Good Quality!");

  } 
  else if(gasValue > LowerThreshold && gasValue < UpperThreshold)
  {
    AIRQUALITY = "Moderate";
    Serial.print(" | Air: Moderate Quality!");

  }
  else
  {
    AIRQUALITY = "Poor";
    Serial.print(" | Air: Poor Quality!");
    digitalWrite(led_red, HIGH);
    digitalWrite(led_green, LOW);

    // Produce a tone of 1000Hz for 500 milliseconds (0.5 seconds)
    tone(buzzerPin, 1000);
    delay(1000);
    // Stop the tone for 500 milliseconds (0.5 seconds)
    noTone(buzzerPin);
    delay(500);
  }
  
  Serial.println("");
  delay(2000); // wait 2s for next reading
}

// ************************ GSM AND THINGSPEAK *****************************************************
void initializeGSM() {
  SIM800.println("AT");
  waitForExpectedResponse();
  delay(GSM_DELAY_MS);
  SIM800.println("AT+CPIN?");
  waitForExpectedResponse("+CPIN: READY");
  delay(GSM_DELAY_MS);
  SIM800.println("AT+CFUN=1");
  waitForExpectedResponse();
  delay(GSM_DELAY_MS);
  SIM800.println("AT+CREG?");
  waitForExpectedResponse("+CREG: 0,");
  delay(GSM_DELAY_MS);
}

void establishGPRSConnection() {
  SIM800.println("AT+SAPBR=3,1,\"Contype\",\"GPRS\"");
  waitForExpectedResponse();
  delay(GSM_DELAY_MS);
  SIM800.println("AT+SAPBR=3,1,\"APN\",\"" + String(APN) + "\"");
  waitForExpectedResponse();
  delay(GSM_DELAY_MS);
  if (USER != "") {
    SIM800.println("AT+SAPBR=3,1,\"USER\",\"" + String(USER) + "\"");
    waitForExpectedResponse();
    delay(GSM_DELAY_MS);
  }
  if (PASS != "") {
    SIM800.println("AT+SAPBR=3,1,\"PWD\",\"" + String(PASS) + "\"");
    waitForExpectedResponse();
    delay(GSM_DELAY_MS);
  }
  SIM800.println("AT+SAPBR=1,1");
  waitForExpectedResponse("OK", GPRS_RESPONSE_TIMEOUT_MS);
  delay(GSM_DELAY_MS);
}

boolean isGPRSConnected() {
  SIM800.println("AT+SAPBR=2,1");
  if (waitForExpectedResponse("0.0.0.0") == 1) { return false; }
  return true;
}

boolean endGPRSConnection() {
  SIM800.println("AT+CGATT=0");
  waitForExpectedResponse("OK", DISCONNECT_TIMEOUT_MS);
  return true;
}

boolean waitForExpectedResponse(const char* expectedAnswer, unsigned int timeout) {
  unsigned long previous = millis();
  String response;
  while ((millis() - previous) < timeout) {
    while (SIM800.available()) {
      char c = SIM800.read();
      response.concat(c);
      if (response.indexOf(expectedAnswer) != -1) {
        Serial.println(response);
        return true;
      }
    }
  }
  Serial.println(response);
  return false;
}

void connectToThingSpeak() {
  Serial.println("Connecting to ThingSpeak...");
  SIM800.println("AT+CIPSTART=\"TCP\",\"" + String(THINGSPEAK_HOST) + "\"," + String(THINGSPEAK_PORT));
  if (waitForExpectedResponse("CONNECT OK")) {
    Serial.println("Connected to ThingSpeak");
  } else {
    Serial.println("Connection to ThingSpeak failed");
  }
}

void sendDataToThingSpeak(float temperature, float humidity, int gasValue) {
  String data = "api_key=" + String(API_KEY) + "&field1=" + String(temperature) + "&field2=" + String(humidity) + "&field3=" + String(gasValue);
  String postRequest = "POST /update HTTP/1.1\r\n";
  postRequest += "Host: " + String(THINGSPEAK_HOST) + "\r\n";
  postRequest += "Content-Type: application/x-www-form-urlencoded\r\n";
  postRequest += "Content-Length: " + String(data.length()) + "\r\n\r\n";
  postRequest += data;

  SIM800.println("AT+CIPSEND=" + String(postRequest.length()));
  if (waitForExpectedResponse(">")) {
    SIM800.println(postRequest);
    if (waitForExpectedResponse("OK")) {
      Serial.println("Data sent to ThingSpeak");
    } else {
      Serial.println("Failed to send data to ThingSpeak");
    }
  } else {
    Serial.println("Error in sending data to ThingSpeak");
  }
}

void disconnectFromThingSpeak() {
  SIM800.println("AT+CIPCLOSE");
  waitForExpectedResponse("CLOSE OK");
  Serial.println("Disconnected from ThingSpeak");
  delay(DELAY_MS);
}

void handleThingSpeak(float temperature, float humidity, int gasValue) {
  connectToThingSpeak();
  sendDataToThingSpeak(temperature, humidity, gasValue);
  disconnectFromThingSpeak();
}
