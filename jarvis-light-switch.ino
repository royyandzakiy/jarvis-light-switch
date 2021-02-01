#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <Servo.h> 
#include <NTPClient.h>
#include <WiFiUdp.h>
#include "credentials.h"

/*====================================*/
// global variables

#define MQTT_LIGHT_SWITCH true
#define AUTOMATE_LIGHT_SWITCH false // change to true if want to automate light switch with predefined time
  #define AUTOMATE_LIGHT_SWITCH_MORNING 6 // choose what time should the lights turn OFF at the morning
  #define AUTOMATE_LIGHT_SWITCH_NIGHT 18 // choose what time should the lights turn ON at the night

// WIFI & MQTT
WiFiClient espClient;
PubSubClient client(espClient);
const char* topic_light_switch = "jarvis/light_switch/1";
WiFiUDP ntpUDP;
const long utcOffsetInSeconds = 25200;
NTPClient timeClient(ntpUDP, "pool.ntp.org", utcOffsetInSeconds);

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
void loop_light_switch();

void set_mqtt();
void check_mqtt(long interval);
void mqtt_publish(char*, byte*, unsigned int);

void switch_change_on(bool);
void setup_servo();

/*====================================*/
// main code

void setup() {
  pinMode(BUILTIN_LED, OUTPUT);    // Initialize the BUILTIN_LED pin as an output
  digitalWrite(BUILTIN_LED, LOW); // set LED state to ON during setup
  Serial.begin(115200);
  Serial.println("Setup begin...");
  
  setup_wifi();
  setup_servo();
  setup_mqtt();
  loop_light_switch();
  check_mqtt(10000); // check for interval amount of time
  
  digitalWrite(BUILTIN_LED, HIGH); // reset LED state to OFF
  Serial.println("Setup done. Entering Deep Sleep...");
  ESP.deepSleep(3600e6); // enter deep sleep mode for interval seconds
  // ESP.deepSleep(5e6); // interval only 5 seconds for testing purposes
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
}

void loop_light_switch() {
  #if AUTOMATE_LIGHT_SWITCH
    // will check for current time, then turn off light if the time is right
    timeClient.update();
    String current_time = timeClient.getFormattedTime();
    String current_hour = current_time.substring(0,3);
  
    if (((current_hour.toInt() >= 0 && current_hour.toInt() <= AUTOMATE_LIGHT_SWITCH_MORNING) || ((current_hour.toInt() >= AUTOMATE_LIGHT_SWITCH_NIGHT) && (current_hour.toInt() <= 23)) {
      // if 18:00 or more, turn on lights
      switch_change_on(true);
      Serial.println("lights: on");
      mqtt_publish(topic_light_switch, "1", 1);
    } else {
      switch_change_on(false);
      Serial.println("lights: off");
      mqtt_publish(topic_light_switch, "0", 0);
    }
  #endif
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

  timeClient.begin();
  print_time();
}

void print_time() {
  timeClient.update();
  String current_time = timeClient.getFormattedTime();
  Serial.print("Current time is: ");
  Serial.println(current_time);

  String current_hour = current_time.substring(0,3);
  Serial.println(current_hour.toInt());
}

void setup_mqtt() {
  // mqtt
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  reconnect();
}

void check_mqtt(long interval) { // check for interval amount of time
  int start_count = millis();
  while ((millis() - start_count) < interval) {
    client.loop();
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  #if MQTT_LIGHT_SWITCH
  if ((char)payload[0] == '1') {
    switch_change_on(true);
    Serial.println("lights: on");
  } else if ((char)payload[0] == '0') {
    switch_change_on(false);
    Serial.println("lights: off");
  }
  #endif
}

void mqtt_publish(char* topic, byte* payload, unsigned int length) {
  // Allocate the correct amount of memory for the payload copy
  byte* p = (byte*)malloc(length);
  // Copy the payload to the new buffer
  memcpy(p,payload,length);
  client.publish(topic, p, length);
  // Free the memory
  free(p);
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "JarvisClient-";
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
