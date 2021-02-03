#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <Servo.h> 
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <TimeLib.h>
#include <TimeAlarms.h>
#include "credentials.h"

/*====================================*/
// Global Variables

// MODE SETUP
#define MODE_TIMEBASED_AUTOMATE_PIN_OUTPUT 10 // NodeMCU GPIO14 == SD3
#define MODE_TIMEBASED_AUTOMATE_PIN_INPUT 9 // NodeMCU GPIO12 == SD2
#define MODE_DEEP_SLEEP_PIN_OUTPUT 5 // NodeMCU GPIO12 == D1
#define MODE_DEEP_SLEEP_PIN_INPUT 4 // NodeMCU GPIO12 == D2
bool mode_switch_with_mqtt = true;
bool mode_deep_sleep = false;
bool mode_timebased_automate = true; // change to true if want to automate light switch with predefined time
  #define MODE_TIMEBASED_AUTOMATE_MORNING 6 // choose what time should the lights turn OFF at the morning
  #define MODE_TIMEBASED_AUTOMATE_NIGHT 18 // choose what time should the lights turn ON at the night
long last_hour = 0;
long last_millis = 0;

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
#define SERVO_PIN 2 // Declare the Servo pin  2 = D4
Servo servo; // Create a servo object 

/*====================================*/
// Prototypes

void setup_modes();

void setup_wifi();

void setup_mqtt();
void callback(const char*, byte*, unsigned int);
void reconnect();
void loop_timebased_automate();

void set_mqtt();
void check_mqtt(unsigned long);
void mqtt_publish(const char*, const char*, unsigned int);

void update_time();
void print_time();
void printDigits(int);


void switch_turn_on(bool);
void setup_servo();

void blink_once();

/*====================================*/
// Main Code

void setup() {
  pinMode(BUILTIN_LED, OUTPUT);    // Initialize the BUILTIN_LED pin as an output
  digitalWrite(BUILTIN_LED, LOW); // set LED state to ON during setup
  

  Serial.begin(115200);
  Serial.println("Setup begin...");
  
  setup_servo();
  setup_modes();
  setup_wifi();
  setup_mqtt();

  check_mqtt(3000); // check for interval amount of time
  
  digitalWrite(BUILTIN_LED, HIGH); // reset LED state to OFF
  Serial.println("Setup done.");

  if (mode_deep_sleep) { // default is false, except if set in setup_modes()
    Serial.println("Entering Deep Sleep...");
    ESP.deepSleep(3600e6); // enter deep sleep mode for 3600 interval seconds
  }

  update_time();

  Alarm.timerRepeat(3, blink_once); // blinks every 3 seconds
}

void loop() {
  // MQTT
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  // TIME
  if (((millis() - last_millis) > 3600000) && ((hour() - last_hour) != 1)) {
    // only check time under 2 conditions: (1) if an hour has passed, and (2) if the last hour and current hour 
    // is NOT different by 1 (which means it has been off). also gives a little bit of redundancy so there will 
    // be a check at least one times a day at change of day 
    Serial.println(millis() - last_millis);
    update_time();
  }
  Alarm.delay(1); // mandatory to add this so the alarm functions will run
  
  // LIGHT SWITCH
  if (mode_timebased_automate) {
    loop_timebased_automate();
  }
}

void blink_once() {
  digitalWrite(BUILTIN_LED, LOW);
  delay(500);
  digitalWrite(BUILTIN_LED, HIGH);
  delay(500);
  // Serial.println("blink_once");
}

/*====================================*/
// Functions

// MODES
void setup_modes() {
  // MODE: TIMEBASED_AUTOMATE (default true)
  pinMode(MODE_TIMEBASED_AUTOMATE_PIN_OUTPUT, OUTPUT);    // output pin for mode_timebased_automate false
  pinMode(MODE_TIMEBASED_AUTOMATE_PIN_INPUT, INPUT);     // input pin for mode_timebased_automate false
  digitalWrite(MODE_TIMEBASED_AUTOMATE_PIN_OUTPUT, HIGH);
  if (digitalRead(MODE_TIMEBASED_AUTOMATE_PIN_INPUT) > 0) {
    Serial.println("mode_timebased_automate = off");
    mode_timebased_automate = false;
  } else {
    Serial.println("mode_timebased_automate = on (default)");
  }
  Serial.println(digitalRead(MODE_TIMEBASED_AUTOMATE_PIN_INPUT));
  
  // MODE: DEEP_SLEEP (default false)
  pinMode(MODE_DEEP_SLEEP_PIN_OUTPUT, OUTPUT);     // output pin for mode_deep_sleep true
  pinMode(MODE_DEEP_SLEEP_PIN_INPUT, INPUT);      // input pin for mode_deep_sleep true
  digitalWrite(MODE_DEEP_SLEEP_PIN_OUTPUT, HIGH);
  if (digitalRead(MODE_DEEP_SLEEP_PIN_INPUT) > 0) {
    Serial.println("mode_deep_sleep = on");
    mode_deep_sleep = true;
  } else {
    Serial.println("mode_deep_sleep = off (default)");
  }
  Serial.println(digitalRead(MODE_DEEP_SLEEP_PIN_INPUT));
}

// TIME
void update_time() {
  Serial.println("update_time");
  timeClient.update();

  String current_time = timeClient.getFormattedTime();
  int current_hour = current_time.substring(0,3).toInt();
  int current_minute = current_time.substring(3,5).toInt();
  setTime(current_hour,current_minute,0,1,1,11);
  
  last_hour = hour();
  last_millis = millis();
  last_hour = current_hour;

  print_time();
}

void print_time() {
  String current_time = timeClient.getFormattedTime();
  Serial.print("Current time is: ");
  Serial.print(hour());
  printDigits(minute());
  printDigits(second());
  Serial.println();
}

void printDigits(int digits) {
  Serial.print(":");
  if (digits < 10)
    Serial.print('0');
  Serial.print(digits);
}

// WIFI
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
}

// MQTT
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

  if (mode_switch_with_mqtt) {
    if ((char)payload[0] == '1') {
      switch_turn_on(true);
      Serial.println("lights: on");
    } else if ((char)payload[0] == '0') {
      switch_turn_on(false);
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

void loop_timebased_automate() {
  // will check for current time, then turn off light if the time is right
  if (((hour() >= 0 && hour() <= MODE_TIMEBASED_AUTOMATE_MORNING) || ((hour() >= MODE_TIMEBASED_AUTOMATE_NIGHT) && (hour() <= 23)))) {
    // if 18:00 or more, turn on lights
    Serial.println("Night Time: On");
    mqtt_publish(topic_light_switch, "1", 1);
  } else {
    Serial.println("Night Time: Off");
    mqtt_publish(topic_light_switch, "0", 0);
  }
}

// SERVO
void setup_servo() { 
   // We need to attach the servo to the used pin number 
   servo.attach(SERVO_PIN); 
}

void switch_turn_on(bool turn_on) {
  if (turn_on) {
    servo.write(0);
    delay(500);
  } else {
    servo.write(30);
    delay(500);
  }
}