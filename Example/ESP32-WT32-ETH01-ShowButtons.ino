
#include <WebServer_WT32_ETH01.h>
#include "Configure.h"
#include <Arduino.h>
#include <PubSubClient.h>


//===============================[ MOTT ]=================================
char msg[MSG_BUFFER_SIZE];
const int MQTT_SERVERPORT = 1883;  // use 8883 for SSL
const char* MQTT_SERVER = "192.168.10.10";
const char* MQTT_USERNAME = "mqttuser";
const char* MQTT_KEY =  "1234567";


//======================[ Touch variables ] =============================
const uint8_t threshold = 30;
bool touchedT0 = false; // Touch Sensor 0 GPIO4
bool touchedT3 = false; // Touch Sensor 3 GPI015

const long touchDelay = 350; //ms
int value = 0;

volatile unsigned long sinceLastTouchT0 = 0;
volatile unsigned long sinceLastTouchT3 = 0;

bool touchDelayComp(unsigned long);

void IRAM_ATTR T0wasActivated() {
  touchedT0 = true;
}
void IRAM_ATTR T3wasActivated() {
  touchedT3 = true;
}


//================[ Push Button variables]===============================
short pressed[100]; // an array keeping state of the buttons between loops
short buttons; // number of buttons
int Processed = 0;
unsigned long elapseTime;


//========= Select the IP address according to your local network ========
IPAddress myIP(192, 168, 10, 128);
IPAddress myGW(192, 168, 10, 1);
IPAddress mySN(255, 255, 255, 0);
IPAddress myDNS(192, 168, 10, 1);


// Initialize the Ethernet client object
WiFiClient espClient;
PubSubClient client(espClient);


void setup()
{
  Serial.begin(SERIALRATE);
  while (!Serial);

  WT32_ETH01_onEvent();  // To be called before ETH.begin()
  ETH.begin(ETH_PHY_ADDR, ETH_PHY_POWER);
  ETH.config(myIP, myGW, mySN, myDNS);  // Static IP
  WT32_ETH01_waitForConnect();

  //==========================[ MQTT ]======================================
  client.setServer(MQTT_SERVER, MQTT_SERVERPORT);
  client.setCallback(callback);

  //===================[ Push Button config ]===============================

  // work out the number of buttons
  buttons = BUTTONS;
  if (buttons > sizeof(pressed) / sizeof(short))
  {
    buttons = sizeof(pressed) / sizeof(short);
#ifdef DEBUG
    Serial.print("Buttons limited to ");
    Serial.println(buttons);
#endif
  }

  touchAttachInterrupt(T0, T0wasActivated, threshold);
  touchAttachInterrupt(T3, T3wasActivated, threshold);

  // setup the power pin
  pinMode(POWERPIN, OUTPUT);
  digitalWrite(POWERPIN, HIGH);

  // set up the button pressed pin
  pinMode(PRESSPIN, OUTPUT);


  // setup our input pins
  for (int i = 0; i < buttons; i++)
  {
    pinMode(pins[i], INPUT_PULLUP);
    pressed[i] = LOW;
  }

}

void loop()
{

  if (!client.connected()) {
    reconnect();
  }

  getButtonPress();  //Process push button event triggers
  //getTouchState();  //Process touch event triggers
}



void getButtonPress() {

  if ((millis() - elapseTime) > 10) {  // a small delay helps deal with key bounce

    bool ispressed = false; // keep track if any button press so the press indicator LED can be turned on

    // check each button
    for (int i = 0; i < BUTTONS; i++)
    {
      // read it
      short v = digitalRead(pins[i]);

      // if it is pressed immediately light the led
      if (v == LOW)
      {
        digitalWrite(PRESSPIN, HIGH);
        ispressed = true;

      }

      // if the button has changed state
      if (pressed[i] != v)
      {
#ifdef DEBUG
        Serial.print("Pin changed state: ");
        Serial.print(pins[i]);
        Serial.print(" : ");
        Serial.println(v);
#endif
        if (v == LOW)
        {
          // button is newly pressed
          if (pins[i] == 12) {
            SendData("Next");
            //Serial.println(pins[i]);
          }
          if (pins[i] == 14) {
            SendData ("Back");
            //Serial.println(pins[i]);
          }
        }
        pressed[i] = v;
      }
    }

    // if no button is pressed clear the pressed LED
    if (!ispressed)
    {
      digitalWrite(PRESSPIN, LOW);
    }
    elapseTime = millis();
  }

}

void getTouchState() {

  if (touchedT0)
  {
    touchedT0 = false;
    if (touchDelayComp(sinceLastTouchT0))
    {
#ifdef DEBUG
      //Reduce value when Touch0 is trigered
      value = value - 1;
      Serial.print("T0: "); Serial.println(value);
#endif
      SendData("/api/command/Next Playlist Item");
      sinceLastTouchT0 = millis();
    }
  }

  if (touchedT3)
  {
    touchedT3 = false;
    if (touchDelayComp(sinceLastTouchT3))
    {
#ifdef DEBUG
      //incriment values when Touch3 is triggered
      value = value + 1;
      Serial.print("T3: "); Serial.println(value);
#endif
      SendData ("//api/command/Prev Playlist Item");
      sinceLastTouchT3 = millis();
    }
  }

}

bool touchDelayComp(unsigned long lastTouch)
{
  if (millis() - lastTouch < touchDelay) return false;
  return true;
}


void SendData (String urlrequest) {

  if (urlrequest == "Next") {

    snprintf (msg, MSG_BUFFER_SIZE, "%ld", "next");
    digitalWrite(2, HIGH);
    Serial.print("Publish message: ");
    Serial.println(msg);
    client.publish("falcon/player/ShowPlayer/set/playlist/${PLAYLISTNAME}/next", msg);
  }

  if (urlrequest == "Back") {

    snprintf (msg, MSG_BUFFER_SIZE, "%ld", "prev");
    digitalWrite(2, HIGH);
    Serial.print("Publish message: ");
    Serial.println(msg);
    client.publish("falcon/player/ShowPlayer/set/playlist/${PLAYLISTNAME}/prev", msg);
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

  if ((char)payload[0] == '1') {
    digitalWrite(2, LOW);   // Turn the LED on if an 1 was received as first character
  } else {
    digitalWrite(2, HIGH);  // Turn the LED off by making the voltage HIGH
  }

}

void reconnect() {

  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (client.connect(HOSTNAME, MQTT_USERNAME, MQTT_KEY)) {
      Serial.println("connected");

    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}
