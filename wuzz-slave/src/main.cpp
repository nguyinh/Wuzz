#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <FastLED.h>
#define NUM_LEDS 6
#define LED_PIN D2

CRGB leds[NUM_LEDS];
WiFiUDP Udp;
char incomingPacket[255];
int n = 0;

enum slaveState {
  BOOTING,
  PAIRING,
  PAIRED,
  STEADY,
  ROUND,
  LEAD,
  EXCLUDED,
  PAUSED,
  WIN,
  LOSE
};

enum ledState {
  BLUE_STEADY,
  BLUE_BREATHING,
  ORANGE_STEADY,
  GREEN_PING,
  BLUE_ROTATING,
  PURPLE_STEADY,
  GREEN_BREATHING,
  RED_BREATHING,
  RED_ALTERNATING
};


// STATE & CONTEXT
char state = BOOTING;
bool D1state = false;

// TIMERS
unsigned long LEDTime = 0;
unsigned long resetWait = 0;

// LEDS
uint8_t blueColor = 0;
boolean blueBreathFlow = true;
uint8_t greenColor = 0;
boolean greenBreathFlow = true;
uint8_t redColor = 0;
boolean redBreathFlow = true;

// PROTOTYPES
void animateLed(char, unsigned long);
char* OnMasterReceive();
bool onButtonDown(uint8_t);


void setup() {
  delay(2000);
  FastLED.addLeds<WS2812B, LED_PIN, RGB>(leds, NUM_LEDS);
  pinMode(D1, INPUT);

  state = PAIRING;
  Serial.begin(9600);
  Serial.println();

  WiFi.mode(WIFI_STA);
  Serial.println("[Connecting to Access point ...]");

  WiFi.begin("Wuzz-master", "thisisasecret");
  while (WiFi.status() != WL_CONNECTED) {
    animateLed(BLUE_STEADY, 0);
    delay(500);
  }

  state = PAIRED;

  Serial.println();
  Serial.println("[Connected to Wuzz-master]");
  Serial.println(WiFi.localIP());

  Udp.begin(7778);

  Udp.beginPacket("192.168.4.1", 7777);
  Udp.write("Connected");
  Udp.endPacket();

  LEDTime = millis();

  animateLed(GREEN_PING, 2000);
}

void loop() {
  switch(state) {
    case PAIRED:
      if (millis() - LEDTime > 2000)
        animateLed(GREEN_PING, 50);

      if (strcmp(OnMasterReceive(), "Steady") == 0)
        state = STEADY;

      break;

    case STEADY:
      animateLed(ORANGE_STEADY, 0);

      if (strcmp(OnMasterReceive(), "Round") == 0)
        state = ROUND;

      break;
    case ROUND:

      // If dome button push, send message
      if (onButtonDown(D1)) {
        Udp.beginPacket("192.168.4.1", 7777);
        Udp.write("Wuzz");
        Udp.endPacket();
      }

      char roundResp[7];
      strncpy(roundResp, OnMasterReceive(), 7);
      if (strcmp(roundResp, "Lead") == 0)
        state = LEAD;
      else if (strcmp(roundResp, "Pause") == 0)
        state = PAUSED;
      else if (strcmp(roundResp, "Steady") == 0)
        state = STEADY;

      animateLed(BLUE_STEADY, 0);

      // if (Udp.parsePacket()) {
      //   int len = Udp.read(incomingPacket, 255);
      //   if (len > 0)
      //     incomingPacket[len] = 0;
      //   Serial.printf("[%s] %s\n", Udp.remoteIP().toString().c_str(), incomingPacket);
      //   if (strcmp(incomingPacket, "Lead") == 0) {
      //     state = LEAD;
      //   }
      //   else if (strcmp(incomingPacket, "Paused") == 0) {
      //     state = PAUSED;
      //   }
      // }

      break;
    case LEAD:

      char leadResp[9];
      strncpy(leadResp, OnMasterReceive(), 9);

      if (strcmp(leadResp, "Win") == 0) {
        resetWait = millis(); //TODO: changename
        state = WIN;
      }
      else if (strcmp(leadResp, "Lose") == 0) {
        state = LOSE;
      }
      else if (strcmp(leadResp, "Excluded") == 0) {
        state = EXCLUDED;
      }

      animateLed(PURPLE_STEADY, 0);

      break;

    case EXCLUDED:

      if (strcmp(OnMasterReceive(), "Steady") == 0)
        state = STEADY;   // Or ROUND

      animateLed(RED_ALTERNATING, 0);

      break;

    case PAUSED:

      char pauseResp[9];
      strncpy(pauseResp, OnMasterReceive(), 9);
      if (strcmp(pauseResp, "Steady") == 0)
        state = STEADY;
      else if (strcmp(pauseResp, "Round") == 0)   // TODO: remove ROUND ? -> bad UX
        state = ROUND;

      break;

    case WIN:

      // if (resetWait != 0 && millis() - resetWait >= 2000) {
      //   Udp.beginPacket("192.168.4.1", 7777);
      //   Udp.write("WinEnd");
      //   Udp.endPacket();
      //
      //   resetWait = 0;
      // }

      if (strcmp(OnMasterReceive(), "Steady") == 0)
        state = STEADY;   // Or ROUND

      animateLed(GREEN_BREATHING, 5);

      break;

    case LOSE:

      char loseResp[9];
      strncpy(loseResp, OnMasterReceive(), 9);
      if (strcmp(loseResp, "Exclude") == 0)
        state = EXCLUDED;
      else if (strcmp(loseResp, "Steady") == 0)
        state = STEADY;

      animateLed(RED_BREATHING, 5);

      break;

    default: break;
  }
}


void animateLed(char ledState, unsigned long pauseTime) {
  switch(ledState) {
    case BLUE_STEADY:
      for (int i = 0; i < NUM_LEDS; i++) {
        leds[i] = CRGB(0, 0, 50);
        FastLED.show();
      }
      break;
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
    case BLUE_ROTATING:
      for (int i = 0; i < NUM_LEDS; i++) {
        leds[i] = CRGB(0, 0, 50);
        FastLED.show();
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
    case GREEN_PING:
      if (millis() - LEDTime < pauseTime) {
        for (int i = 0; i < NUM_LEDS; i++) {
          leds[i] = CRGB(50, 0, 0);
          FastLED.show();
        }
      }
      else {
        for (int i = 0; i < NUM_LEDS; i++) {
          leds[i] = CRGB::Black;
          FastLED.show();
        }
      }
      break;
    case GREEN_BREATHING:
      if (millis() - LEDTime >= pauseTime) {
        LEDTime = millis();
        if (greenBreathFlow) {
          for (int i = 0; i < NUM_LEDS; i++) {
            leds[i] = CRGB(greenColor++, 0, 0);
            FastLED.show();
          }
          if (greenColor >= 100)
            greenBreathFlow = false;
        }
        else {
          for (int i = 0; i < NUM_LEDS; i++) {
            leds[i] = CRGB(greenColor--, 0, 0);
            FastLED.show();
          }
          if (greenColor <= 0)
            greenBreathFlow = true;
        }
      }
      break;

    case RED_BREATHING:
      if (millis() - LEDTime >= pauseTime) {
        LEDTime = millis();
        if (redBreathFlow) {
          for (int i = 0; i < NUM_LEDS; i++) {
            leds[i] = CRGB(0, redColor++, 0);
            FastLED.show();
          }
          if (redColor >= 100)
            redBreathFlow = false;
        }
        else {
          for (int i = 0; i < NUM_LEDS; i++) {
            leds[i] = CRGB(0, redColor--, 0);
            FastLED.show();
          }
          if (redColor <= 0)
            redBreathFlow = true;
        }
      }
      break;

      case RED_ALTERNATING:
        for (int i = 0; i < NUM_LEDS; i++) {
          if (i%2 ==0 )
            leds[i] = CRGB(0, 50, 0);
          else
            leds[i] = CRGB::Black;
          FastLED.show();
        }

      break;

    default: break;
  }
}

char* OnMasterReceive() {
  if (Udp.parsePacket()) {
    int len = Udp.read(incomingPacket, 255);
    if (len > 0)
      incomingPacket[len] = 0;
    return incomingPacket;
  }
  else
    return " ";
}

bool onButtonDown(uint8_t pin) {
  switch(pin) {
    case D1:
      // On button down
      if (digitalRead(D1)) {
        if (!D1state) {
          D1state = true;
          // D1PushTime = millis();
          return true;
        }
      }

      // On button up
      if (D1state && !digitalRead(D1)) {
        D1state = false;
        // D1PushTime = 0;
      }

      break;
    default: break;
  }
  return false;
}
