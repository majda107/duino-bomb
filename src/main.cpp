#include <Arduino.h>
#include <math.h>
#include <FastLED.h>

// const double PI = 3.14159;
#define LED_PIN   5
#define PIEZO_PIN 6
#define SETUP_PIN 2

#define RED_CABLE 9
#define GREEN_CABLE 8

#define NUM_LEDS    24
#define BRIGHTNESS  255
#define LED_TYPE    WS2811
#define COLOR_ORDER GRB
CRGB leds[NUM_LEDS];

#define UPDATES_PER_SECOND 100



// CUSTOM PROPS
const int DISARM_ELAPSED = NUM_LEDS * 4 + 16;
// END CUSTOM PROPS

// void drawLoop();


// int loopcnt = 0;

enum BombState {
  Setup,
  Disarmed,
  Armed,

  Defused,
  Fired 
};

struct Vector3 {
  byte x;
  byte y;
  byte z;

  Vector3(byte r, byte g, byte b)
  {
    x = r;
    y = g;
    z = b;
  }
};

class Timer {
private:
  unsigned long m_lastMillis = 0;

public:
  unsigned long duration;
  bool ready = false;

  Timer(int duration) {
    this->duration = duration;
  }

  void tick() {
    auto mls = millis();

    if(mls - this->m_lastMillis >= this->duration) {
      this->ready = true;
      this->m_lastMillis = mls;
    }
  }

  void reset() {
    this->ready = false;
    this->m_lastMillis = millis();
  }
};

Vector3 hue(float angle) {
  auto third = PI / 3.0;

  byte r = sin(angle) * 127.0 + 128.0;
  byte g = sin(angle + 2.0*third) * 127.0 + 128.0;
  byte b = sin(angle + 4.0*third) * 127.0 + 128.0;

  return Vector3(r, g, b);
}


// GLOBAL STATE
BombState state;

// SETUP STATE
byte setupBrightness = 122;

// COMMON STATE (DISARM + ARM)
Timer commonTimer(1000 / (NUM_LEDS + 1));
byte ledInLapCount = 0;
byte commonPosition = 0;
unsigned long commonElapsed = DISARM_ELAPSED;

void setup() {

  delay( 3000 );

  state = BombState::Setup;

  FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection( TypicalLEDStrip );
  FastLED.setBrightness(  BRIGHTNESS );

  pinMode(PIEZO_PIN, OUTPUT);
  pinMode(SETUP_PIN, INPUT_PULLUP);

  pinMode(RED_CABLE, INPUT_PULLUP);
  pinMode(GREEN_CABLE, INPUT_PULLUP);

  Serial.begin(9600);
}

void renderArm() {
      for(int i = 0; i < NUM_LEDS; i++) {
        // elapsed progress
        if(i < commonElapsed % NUM_LEDS) {
          leds[i] = CRGB(20, 20, 0);
        } else {
          leds[i] = CRGB(0, 0, 0);
        }

        // elapsed minutes progress
        if(i < (commonElapsed / NUM_LEDS)) {
          leds[i] = CRGB(255, 0, 0);
        }

        // one orange (red) overlay color
        if(i - (commonElapsed % NUM_LEDS) == 0) {
          leds[i] = CRGB(20, 20, 0);
        }

        // current pos
        if(i == commonPosition) {
          leds[i] = CRGB(255, 255, 255);
        }
      }

      commonPosition++;
      if(commonPosition >= NUM_LEDS)
        commonPosition = 0;
}

void renderDefused() {
  for(int i = 0; i < NUM_LEDS; i++) {
    leds[i] = CRGB(0, 255, 0);
  }
}

void renderFired() {
  for(int i = 0; i < NUM_LEDS; i++) {
    leds[i] = CRGB(255, 0, 0);
  }
}


void loop() {

  switch(state) {
    // HUE TMP

      // for(int i = 0; i < NUM_LEDS; i++) {
      //   int distance = (abs(i - setupPosition)) * 10;

      //   auto color = hue(i * (360 / NUM_LEDS) * PI / 180);
      //   leds[i] = CRGB((double)color.x / distance, (double)color.y / distance, (double)color.z / distance);
      // }


      // setupPosition++;
      // if(setupPosition >= NUM_LEDS)
        // setupPosition = 0;

      // FastLED.setBrightness(map(val * 255, 0, 255, 20, 255));

    case BombState::Setup: 
    {
      auto val = (sin(PI + (double)millis() / 500.0) + 1) / 2;
      auto valMap = map(val * 255, 0, 255, 10, 255);

      for(int i = 0; i < NUM_LEDS; i++) {
        leds[i] = CRGB(10, 10, valMap);
      }

      FastLED.delay(1000 / NUM_LEDS);
      FastLED.show();

      break;
    }
    case BombState::Disarmed:
    case BombState::Armed:
    {
      commonTimer.tick();

      if(commonTimer.ready) {
        if(ledInLapCount % 25 == 0) {
          tone(PIEZO_PIN, 1000);

          ledInLapCount = 0;

          if(state == BombState::Armed) {
            commonElapsed += 1;
          }
        } else {
          noTone(PIEZO_PIN);

          renderArm();
          FastLED.show();
        }
        
        ledInLapCount++;
        commonTimer.reset();
      }

      break;
    }
    
    case BombState::Defused:
    {
      renderDefused();
      FastLED.show();
      break;
    }

    case BombState::Fired:
    {
      renderFired();
      FastLED.show();
      break;
    }
  }

  if(digitalRead(SETUP_PIN) == LOW && state == BombState::Setup) {
    Serial.println("DISARMING!");
    state = BombState::Disarmed;
  }

  if(digitalRead(SETUP_PIN) == HIGH && state == BombState::Disarmed) {
    state = BombState::Armed;
  }


  // TODO check all cables
  if(digitalRead(RED_CABLE) == HIGH && (state == BombState::Armed || state == BombState::Disarmed)) {
    state= BombState::Fired;
  }

  if(digitalRead(GREEN_CABLE) == HIGH && (state == BombState::Armed || state == BombState::Disarmed)) {
    state = BombState::Defused;
  }
}

// int position = 0;
// void drawLoop()
// {
//  position++;
//  if(position > NUM_LEDS -1)
//   position = 0;
//   for (int i = 0; i < NUM_LEDS ; i++)
//   {
//     leds[i] = CRGB(0,0,0);
//     if(i == position)
//       leds[i] = CRGB(50,50,50);
//   }

// }