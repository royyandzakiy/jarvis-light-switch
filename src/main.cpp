#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <Servo.h> 
#include <NTPClient.h>
#include <WiFiUdp.h>
#include "credentials.h"

/*====================================*/
// global variables

bool light_switch_mqtt = true;
bool light_switch_time_automate = true; // change to true if want to automate light switch with predefined time
  #define LIGHT_SWITCH_TIME_AUTOMATE_MORNING 6 // choose what time should the lights turn OFF at the morning
  #define LIGHT_SWITCH_TIME_AUTOMATE_NIGHT 18 // choose what time should the lights turn ON at the night
bool mode_deep_sleep = false;

// pins for manual settings
#define LIGHT_SWITCH_TIME_AUTOMATE_PIN 14
#define MODE_DEEP_SLEEP_PIN 5

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
void setup_mqtt();
void callback(const char*, byte*, unsigned int);
void reconnect();
void loop_light_switch();
void print_time();
void setup_modes();

void set_mqtt();
void check_mqtt(unsigned long);
void mqtt_publish(const char*, const char*, unsigned int);

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
  setup_modes();
  check_mqtt(10000); // check for interval amount of time
  
  digitalWrite(BUILTIN_LED, HIGH); // reset LED state to OFF
  Serial.println("Setup done.");

  if (mode_deep_sleep) {
    Serial.println("Entering Deep Sleep...");
    ESP.deepSleep(3600e6); // enter deep sleep mode for interval seconds
    // ESP.deepSleep(5e6); // interval only 5 seconds for testing purposes
  }
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  loop_light_switch();
}

void loop_light_switch() {
  if (light_switch_time_automate) {
    // will check for current time, then turn off light if the time is right
    timeClient.update();
    String current_time = timeClient.getFormattedTime();
    String current_hour = current_time.substring(0,3);
  
    if (((current_hour.toInt() >= 0 && current_hour.toInt() <= LIGHT_SWITCH_TIME_AUTOMATE_MORNING) || ((current_hour.toInt() >= LIGHT_SWITCH_TIME_AUTOMATE_NIGHT) && (current_hour.toInt() <= 23)))) {
      // if 18:00 or more, turn on lights
      Serial.println("Night Time: On");
      mqtt_publish(topic_light_switch, "1", 1);
    } else {
      Serial.println("Night Time: Off");
      mqtt_publish(topic_light_switch, "0", 0);
    }
  }
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

void setup_modes() {
  // MODE: LIGHT_SWITCH_TIME_AUTOMATE (default true)
  pinMode(LIGHT_SWITCH_TIME_AUTOMATE_PIN, OUTPUT);    // output pin for light_switch_time_automate false
  pinMode(12, INPUT);     // input pin for light_switch_time_automate false
  digitalWrite(LIGHT_SWITCH_TIME_AUTOMATE_PIN, HIGH);
  if (digitalRead(12) > 0) {
    light_switch_time_automate = false;
  }
  
  // MODE: DEEP_SLEEP (default false)
  pinMode(MODE_DEEP_SLEEP_PIN, OUTPUT);     // output pin for mode_deep_sleep true
  pinMode(4, INPUT);      // input pin for mode_deep_sleep true
  digitalWrite(MODE_DEEP_SLEEP_PIN, HIGH);
  if (digitalRead(4) > 0) {
    mode_deep_sleep = true;
  }
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

void check_mqtt(unsigned long interval) { // check for interval amount of time
  unsigned long start_count = millis();
  while ((millis() - start_count) < interval) {
    client.loop();
  }
}

void callback(const char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (unsigned int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  if (light_switch_mqtt) {
    if ((char)payload[0] == '1') {
      switch_change_on(true);
      Serial.println("lights: on");
    } else if ((char)payload[0] == '0') {
      switch_change_on(false);
      Serial.println("lights: off");
    }
  }
}

void mqtt_publish(const char* topic, const char* payload, unsigned int length) {
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
