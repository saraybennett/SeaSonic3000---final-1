#include "secret.hpp"  //use the secret-template! put in your wifi details and change the filename to secret.hpp

#include <WiFiNINA.h>
#include <WebSocketClient.h>
#include <ArduinoJson.h>
#include <Servo.h>
#include "Adafruit_VL53L0X.h"

char ssid[] = SECRET_SSID;  // your network SSID (name)
char pass[] = SECRET_PASS;  // your network password (use for WPA, or use as key for WEP)

// WebSocket server
const char* websocket_server = "seasonic3000-final-1.onrender.com";  //update this with the actual server url
const int websocket_port = 443;                                      // SSL port for wss://

// Pin definitions
const int SERVO_PIN = 2;   //servo motor
const int JELLY_PIN = 3;   //jelly light for testing
const int ANGLER_PIN = 9;  //angler light


// make an instance of the library:
Adafruit_VL53L0X sensor = Adafruit_VL53L0X();


//for time of flight distance sensor
const int maxDistance = 2000;

// WebSocket client (use WiFiSSLClient for SSL/TLS)
WiFiSSLClient wifiClient;
WebSocketClient wsClient = WebSocketClient(wifiClient, websocket_server, websocket_port);

// Servo object
Servo myServo;

//state of my variables
int servoPosition = 90;  //eventually want a motor that can be an off OR on bool
bool jellyState = false;

// Connection tracking
unsigned long lastReconnectAttempt = 0;
const unsigned long reconnectInterval = 5000;

// Keepalive tracking
unsigned long lastHeartbeat = 0;
const unsigned long heartbeatInterval = 30000;  // Send heartbeat every 30 seconds


void setup() {
  Serial.begin(9600);
  while (!Serial) {
    ;  //Wait for serial port to connect
  }

  //set pin mode:
  pinMode(JELLY_PIN, OUTPUT);

  // Initialize LEDs to off
  analogWrite(ANGLER_PIN, 0);
  digitalWrite(JELLY_PIN, LOW);

  // Initialize servo - eventually want a diff kind of motor
  myServo.attach(SERVO_PIN);
  myServo.write(servoPosition);  // Set to center position (90 degrees)

  // Connect to WiFi
  connectWiFi();

  // Connect to WebSocket server
  connectWebSocket();

  // initialize sensor, stop if it fails:
  /* if (!sensor.begin()) { */
  /*   Serial.println("Sensor not responding. Check wiring."); */
  /*   while (true) */
  /*     ; */
  /* } */

  /* config can be:
VL53L0X_SENSE_DEFAULT: about 500mm range
VL53L0X_SENSE_LONG_RANGE: about 2000mm range
VL53L0X_SENSE_HIGH_SPEED: about 500mm range
VL53L0X_SENSE_HIGH_ACCURACY: about 400mm range, 1mm accuracy
*/
  // //decide what configuration we want for this
  // sensor.configSensor(Adafruit_VL53L0X::VL53L0X_SENSE_LONG_RANGE);
  // // set sensor to range continuously:
  // sensor.startRangeContinuous();
}

void loop() {
  if (wsClient.connected()) {  // Check if connected

    // Check for incoming messages
    int messageSize = wsClient.parseMessage();
    if (messageSize > 0) {  //if we have a message, do this!
      Serial.print("Incoming message size: ");
      Serial.println(messageSize);

      String message = "";                 //start with an empty string
      while (wsClient.available()) {       //while there are messages to read
        message += (char)wsClient.read();  //read the message and concatenate to our message string
      }

      if (message.length() > 0) {  //if there is anything in the message
        handleMessage(message);    //call our helper fuction to parse the message
      }
    }

    //time of flight sensor - > light code - TOF not working 11.30 SB
    // Commented out until sensor is working properly
    // if (sensor.isRangeComplete()) {
    // // read the result:
    // int result = sensor.readRangeResult();

    // //ai overview helped write this section
    // int brightness_angler = map(result, 40, 500, 255, 0);

    // // Ensure brightness is within the valid range (0-255)
    // brightness_angler = constrain(brightness_angler, 0, 255);

    // // Set the LED brightness using PWM
    // analogWrite(ANGLER_PIN, brightness_angler);

    // // Print values for debugging
    // Serial.print("Distance (mm): ");
    // Serial.print(result);
    // Serial.print("\tBrightness (0-255): ");
    // Serial.println(brightness_angler);
    // Serial.println("brightness updated - sending to server");

    // // Send brightness to server - SEE adam's notes about this on discord
    //   wsClient.beginMessage(TYPE_TEXT);
    //   String bangleString = String(brightness_angler);
    //   wsClient.print("{\"type\":\"brightness_angler\",\"value\":" + bangleString + "}");
    //   wsClient.endMessage();
    // }
    // else {
    //   // Handle invalid readings, e.g., by turning the LED off
    //   analogWrite(ANGLER_PIN, 0);
    //   Serial.println("Out of range");
    // }
    // Send periodic heartbeat to keep connection alive
    if (millis() - lastHeartbeat > heartbeatInterval) {
      Serial.println("Sending heartbeat ping...");
      wsClient.beginMessage(TYPE_TEXT);
      wsClient.print("{\"type\":\"ping\"}");  //sends {"type":"ping"} to the server, which is not broadcast to clients
      wsClient.endMessage();
      lastHeartbeat = millis();
    }
  } else {
    // Try to reconnect
    Serial.print("Connection status: ");
    Serial.println(wsClient.connected() ? "Connected" : "Disconnected");
    if (millis() - lastReconnectAttempt > reconnectInterval) {
      Serial.println("WebSocket disconnected, reconnecting...");
      connectWebSocket();
      lastReconnectAttempt = millis();
    }
  }

  delay(50);
}

//this funcition handles connecting to wifi
void connectWiFi() {
  Serial.print("Connecting to WiFi: ");
  Serial.println(ssid);

  while (WiFi.begin(ssid, pass) != WL_CONNECTED) {
    Serial.print(".");
    delay(1000);
  }

  Serial.println("\nWiFi connected!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

// this function handles connecting to the socket server
void connectWebSocket() {
  Serial.print("Connecting to WebSocket server: ");
  Serial.print(websocket_server);
  Serial.print(":");
  Serial.println(websocket_port);

  wsClient.begin();

  if (wsClient.connected()) {
    Serial.println("WebSocket connected!");
    lastHeartbeat = millis();  // Reset heartbeat timer on new connection
  } else {
    Serial.println("WebSocket connection failed!");
  }
}

//help function to parse the message from ther server
//called whenever a message is received
void handleMessage(String message) {
  Serial.print("Received: ");
  Serial.println(message);

  // Parse JSON using ArduinoJson
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, message);

  if (error) {
    Serial.print("JSON parsing failed: ");
    Serial.println(error.c_str());
    return;
  }

  // Extract the type field
  const char* type = doc["type"];

  // Check for initialState message
  String typeStr = String(type);  //typecast the char* to a string for comparision

  Serial.print("type string = ");
  Serial.println(typeStr);

  if (typeStr == "initialState") {  //all the data
    /* ledState = doc["state"]["ledOn"];                   //update led state to match server state */
    /* digitalWrite(BLUE_LED_PIN, ledState ? HIGH : LOW);  //ternery, handle the light value accordingly */
    /* Serial.print("Initial LED state: "); */
    /* Serial.println(ledState ? "ON" : "OFF"); */

    //   // Get brightness and flashInterval from initial state
    //   if (doc["state"].containsKey("brightness")) {
    //     brightness = doc["state"]["brightness"];
    //     analogWrite(YELLOW_LED_PIN, brightness);
    //     Serial.print("Initial brightness: ");
    //     Serial.println(brightness);
    //   }

    //   if (doc["state"].containsKey("pulse")) {
    //     pulseInterval = doc["state"]["pulse"];
    //     Serial.print("Initial pulse interval: ");
    //     Serial.println(pulseInterval);
    //   }

    if (doc["state"].containsKey("servo")) {
      servoPosition = doc["state"]["servo"];
      servoPosition = constrain(servoPosition, 0, 180);
      myServo.write(servoPosition);
      Serial.print("Initial servo position: ");
      Serial.println(servoPosition);
    }

    if (doc["state"].containsKey("jellyOn")) {  //when you are sending the initial state from your server the sent object has the key jellyOn, not jellyState
      jellyState = doc["state"]["jellyOn"];
      Serial.print("Initial jellyState: ");
      Serial.println(jellyState);
    }


  }  //end of initial state check
  // // Check for brightness message
  // else if (typeStr == "brightness") {
  //   brightness = doc["value"];
  //   analogWrite(YELLOW_LED_PIN, brightness);
  //   Serial.print("Brightness updated to: ");
  //   Serial.println(brightness);
  // }
  // // Check for flashInterval message
  // else if (typeStr == "pulse") {
  //   pulseInterval = doc["value"];
  //   Serial.print("pulse interval updated to: ");
  //   Serial.println(pulseInterval);

  //   // Reset flash timer when interval changes
  //   lastPulseTime = millis();
  //   if (pulseInterval == 0) {
  //     digitalWrite(GREEN_LED_PIN, LOW);
  //     greenLedState = false;
  //   }
  // }
  // Check for servo message
  else if (typeStr == "servo") {
    servoPosition = doc["value"];
    // Constrain value to valid servo range (0-180)
    servoPosition = constrain(servoPosition, 0, 180);
    myServo.write(servoPosition);
    Serial.print("Servo position updated to: ");
    Serial.println(servoPosition);
  } else if (typeStr == "jellyState") {                //check for jellyState, your server is sending the message jellyState with a value of true or false
    jellyState = doc["value"];                         //parse the led state from the returned json.
    digitalWrite(JELLY_PIN, jellyState ? HIGH : LOW);  //ternery, handle the light value accordingly
    Serial.print("jellyState toggled to: ");
    Serial.println(jellyState ? "ON" : "OFF");
  }
}
