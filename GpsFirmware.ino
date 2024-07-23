// Your GPRS credentials (leave empty, if not needed)
#include <SoftwareSerial.h>
#include <PubSubClient.h>

#define MYPORT_TX 19
#define MYPORT_RX 18

//SoftwareSerial myPort;

const char apn[]      = "mtelsmart"; // APN (example: internet.vodafone.pt) use https://wiki.apnchanger.org
const char gprsUser[] = ""; // GPRS User
const char gprsPass[] = ""; // GPRS Password

// SIM card PIN (leave empty, if not defined)
const char simPIN[]   = "";

char mailStr[52];

long lastMsg = 0;
char msg[50];
int value = 0;

// Server details
// The server variable can be just a domain name or it can have a subdomain. It depends on the service you are using
const char server[] = "gps.server-ip.com"; // domain name: example.com, maker.ifttt.com, etc
const char resource[] = "/api/device-data/";         // resource path, for example: /post-data.php
const char resource2[] = "gps.server-ip.com/api/device-data/";         // resource path, for example: /post-data.php
const int  port = 80;                             // server port number

// Keep this API Key value to be compatible with the PHP code provided in the project page.
// If you change the apiKeyValue value, the PHP file /post-data.php also needs to have the same key
String apiKeyValue = "tPmAT5Ab3j7F9";

// TTGO T-Call pins
#define MODEM_RST            5
#define MODEM_PWKEY          4
#define MODEM_POWER_ON       23
#define MODEM_TX             27
#define MODEM_RX             26
#define I2C_SDA              21
#define I2C_SCL              22

// Set serial for debug console (to Serial Monitor, default speed 115200)
#define SerialMon Serial
// Set serial for AT commands (to SIM800 module)
#define SerialAT Serial1


// Your phone number to send SMS: + (plus sign)
#define SMS_TARGET  "+38766111111"

//sender phone number with country code.
//not gsm module phone number
//const String SMS_TARGET = "+38765403648";


// Configure TinyGSM library
#define TINY_GSM_MODEM_SIM800      // Modem is SIM800
#define TINY_GSM_RX_BUFFER   1024  // Set RX buffer to 1Kb

// Define the serial console for debug prints, if needed
//#define DUMP_AT_COMMANDS

#include <Wire.h>
#include <TinyGsmClient.h>
#include <TinyGPS++.h>
//#include <Arduino_JSON.h>
#include <ArduinoJson.h>
//#include <HTTPClient.h>
//#include <ArduinoHttpClient.h>

#ifdef DUMP_AT_COMMANDS
#include <StreamDebugger.h>
StreamDebugger debugger(SerialAT, SerialMon);
TinyGsm modem(debugger);
#else
TinyGsm modem(SerialAT);
#endif


// I2C for SIM800 (to keep it running when powered from battery)
TwoWire I2CPower = TwoWire(0);


// TinyGSM Client for Internet connection
TinyGsmClient client(modem, 0);

TinyGPSPlus gps;

#define uS_TO_S_FACTOR 1000000UL   /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP  60        /* Time ESP32 will go to sleep (in seconds) 3600 seconds = 1 hour */

#define IP5306_ADDR          0x75
#define IP5306_REG_SYS_CTL0  0x00

String smsStatus, senderNumber, receivedDate, smsMsg;
boolean newData = false;

SoftwareSerial myPort;
//HttpClient httpClient = HttpClient(client, server, port);
char jsonOutput[128];
//// Replace with your unique IFTTT URL resource
////const char* resource = "/trigger/event/with/key/qQg5R1ty5BPDGKbK_gQIS";
//const char* resource1 = "http://maker.ifttt.com/trigger/event/with/key/qQg5R1ty5BPDGKbK_gQIS";
//
//// Maker Webhooks IFTTT
//const char* serverEvents = "maker.ifttt.com";

// Add your MQTT Broker IP address, example:
//const char* mqtt_server = "192.168.1.144";
const char* mqtt_server = "xxx.xxx.xxx.xxx";
PubSubClient mqttClient(client);

char* jsonString = "Test";

//CborPayload payload;
String deviceId = "1";

float latitude, longitude;
int speed;

bool setPowerBoostKeepOn(int en) {
  I2CPower.beginTransmission(IP5306_ADDR);
  I2CPower.write(IP5306_REG_SYS_CTL0);
  if (en) {
    I2CPower.write(0x37); // Set bit1: 1 enable 0 disable boost keep on
  } else {
    I2CPower.write(0x35); // 0x37 is default reg value
  }
  return I2CPower.endTransmission() == 0;
}



void callback(char* topic, byte* message, unsigned int length) {
  Serial.print("Message arrived on topic: ");
  Serial.print(topic);
  Serial.print(". Message: ");
  //  String messageTemp;

  for (int i = 0; i < length; i++) {
    Serial.print((char)message[i]);
    //    messageTemp += (char)message[i];
  }
  Serial.println();
}

void reconnect() {
  // Loop until we're reconnected
  while (!mqttClient.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (mqttClient.connect("ESP32GPS", "gps", "Password++")) {
      Serial.println("connected");
      // Subscribe
      mqttClient.subscribe("esp32/output");
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void publishSensorData() {
  char JSONmessageBuffer[512];
  DynamicJsonDocument doc(512);

  JsonObject gps = doc.createNestedObject("gps");
  gps["latitude"] = latitude;
  gps["longitude"] = longitude;
  gps["speed"] = speed;

  serializeJson(doc, JSONmessageBuffer);

  char topic[128];
  snprintf(topic, sizeof topic, "%s%s%s", "device/", deviceId, "/state");
  mqttClient.publish(topic, JSONmessageBuffer, false);
  Serial.print("[DATA] Published sensor data to AllThingsTalk: ");
  Serial.println(topic);
}

void setup() {
  // Set serial monitor debugging window baud rate to 115200
  SerialMon.begin(9600);
  //  myPort.begin(9600);
  myPort.begin(9600, SWSERIAL_8N1, MYPORT_RX, MYPORT_TX, false);
  if (!myPort) { // If the object did not initialize, then its configuration is invalid
    Serial.println("Invalid SoftwareSerial pin configuration, check config");
    while (1) { // Don't continue with invalid configuration
      delay (1000);
    }
  }


  // Set modem reset, enable, power pins
  pinMode(MODEM_PWKEY, OUTPUT);
  pinMode(MODEM_RST, OUTPUT);
  pinMode(MODEM_POWER_ON, OUTPUT);
  digitalWrite(MODEM_PWKEY, LOW);
  digitalWrite(MODEM_RST, HIGH);
  digitalWrite(MODEM_POWER_ON, HIGH);

  // Set GSM module baud rate and UART pins
  SerialAT.begin(9600, SERIAL_8N1, MODEM_RX, MODEM_TX);
  delay(3000);

  // Restart SIM800 module, it takes quite some time
  // To skip it, call init() instead of restart()
  //  SerialMon.println("Initializing modem...");
  modem.restart();
  // use modem.init() if you don't need the complete restart

  // Unlock your SIM card with a PIN if needed
  if (strlen(simPIN) && modem.getSimStatus() != 3 ) {
    modem.simUnlock(simPIN);
  }

  //  // You might need to change the BME280 I2C address, in our case it's 0x76
  //  if (!bme.begin(0x76, &I2CBME)) {
  //    Serial.println("Could not find a valid BME280 sensor, check wiring!");
  //    while (1);
  //  }

  ////  // Configure the wake up source as timer wake up
  ////  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
  SerialMon.print("Connecting to APN: ");
  SerialMon.print(apn);
  if (!modem.gprsConnect(apn, gprsUser, gprsPass)) {
    SerialMon.println(" fail");
  }
  else {
    SerialMon.println(" OK");
    bool res = modem.isGprsConnected();
    SerialMon.println(res ? "Gprs connected" : "Gprs not connected");
  }


  smsStatus = "";
  senderNumber = "";
  receivedDate = "";
  smsMsg = "";

  //  SerialAT.print("AT + CMGF = 1\r"); //SMS text mode
  delay(1000);

  mqttClient.setServer(mqtt_server, 1883);
  mqttClient.setCallback(callback);
}

void loop() {

  if (!mqttClient.connected()) {
    reconnect();
  }
  mqttClient.loop();

  while (myPort.available())
  {
    //      Serial.println(myPort.read());
    if (gps.encode(myPort.read()))
    {
      latitude = gps.location.lat();
      longitude = gps.location.lng();
      speed = gps.speed.kmph();



    }
  }

  long now = millis();
  if (now - lastMsg > 5000) {
    lastMsg = now;

    
    publishSensorData();

  }

}
// Put ESP32 into deep sleep mode (with timer wake up)
//    esp_deep_sleep_start();


