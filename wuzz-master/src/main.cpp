#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <FastLED.h>
#include <String.h>
#define NUM_LEDS 6
#define LED_PIN D2

CRGB leds[NUM_LEDS];
WiFiUDP Udp;
WiFiEventHandler stationConnectedHandler;
WiFiEventHandler stationDisconnectedHandler;

// ENUMS & STRUCTS
enum masterState {
  BOOTING,
  PAIRING,
  STEADY,
  ROUND,
  LEAD,
  PAUSE,
  ON_WIN,
  ON_LOSE,
  EXCLUDE
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
  unsigned char Score = 0;
  unsigned char state = BOOTING;
} Slave;

typedef struct {
  int index = -1;
  String IP = "";
} Leader;

// STATE & CONTEXT
char state = BOOTING;

const char SLAVES_SIZE = 5;
Slave slaves[SLAVES_SIZE];
Slave pingedSlaves[SLAVES_SIZE];
String slaveLeaderIP = "";
Leader leader;

unsigned short localUDPPort = 7777;
char incomingPacket[255];

bool steadyOnce = true;
bool leadOnce = true;



// TIMERS
unsigned long pingTime = 0;
unsigned long ExpirePingTime = 0;
unsigned long D1PushTime = 0;
unsigned long LEDTime = 0;

// LEDS
uint8_t blueColor = 0;
boolean blueBreathFlow = true;

// BUTTONS
boolean D1state = false;
boolean D7state = false;

// PROTOTYPES
void setColor(uint8_t);
void animateLed(char, unsigned long);
bool onButtonUp(uint8_t);
char* OnSlaveReceive();
void onStationConnected(const WiFiEventSoftAPModeStationConnected&);
void onStationDisconnected(const WiFiEventSoftAPModeStationDisconnected&);
void sendState(unsigned char, int);

void setup() {
  delay(2000);
  FastLED.addLeds<WS2812B, LED_PIN, RGB>(leds, NUM_LEDS);
  pinMode(D1, INPUT);
  pinMode(D7, INPUT);

  Serial.begin(9600);
  Serial.println();

  Serial.println("[Setup Access point ...]");
  WiFi.mode(WIFI_AP);
  if (WiFi.softAP("Wuzz-master", "thisisasecret")) {
    Serial.print(".");
  }

  stationConnectedHandler = WiFi.onSoftAPModeStationConnected(&onStationConnected);
  stationDisconnectedHandler = WiFi.onSoftAPModeStationDisconnected(&onStationDisconnected);

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

void onStationConnected(const WiFiEventSoftAPModeStationConnected& evt) {
  Serial.print("Station connected: ");
  // Serial.println(evt.mac);
}

void onStationDisconnected(const WiFiEventSoftAPModeStationDisconnected& evt) {
  Serial.print("Station disconnected: ");
  // Serial.println(macToString(evt.mac));
}

void loop() {
  switch(state) {
    case PAIRING:
      // Get connected Slave
      if (Udp.parsePacket()) {
        int len = Udp.read(incomingPacket, 255);
        if (len > 0)
          incomingPacket[len] = 0;
        Serial.printf("[%s] %s\n", Udp.remoteIP().toString().c_str(), incomingPacket);
        if (strcmp(incomingPacket, "Connected") == 0) {
          for (int s=0; s<SLAVES_SIZE ; s++) {
            if (slaves[s].IP == "") {
              slaves[s].IP = Udp.remoteIP().toString().c_str();
              // slaves[s].ID = generateID();

              // pingTime = millis();
              break;
            }
          }
        }
      }


      // Store temporarily each Slave who answered to Ping
      // if (pingTime != 0 && Udp.parsePacket() && millis() - pingTime < 1000) {
      //   int len = Udp.read(incomingPacket, 255);
      //   if (len > 0)
      //     incomingPacket[len] = 0;
      //   Serial.printf("[%s] %s\n", Udp.remoteIP().toString().c_str(), incomingPacket);
      //   if (strcmp(incomingPacket, "Pong") == 0) {
      //     for (int p=0; p<SLAVES_SIZE ; p++) {
      //       if (pingedSlaves[p].IP == "") {
      //         pingedSlaves[p].IP = Udp.remoteIP().toString().c_str(),
      //         // slaves[p].ID = generateID();
      //
      //         pingTime = millis();
      //         break;
      //       }
      //     }
      //   }
      // }
      // else if (pingTime != 0 && millis() - pingTime >= 1000) {
      //   pingTime = 0;
      //
      //   // Find all slaves which ponged
      //   for (int s=0; s<SLAVES_SIZE ; s++) {
      //     int slaveIndex = -1;
      //     for (int p=0; p<SLAVES_SIZE ; p++) {
      //       if (slaves[s].IP == pingedSlaves[p].IP) {
      //         slaveIndex = s;
      //         // slaves[s].ID = generateID();
      //         break;
      //       }
      //     }
      //
      //     // If a slave didn't answered to ping, remove it
      //     if (slaveIndex != -1) {
      //       if (s == slaveIndex) {
      //         // Remove slave & shift all remaining slaves
      //         for (int c=slaveIndex; c<SLAVES_SIZE-1 ; c++) {
      //           slaves[c].IP = slaves[c+1].IP;
      //           slaves[c].ID = slaves[c+1].ID;
      //
      //           if (slaves[c+1].IP == "")
      //             break;
      //         }
      //         // Empty last slave
      //         slaves[SLAVES_SIZE-1].IP = "";
      //         slaves[SLAVES_SIZE-1].ID = "";
      //         break;
      //       }
      //     }
      //   }
      // }
      // else if (pingTime == 0) {
      //   // Ping saved slaves
      //   for (int s=0; s<SLAVES_SIZE ; s++) {
      //     if (slaves[s].IP != "") {
      //       Udp.beginPacket(slaves[s].IP.c_str(), 7778);
      //       Udp.write("Ping");
      //       Udp.endPacket();
      //     }
      //     else {
      //       pingTime = millis();
      //       break;
      //     }
      //   }
      // }


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
        for (int s=0; s<SLAVES_SIZE ; s++) {
          if (slaves[s].IP != "") {
            sendState(STEADY, s);
          }
          else {
            steadyOnce = false;
            break;
          }
        }

        state = STEADY;
      }

      break;

    case STEADY:

      animateLed(ORANGE_STEADY, 0);

      if (onButtonUp(D1)) {
        for (int s=0; s<SLAVES_SIZE ; s++) {
          if (slaves[s].state == STEADY && slaves[s].IP != "") {
            // TODO: watch for multisend
            sendState(ROUND, s);
          }
          else {
            animateLed(OFF, 0);
            state = ROUND;
          }
        }
      }

      // TODO: Add a reset for all players (excluded ones)
      break;

    case ROUND:

      if (strcmp(OnSlaveReceive(), "Wuzz") == 0) {
        Udp.beginPacket(Udp.remoteIP(), 7778);
        Udp.write("Lead");
        Udp.endPacket();
        leader.IP = Udp.remoteIP().toString();

        // Pause other players
        for (int s=0; s<SLAVES_SIZE; s++) {
          if (slaves[s].IP != Udp.remoteIP().toString() && slaves[s].IP != "") {
            sendState(PAUSE, s);
          }
          else if (slaves[s].IP == Udp.remoteIP().toString()) {
            leader.index = s;
            slaves[s].state = LEAD;
          }
        }

        state = PAUSE;
      }

      // Back to Steady if needed
      if (onButtonUp(D1)) {
        for (int s=0; s<SLAVES_SIZE; s++) {
          Serial.print("index: ");
          Serial.print(s);
          Serial.print("   state: ");
          Serial.println(slaves[s].state);
          if (slaves[s].state == ROUND && slaves[s].IP != "") {
            sendState(STEADY, s);
          }
        }

        state = STEADY;
      }

      break;

    case PAUSE:

      animateLed(PURPLE_STEADY, 0);

      // On Lead win
      if (onButtonUp(D1)) {
        // Send Win signal to Lead player
        sendState(ON_WIN, leader.index);
        // Add 1 point to Lead

        unsigned long resetWait = millis();
        while (millis() - resetWait < 2000) {
          yield();
        }

        for (int s=0; s<SLAVES_SIZE; s++) {
          if (slaves[s].IP != "") {
            sendState(STEADY, s);
          }
        }

        state = STEADY;
      }
      // On Lead lose
      else if (onButtonUp(D7)) {
        // Send Lose signal to Lead player
        sendState(ON_LOSE, leader.index);
        // Remove 1 point to Lead or do nothing

        state = ON_LOSE;
      }

      break;

    case ON_WIN:

      // if (strcmp(OnSlaveReceive(), "WinEnd") == 0) {
      //   // Reset other players
      //   for (int s=0; s<SLAVES_SIZE; s++) {
      //     if (slaves[s].IP != "") {
      //       sendState(STEADY, s);
      //       // Udp.beginPacket(slaves[s].IP.c_str(), 7778);
      //       // Udp.write("Steady");
      //       // Udp.endPacket();
      //     }
      //   }
      //
      //   state = STEADY;
      // }

      break;

    case ON_LOSE:

      animateLed(ORANGE_STEADY, 0);

      if (onButtonUp(D1)) {
        // Exclude Lead player
        sendState(EXCLUDE, leader.index);

        // Re-launch round for other players
        for (int s=0; s<SLAVES_SIZE; s++) {
          if (s != leader.index && slaves[s].IP != "") {
            sendState(STEADY, s);
          }
        }

        animateLed(OFF, 0);
        state = STEADY;
      }
      else if (onButtonUp(D7)) {
        // Reset round for all players
        for (int s=0; s<SLAVES_SIZE; s++) {
          if (slaves[s].IP != "") {
            sendState(STEADY, s);
          }
          else
            break;
        }
        state = STEADY;
      }

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
      // On button down
      if (digitalRead(D1)) {
        if (!D1state) {
          // D1PushTime = millis();
          D1state = true;
        }
      }

      // On button up
      if (D1state && !digitalRead(D1)) {
        D1state = false;
        // D1PushTime = 0;
        return true;
      }

      return false;
      break;

    case D7:
      // On button down
      if (digitalRead(D7)) {
        if (!D7state) {
          // D7PushTime = millis();
          D7state = true;
        }
      }

      // On button up
      if (D7state && !digitalRead(D7)) {
        D7state = false;
        // D7PushTime = 0;
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

void sendState(unsigned char newState, int index) {
  slaves[index].state = newState;
  switch(newState) {
    case STEADY:
      Udp.beginPacket(slaves[index].IP.c_str(), 7778);
      Udp.write("Steady");
      Udp.endPacket();
      break;
    case ROUND:
      Udp.beginPacket(slaves[index].IP.c_str(), 7778);
      Udp.write("Round");
      Udp.endPacket();
      break;
    case LEAD:
      Udp.beginPacket(slaves[index].IP.c_str(), 7778);
      Udp.write("Lead");
      Udp.endPacket();
      break;
    case PAUSE:
      Udp.beginPacket(slaves[index].IP.c_str(), 7778);
      Udp.write("Pause");
      Udp.endPacket();
      break;
    case ON_WIN:
      Udp.beginPacket(slaves[index].IP.c_str(), 7778);
      Udp.write("Win");
      Udp.endPacket();
      break;
    case ON_LOSE:
      Udp.beginPacket(slaves[index].IP.c_str(), 7778);
      Udp.write("Lose");
      Udp.endPacket();
      break;
    case EXCLUDE:
      Udp.beginPacket(slaves[index].IP.c_str(), 7778);
      Udp.write("Exclude");
      Udp.endPacket();
      break;
    default:
      break;
  }
}
