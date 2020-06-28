#include <Arduino.h>
#include <ArduinoOTA.h>
#include <RemoteDebug.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <FastLED.h>
#include <SPI.h>
#include <String.h>
#define NUM_LEDS 6
#define LED_PIN D2

CRGB leds[NUM_LEDS];
WiFiUDP Udp;
// RemoteDebug Debug;
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
  GREEN_ROUND,
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
unsigned int greenRoundPosition = NUM_LEDS * 2;
boolean greenRoundState = false;

// BUTTONS
boolean D1state = false;
#define D1PIN D1
boolean D6state = false;
#define D6PIN D5

// PROTOTYPES
void setColor(uint8_t);
boolean animateLed(char, unsigned long);
bool onButtonUp(uint8_t);
char* OnSlaveReceive();
void flushPackets();
void onStationConnected(const WiFiEventSoftAPModeStationConnected&);
void onStationDisconnected(const WiFiEventSoftAPModeStationDisconnected&);
void sendState(unsigned char, int);
void doGreenRound();

// DEBUG
bool doOnce = true;

void setup() {
  delay(2000);
  FastLED.addLeds<WS2812B, LED_PIN, RGB>(leds, NUM_LEDS);
  pinMode(D1PIN, INPUT);
  pinMode(D6PIN, INPUT);



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

  ArduinoOTA.setHostname("espmaster");
  ArduinoOTA.begin();
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
          doGreenRound();
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

      
      if (!animateLed(GREEN_ROUND, 100))
        animateLed(BLUE_BREATHING, 20);
      

      if (onButtonUp(D1PIN)) {
        for (int s=0; s<SLAVES_SIZE ; s++) {
          if (slaves[s].IP != "") {
            sendState(ROUND, s);
          }
          else {
            steadyOnce = false;
            break;
          }
        }

        state = ROUND;
      }

      break;

    // DEPRECATED
    case STEADY:

      animateLed(ORANGE_STEADY, 0);

      if (onButtonUp(D1PIN)) {
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

      animateLed(OFF, 0);

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
      if (onButtonUp(D1PIN)) {
        for (int s=0; s<SLAVES_SIZE; s++) {
          Serial.print("index: ");
          Serial.print(s);
          Serial.print("   state: ");
          Serial.println(slaves[s].state);
          if (slaves[s].state == ROUND && slaves[s].IP != "") {
            sendState(ROUND, s);
          }
        }

        state = ROUND;
      }

      break;

    case PAUSE:

      animateLed(PURPLE_STEADY, 0);

      // On Lead win
      if (onButtonUp(D1PIN)) {
        // Send Win signal to Lead player
        sendState(ON_WIN, leader.index);
        // Add 1 point to Lead

        unsigned long resetWait = millis();
        while (millis() - resetWait < 2000) {
          yield();
        }

        for (int s=0; s<SLAVES_SIZE; s++) {
          if (slaves[s].IP != "") {
            sendState(ROUND, s);
          }
        }

        // Prevent concurrency Wuzz (happen on same slaves buzzer timing)
        flushPackets();

        state = ROUND;
      }
      // On Lead lose
      else if (onButtonUp(D6PIN)) {
        // Send Lose signal to Lead playert
        sendState(ON_LOSE, leader.index);
        // Remove 1 point to Lead or do nothing

        unsigned long resetWait = millis();
        while (millis() - resetWait < 2000) {
          yield();
        }

        for (int s=0; s<SLAVES_SIZE; s++) {
          if (slaves[s].IP != "") {
            sendState(ROUND, s);
          }
        }

        // Prevent concurrency Wuzz (happen on same slaves buzzer timing)
        flushPackets();

        state = ROUND;
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

      if (onButtonUp(D6PIN)) {
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
      else if (onButtonUp(D1PIN)) {
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

  // Handle OTA updates
  ArduinoOTA.handle();

  // Handle OTA debug
  // Debug.handle();
}

boolean animateLed(char ledState, unsigned long pauseTime) {
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
    case GREEN_ROUND:
      if (greenRoundPosition >= NUM_LEDS + 2) return false;

      if (millis() - LEDTime >= pauseTime) {
        LEDTime = millis();
        for (unsigned int i = 0; i < NUM_LEDS; i++) {
          if (i == greenRoundPosition - 1) leds[i] = CRGB(5, 0, 0);
          else if (i == greenRoundPosition) leds[i] = CRGB(50, 0, 0);
          else if (i == greenRoundPosition + 1) leds[i] = CRGB(5, 0, 0);
          else leds[i] = CRGB::Black;
        }
        greenRoundPosition++;
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
  
  return true;
}

void doGreenRound() {
  greenRoundPosition = 0;
}

bool onButtonUp(uint8_t pin) {
  switch(pin) {
    case D1PIN:
      // On button down
      if (!digitalRead(D1PIN)) {
        if (!D1state) {
          // D1PushTime = millis();
          D1state = true;
        }
      }

      // On button up
      if (D1state && digitalRead(D1PIN)) {
        D1state = false;
        // D1PushTime = 0;
        return true;
      }

      return false;
      break;

    case D6PIN:
      // On button down
      if (!digitalRead(D6PIN)) {
        if (!D6state) {
          // D6PushTime = millis();
          D6state = true;
        }
      }

      // On button up
      if (D6state && digitalRead(D6PIN)) {
        D6state = false;
        // D6PushTime = 0;
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

void flushPackets() {
  while (Udp.parsePacket())
    Udp.read(incomingPacket, 255);
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
