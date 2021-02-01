#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <Servo.h> 
#include "credentials.h"

/*====================================*/
// global variables

// WIFI & MQTT
WiFiClient espClient;
PubSubClient client(espClient);
const char* topic_light_switch = "jarvis/light_switch/1";

#ifndef CREDENTIALS_H
  // Update these with values suitable for your network.
  const char* ssid = "WIFI_SSID";
  const char* password = "WIFI_PASS";
  const char* mqtt_server = "broker-example.com";
  const char* mqtt_user = "MQTT_USER"; // if exists
  const char* mqtt_pass = "MQTT_PASS"; // if exists
#endif

// SERVO
int servoPin = 2; // Declare the Servo pin  2 = D4
Servo servo; // Create a servo object 

/*====================================*/
// Prototypes

void setup_wifi();
void callback(char*, byte*, unsigned int);
void reconnect();

void switch_change_on(bool);
void setup_servo();

/*====================================*/
// main code

void setup() {
  pinMode(BUILTIN_LED, OUTPUT);    // Initialize the BUILTIN_LED pin as an output
  digitalWrite(BUILTIN_LED, HIGH); // set LED state to ON during setup
  Serial.begin(115200);
  
  setup_wifi();
  setup_servo();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);

  digitalWrite(BUILTIN_LED, HIGH); // reset LED state to OFF
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
}

/*====================================*/
// Functions

// WIFI & MQTT
void setup_wifi() {
  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  randomSeed(micros());

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  // Switch on the LED if an 1 was received as first character
  if ((char)payload[0] == '1') {
    switch_change_on(true);
    digitalWrite(BUILTIN_LED, LOW);
  } else if ((char)payload[0] == '0') {
    switch_change_on(false);  // Turn the LED off by making the voltage HIGH
    digitalWrite(BUILTIN_LED, HIGH);
  }

}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (client.connect(clientId.c_str()), mqtt_user, mqtt_pass) {
      Serial.println("connected");
      // ... and resubscribe
      client.subscribe(topic_light_switch);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

// SERVO
void switch_change_on(bool turn_on) {
  if (turn_on) {
    servo.write(0);
    delay(500);
  } else {
    servo.write(30);
    delay(500);
  }
}

void setup_servo() { 
   // We need to attach the servo to the used pin number 
   servo.attach(servoPin); 
}
