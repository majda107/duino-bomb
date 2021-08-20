#include <Arduino.h>
#include <math.h>
#include <FastLED.h>

// const double PI = 3.14159;
#define LED_PIN 5
#define PIEZO_PIN 6
#define SETUP_PIN 2

#define RED_CABLE 9
#define GREEN_CABLE 8

#define NUM_LEDS 24
#define BRIGHTNESS 255
#define LED_TYPE WS2811
#define COLOR_ORDER GRB
CRGB leds[NUM_LEDS];

#define UPDATES_PER_SECOND 100

char notes[] = "GGAGcB GGAGdc GGxecBA yyecdc";
int beats[] = {2, 2, 8, 8, 8, 16, 1, 2, 2, 8, 8, 8, 16, 1, 2, 2, 8, 8, 8, 8, 16, 1, 2, 2, 8, 8, 8, 16};

// CUSTOM PROPS
const int DISARM_ELAPSED = NUM_LEDS * 4 + 16;
// END CUSTOM PROPS

// void drawLoop();

// int loopcnt = 0;

enum BombState
{
  Setup,
  Disarmed,
  Armed,

  Defused,
  Fired,

  Finished,

  Party
};

struct Vector3
{
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

class Timer
{
private:
  unsigned long m_lastMillis = 0;

public:
  unsigned long duration;
  bool ready = false;

  Timer(int duration)
  {
    this->duration = duration;
  }

  void tick()
  {
    auto mls = millis();

    if (mls - this->m_lastMillis >= this->duration)
    {
      this->ready = true;
      this->m_lastMillis = mls;
    }
  }

  void reset()
  {
    this->ready = false;
    this->m_lastMillis = millis();
  }
};

Vector3 hue(float angle)
{
  auto third = PI / 3.0;

  byte r = sin(angle) * 127.0 + 128.0;
  byte g = sin(angle + 2.0 * third) * 127.0 + 128.0;
  byte b = sin(angle + 4.0 * third) * 127.0 + 128.0;

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

// PARTY STATE
unsigned long partyAngle = 0;

void setup()
{

  delay(3000);

  state = BombState::Setup;

  FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
  FastLED.setBrightness(BRIGHTNESS);

  pinMode(PIEZO_PIN, OUTPUT);
  pinMode(SETUP_PIN, INPUT_PULLUP);

  pinMode(RED_CABLE, INPUT_PULLUP);
  pinMode(GREEN_CABLE, INPUT_PULLUP);

  Serial.begin(9600);
}

void renderArm()
{
  for (int i = 0; i < NUM_LEDS; i++)
  {
    // elapsed progress
    if (i < commonElapsed % NUM_LEDS)
    {
      leds[i] = CRGB(20, 20, 0);
    }
    else
    {
      leds[i] = CRGB(0, 0, 0);
    }

    // elapsed minutes progress
    if (i < (commonElapsed / NUM_LEDS))
    {
      leds[i] = CRGB(255, 0, 0);
    }

    // one orange (red) overlay color
    if (i - (commonElapsed % NUM_LEDS) == 0)
    {
      leds[i] = CRGB(20, 20, 0);
    }

    // current pos
    if (i == commonPosition)
    {
      leds[i] = CRGB(255, 255, 255);
    }
  }

  commonPosition++;
  if (commonPosition >= NUM_LEDS)
    commonPosition = 0;
}

void renderDefused()
{
  for (int j = 0; j < 6; j++)
  {
    for (int i = 0; i < NUM_LEDS; i++)
    {
      leds[i] = CRGB(0, 255, 0);
    }

    if (j % 2 == 0)
    {
      FastLED.setBrightness(255);
      FastLED.show();

      tone(PIEZO_PIN, 1200);
      delay(50);
    }
    else
    {
      FastLED.setBrightness(0);
      FastLED.show();

      noTone(PIEZO_PIN);
      delay(120);
    }
  }

  delay(50);

  for (int i = 255; i >= 0; i--)
  {
    FastLED.setBrightness(i);
    FastLED.show();

    tone(PIEZO_PIN, i * 4);
    delay(11);
  }

  delay(200);
  noTone(PIEZO_PIN);
}

void renderFired()
{
  for (int i = 0; i < NUM_LEDS; i++)
  {
    leds[i] = CRGB(255, 0, 0);
  }
  FastLED.show();
  tone(PIEZO_PIN, 1300);

  delay(80);

  for (int i = 0; i < NUM_LEDS; i++)
  {
    leds[i] = CRGB(0, 0, 0);
  }
  FastLED.show();
  noTone(PIEZO_PIN);

  delay(140);
}

void playTone(int tone, int duration)
{

  for (long i = 0; i < duration * 1000L; i += tone * 2)
  {

    digitalWrite(PIEZO_PIN, HIGH);

    delayMicroseconds(tone);

    digitalWrite(PIEZO_PIN, LOW);

    delayMicroseconds(tone);
  }
}

void playNote(char note, int duration)
{

  char names[] = {'C', 'D', 'E', 'F', 'G', 'A', 'B',

                  'c', 'd', 'e', 'f', 'g', 'a', 'b',

                  'x', 'y'};

  int tones[] = {1915, 1700, 1519, 1432, 1275, 1136, 1014,

                 956, 834, 765, 593, 468, 346, 224,

                 655, 715};

  int SPEE = 5;

  // play the tone corresponding to the note name

  for (int i = 0; i < 17; i++)
  {

    if (names[i] == note)
    {
      int newduration = duration / SPEE;
      playTone(tones[i], newduration);
    }
  }
}

void renderParty()
{
  for (int i = 0; i < NUM_LEDS; i++)
  {
    auto color = hue((i * (360 / NUM_LEDS) + partyAngle) * PI / 180);
    leds[i] = CRGB(color.x, color.y, color.z);
  }

  partyAngle += 5;

  FastLED.setBrightness(70);
  FastLED.show();

  for (int i = 0; i < 28; i++)
  {

    if (notes[i] == ' ')
    {

      delay(beats[i] * 150); // rest
    }
    else
    {

      playNote(notes[i], beats[i] * 150);
    }

    // pause between notes

    delay(150);
  }
}

void loop()
{

  switch (state)
  {
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

    for (int i = 0; i < NUM_LEDS; i++)
    {
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

    if (commonTimer.ready)
    {
      if (ledInLapCount % 25 == 0)
      {
        tone(PIEZO_PIN, 1000);

        ledInLapCount = 0;

        if (state == BombState::Armed)
        {
          commonElapsed += 1;
        }
      }
      else
      {
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
    state = BombState::Finished;
    break;
  }

  case BombState::Fired:
  {
    renderFired();
    FastLED.show();
    break;
  }

  case BombState::Finished:
  {
    Serial.println("PLEASE RESTART THE BOMB...");

    if (digitalRead(SETUP_PIN) == LOW)
    {
      delay(3000);
      if (digitalRead(SETUP_PIN) == LOW)
      {
        Serial.println("ENTERED PARTY MODE");

        // activate party mode
        state = BombState::Party;
        break;
      }
    }

    break;
  }

  case BombState::Party:
  {
    renderParty();
    break;
  }
  }

  if (digitalRead(SETUP_PIN) == LOW && state == BombState::Setup)
  {
    Serial.println("DISARMING!");
    state = BombState::Disarmed;
  }

  if (digitalRead(SETUP_PIN) == HIGH && state == BombState::Disarmed)
  {
    state = BombState::Armed;
  }

  // TODO check all cables
  if (digitalRead(RED_CABLE) == HIGH && (state == BombState::Armed || state == BombState::Disarmed))
  {
    state = BombState::Fired;
  }

  if (digitalRead(GREEN_CABLE) == HIGH && (state == BombState::Armed || state == BombState::Disarmed))
  {
    state = BombState::Defused;
  }
}