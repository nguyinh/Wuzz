#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <FastLED.h>
#define NUM_LEDS 6
#define LED_PIN D2

CRGB leds[NUM_LEDS];
WiFiUDP Udp;

// ENUMS & STRUCTS
enum masterState {
  BOOTING,
  PAIRING,
  STEADY,
  ROUND,
  PAUSE
};

enum ledState {
  BLUE_BREATHING,
  ORANGE_STEADY,
  PURPLE_STEADY,
  OFF
};

typedef struct {
  String IP = "";
  String ID = "";
} Slave;

// STATE & CONTEXT
char state = BOOTING;

const char SLAVES_SIZE = 5;
Slave slaves[SLAVES_SIZE];
String slaveLeaderIP = "";

unsigned short localUDPPort = 7777;
char incomingPacket[255];

bool steadyOnce = true;
bool leadOnce = true;



// TIMERS
unsigned long pingTime = 0;
unsigned long D1PushTime = 0;
unsigned long LEDTime = 0;

// LEDS
uint8_t blueColor = 0;
boolean blueBreathFlow = true;

// BUTTONS
boolean D1state = false;

// PROTOTYPES
void setColor(uint8_t);
void animateLed(char, unsigned long);
bool onButtonUp(uint8_t);
char* OnMasterReceive();


void setup() {
  delay(2000);
  FastLED.addLeds<WS2812B, LED_PIN, RGB>(leds, NUM_LEDS);
  pinMode(D1, INPUT);

  Serial.begin(9600);
  Serial.println();

  Serial.println("[Setup Access point ...]");
  WiFi.mode(WIFI_AP);
  if (WiFi.softAP("Wuzz-master", "thisisasecret")) {
    Serial.print(".");
  }

  Serial.println();
  Serial.println("[Access point ready]");
  Serial.println(WiFi.softAPIP());

  Udp.begin(localUDPPort);
  Serial.print("[Listening at ");
  Serial.print(WiFi.softAPIP());
  Serial.print(":");
  Serial.print(localUDPPort);
  Serial.println("]");

  state = PAIRING;
  LEDTime = millis();   // Init LED delay timer
}

void loop() {
  switch(state) {
    case PAIRING:
      if (Udp.parsePacket()) {
        int len = Udp.read(incomingPacket, 255);
        if (len > 0)
          incomingPacket[len] = 0;
        Serial.printf("[%s] %s\n", Udp.remoteIP().toString().c_str(), incomingPacket);
        if (strcmp(incomingPacket, "Connected") == 0) {
          for (int s=0; s<SLAVES_SIZE ; s++) {
            if (slaves[s].IP == "") {
              slaves[s].IP = Udp.remoteIP().toString().c_str(),
              // slaves[s].ID = generateID();

              pingTime = millis();
              break;
            }
          }
        }
      }

      // TODO: Slave ONDISCONNECT if PING don't work
      // for (int s=0; s<SLAVES_SIZE ; s++) {
      //   if (slaves[s].IP == "slaveDisconnected.IP") {   // TODO: REMOVE QUOTES ""
      //     // Shift all remaining slaves
      //     for (int c=s; c<SLAVES_SIZE-1 ; c++) {
      //       slaves[c].IP = slaves[c+1].IP;
      //       slaves[c].ID = slaves[c+1].ID;
      //     }
      //     // Empty last slave
      //     slaves[4].IP = "";
      //     slaves[4].ID = "";
      //     break;
      //   }
      // }

      // TODO: PING slaves
      // if (pingTime != 0 && millis() - pingTime > 1000) {
      //   pingTime = millis();
      //   for (int s=0; s<SLAVES_SIZE ; s++) {
      //     if (slaves[s].IP != "") {
      //       Serial.println("Slaves content : " + slaves[s].IP);
      //     }
      //   }
      // }

      animateLed(BLUE_BREATHING, 5);

      if (onButtonUp(D1)) {
        state = STEADY;
      }

      break;

    case STEADY:
      if (steadyOnce)
        for (int s=0; s<SLAVES_SIZE ; s++) {
          if (slaves[s].IP != "") {
            Udp.beginPacket(slaves[s].IP.c_str(), 7778);
            Udp.write("Steady");
            Udp.endPacket();
          }
          else {
            steadyOnce = false;
            break;
          }
        }

      animateLed(ORANGE_STEADY, 0);



      if (onButtonUp(D1)) {
        for (int s=0; s<SLAVES_SIZE ; s++) {
          if (slaves[s].IP != "") {
            // TODO: watch for multisend
            Udp.beginPacket(slaves[s].IP.c_str(), 7778);
            Udp.write("Round");
            Udp.endPacket();
          }
          else {
            animateLed(OFF, 0);
            state = ROUND;
          }
        }
      }
      break;

    case ROUND:

      if (Udp.parsePacket()) {
        int len = Udp.read(incomingPacket, 255);
        if (len > 0)
          incomingPacket[len] = 0;
        // If packet received is 'Wuzz'
        if (strcmp(incomingPacket, "Wuzz") == 0) {
          // Give Lead to player who pushed
          Udp.beginPacket(Udp.remoteIP(), 7778);
          Udp.write("Lead");
          Udp.endPacket();
          slaveLeaderIP = Udp.remoteIP().toString();

          // Pause other players
          for (int s=0; s<SLAVES_SIZE ; s++) {
            if (slaves[s].IP != Udp.remoteIP().toString() && slaves[s].IP != "") {
              Udp.beginPacket(slaves[s].IP.c_str(), 7778);
              Udp.write("Pause");
              Udp.endPacket();
            }
          }

          state = PAUSE;
        }
      }
      break;

    case PAUSE:

      animateLed(PURPLE_STEADY, 0);

      // if (onButtonUp(D1)) {
      //   if (leadOnce) {
      //     Udp.beginPacket("192.168.4.2", 7778);
      //     Udp.write("Lead");
      //     Udp.endPacket();
      //     leadOnce = false;
      //     animateLed(PURPLE_STEADY, 0);
      //   }
      // }

      // Choose if leader win / lose / exclude ...

      break;

    default: break;
  }
}

void animateLed(char ledState, unsigned long pauseTime) {
  switch(ledState) {
    case BLUE_BREATHING:
      if (millis() - LEDTime >= pauseTime) {
        LEDTime = millis();
        if (blueBreathFlow) {
          for (int i = 0; i < NUM_LEDS; i++) {
            leds[i] = CRGB(0, 0, blueColor++);
            FastLED.show();
          }
          if (blueColor >= 100)
            blueBreathFlow = false;
        }
        else {
          for (int i = 0; i < NUM_LEDS; i++) {
            leds[i] = CRGB(0, 0, blueColor--);
            FastLED.show();
          }
          if (blueColor <= 0)
            blueBreathFlow = true;
        }
      }
      break;

    case ORANGE_STEADY:
      for (int i = 0; i < NUM_LEDS; i++) {
        leds[i] = CRGB(25, 75, 0);
        FastLED.show();
      }
      break;
    case PURPLE_STEADY:
      for (int i = 0; i < NUM_LEDS; i++) {
        leds[i] = CRGB(0, 50, 50);
        FastLED.show();
      }
      break;
    case OFF:
      for (int i = 0; i < NUM_LEDS; i++) {
        leds[i] = CRGB::Black;
        FastLED.show();
      }
      break;
    default: break;
  }
}

bool onButtonUp(uint8_t pin) {
  switch(pin) {
    case D1:
      if (digitalRead(D1)) {
        if (!D1state) {
          // D1PushTime = millis();
          Serial.println("Push D1");
          D1state = true;
        }
      }


      if (D1state && !digitalRead(D1)) {
        D1state = false;
        // D1PushTime = 0;
        Serial.println("Depush D1");
        return true;
      }

      return false;
      break;
    default: break;
  }
  return false;
}

char* OnSlaveReceive() {
  if (Udp.parsePacket()) {
    int len = Udp.read(incomingPacket, 255);
    if (len > 0)
      incomingPacket[len] = 0;
    return incomingPacket;
  }
  else
    return " ";
}
