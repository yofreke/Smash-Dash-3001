#pragma message "Original implementation in Smash-Dash-3000/code/step5-gravity"

// #region Imports

#include <Arduino.h>

// #region FastLED Import

// Add this before including `FastLED.h` to quiet pragma messages
#define FASTLED_INTERNAL

// Use this to import the library from project subdirectory.
// Note: This fails with VSCode Arduino compilation.
//#include "libraries/FastLED-3.3.3/FastLED.h"

// Use this import to include the global FastLED library
//   located at `~/Documents/Arduino/libraries/FastLED-3.3.3/`.
#include <FastLED.h>

// #endregion FastLED Import

#include <SoftwareSerial.h>
// #include "libraries/DFRobotDFPlayerMini/DFRobotDFPlayerMini.h"
#include <DFRobotDFPlayerMini.h>

// #endregion Imports

// #region Constant Variables

// Use `#define` for constant variables to leverage compiler optimization
#define NUM_LEDS 300
#define LED_STRING_DATA_PIN 12

// Note: Milliamp value determined experimentally.
// - 3500mA works well if the 15A power supply is connected directly to one
//   end of the LED strip.
// - 1850mA works well if the LED strip is powered through breadboard and
//   jumper wires
#define LED_MAX_MILLIAMPS 1850

// Maximum brightness displayed by any LED
#define LED_ON_POWER 200

// This is the signal pin for the main power relay
#define POWER_RELAY_PIN 32

// The pin that the start button switch is attached to
#define START_BUTTON_PIN 30
// The pin for start button LED
#define START_BUTTON_LED_PIN 31

// Player 1
#define BUTTON_PIN_ONE 26
#define BUTTON_LED_PIN_ONE 27
// Player 2
#define BUTTON_PIN_TWO 22
#define BUTTON_LED_PIN_TWO 23

// Value range (0~30)
#define MP3_VOLUME 15

#define MP3_START_SOUND 1
#define MP3_BOOP_SOUND 2

#define MIN_BOOP_DELAY 350

#define MIN_PLAYER_POSITION 0
#define MAX_PLAYER_POSITION NUM_LEDS

// Magic number: Tune this based on your own gameplay
#define PLAYER_VELOCITY_PICKUP 0.1

// Maximum player speed is ~32 presses of controller button
#define MAX_PLAYER_VELOCITY (32 * PLAYER_VELOCITY_PICKUP)

// Magic number: Tune this based on your own gameplay
#define FRICTION .96

// The index of the LED on the strip where gravity should start being applied
#define GRAVITY_START 200

// Magic number: Tune this based on your own gameplay
#define GRAVITY_AMOUNT 0.01

// #endregion Constant Variables

// #region Global Variables

// Create an array of `CRGB` instances.
// Each item in the array specifies the color for a given LED on the strip.
// Each item in the array holds a red, green, and blue value.
CRGB leds[NUM_LEDS];

// The last known state of the start button.
// 0 for not pressed, 1 for pressed.
byte lastStartButtonState = 0;

// The last known state of each controller.
byte lastButtonStateOne = 0;
byte lastButtonStateTwo = 0;

// In the game each player moves from the start to the end of the LED strip.
float playerPositionOne = 0;
float playerPositionTwo = 0;

// When a player controller is pressed, that player velocity is increased.
// Player velocity is applied to player position once per `loop()` call.
float playerVelocityOne = 0;
float playerVelocityTwo = 0;

// These variables store the color to display for each player
// 1: Red
const CRGB playerColorOne = CRGB(200, 0, 0);
// 2: Green
const CRGB playerColorTwo = CRGB(9, 227, 67);

// The last time (in milliseconds) that a sound was played.
// The DFPlayer can only play a single sound at once, so rapid calls to start
//   playing a sound result in never hearing more than the first few seconds of
//   the audio clip.
unsigned long lastBoopTime = 0;

// The SoftwareSerial is used to communicate with the DFPlayer.
// Argument order is (RX Pin, TX Pin)
SoftwareSerial mySoftwareSerial(10, 11);
DFRobotDFPlayerMini myDFPlayer;

// #endregion Global Variables

// #region Functions

/**
 * This function will repeatedly attempt to establish a serial connection with
 * the DFPlayer. A single attempt infrequently fails. This guarantees a
 * consistent user experience.
 */
void waitForMp3Connection()
{
  Serial.print("\nInitializing DFPlayer ... (May take 3~5 seconds)\n");

  unsigned int connectionTryCount = 0;

  while (true)
  {
    connectionTryCount++;

    Serial.print("Attempting connection to MP3 player... connectionTryCount= ");
    Serial.print(connectionTryCount);
    Serial.print("\n");

    bool isMp3PlayerConnected = myDFPlayer.begin(mySoftwareSerial);

    if (!isMp3PlayerConnected)
    {
      // Use softwareSerial to communicate with mp3.
      Serial.println("Unable to begin, connection to MP3 player failed:");
      Serial.println("1.Please recheck the connection!");
      Serial.println("2.Please insert the SD card!");

      Serial.println("Retrying...");
      delay(1000);
    }
    else
    {
      break;
    }
  }

  Serial.println("DFPlayer Mini online.");
}

void playBoopSound(const unsigned long now)
{
  if (now - lastBoopTime > MIN_BOOP_DELAY)
  {
    lastBoopTime = now;

    myDFPlayer.playMp3Folder(MP3_BOOP_SOUND);
  }
}

void setup()
{
  Serial.begin(9600);

  // Turn on main power relay
  pinMode(POWER_RELAY_PIN, OUTPUT);
  digitalWrite(POWER_RELAY_PIN, LOW);

  // Ensure the correct pin modes for start button are set
  pinMode(START_BUTTON_PIN, INPUT);
  pinMode(START_BUTTON_LED_PIN, OUTPUT);

  // Ensure the correct pin modes are set for player buttons
  pinMode(BUTTON_PIN_ONE, INPUT);
  pinMode(BUTTON_LED_PIN_ONE, OUTPUT);

  pinMode(BUTTON_PIN_TWO, INPUT);
  pinMode(BUTTON_LED_PIN_TWO, OUTPUT);

  // Initialize the FastLED instance.
  // Use the `NEOPIXEL` protocol for WS2812B strings.
  FastLED.addLeds<NEOPIXEL, LED_STRING_DATA_PIN>(leds, NUM_LEDS);

  // Clear any existing state on the LED strip
  FastLED.clear();

  // Limit maximum brightness.
  // Depends on power supply, LED strip length, LED density, power cable, etc.
  FastLED.setMaxPowerInVoltsAndMilliamps(5, LED_MAX_MILLIAMPS);

  // Setup DFPlayer serial connection
  mySoftwareSerial.begin(9600);

  // Initialize MP3 player
  waitForMp3Connection();

  myDFPlayer.reset();
  myDFPlayer.setTimeOut(500);
  myDFPlayer.volume(MP3_VOLUME);
  myDFPlayer.EQ(DFPLAYER_EQ_NORMAL);
  myDFPlayer.enableDAC();

  // Play the start sound for game
  myDFPlayer.playMp3Folder(MP3_START_SOUND);
}

void loop()
{
  unsigned long now = millis();

  // Check for start button press
  byte startButtonState = digitalRead(START_BUTTON_PIN);

  if (startButtonState != lastStartButtonState)
  {
    lastStartButtonState = startButtonState;
  }

  digitalWrite(START_BUTTON_LED_PIN, startButtonState * LED_ON_POWER);

  // Check for player button press
  byte buttonStateOne = digitalRead(BUTTON_PIN_ONE);
  byte buttonStateTwo = digitalRead(BUTTON_PIN_TWO);

  // Check player 1 button
  if (buttonStateOne != lastButtonStateOne)
  {
    lastButtonStateOne = buttonStateOne;

    // Button has moved from LOW to HIGH
    if (buttonStateOne == HIGH)
    {
      playerVelocityOne = playerVelocityOne + PLAYER_VELOCITY_PICKUP;

      // Clamp player velocity to a maximum value to help balance gameplay.
      if (playerVelocityOne > MAX_PLAYER_VELOCITY)
      {
        playerVelocityOne = MAX_PLAYER_VELOCITY;
      }

      playBoopSound(now);
    }
  }

  // Check player 2 button
  if (buttonStateTwo != lastButtonStateTwo)
  {
    lastButtonStateTwo = buttonStateTwo;
    if (buttonStateTwo == HIGH)
    {
      playerVelocityTwo = playerVelocityTwo + PLAYER_VELOCITY_PICKUP;

      if (playerVelocityTwo > MAX_PLAYER_VELOCITY)
      {
        playerVelocityTwo = MAX_PLAYER_VELOCITY;
      }

      playBoopSound(now);
    }
  }

  // Update controller lights
  digitalWrite(BUTTON_LED_PIN_ONE, buttonStateOne * LED_ON_POWER);
  digitalWrite(BUTTON_LED_PIN_TWO, buttonStateTwo * LED_ON_POWER);

  // Update player positions
  playerPositionOne = playerPositionOne + playerVelocityOne;
  playerPositionTwo = playerPositionTwo + playerVelocityTwo;

  // Apply friction to player velocities
  float lastPlayerVelocityOne = playerVelocityOne;
  playerVelocityOne = playerVelocityOne * FRICTION;
  playerVelocityTwo = playerVelocityTwo * FRICTION;

  // Apply gravity to players
  if (playerPositionOne > GRAVITY_START)
  {
    playerVelocityOne -= GRAVITY_AMOUNT;
  }

  if (playerPositionTwo > GRAVITY_START)
  {
    playerVelocityTwo -= GRAVITY_AMOUNT;
  }

  if (floor(lastPlayerVelocityOne * 10) != floor(playerVelocityOne * 10))
  {
    Serial.print("playerVelocityOne= ");
    Serial.println(playerVelocityOne);
  }

  // Update light strip based on player positions
  for (int lightIndex = 0; lightIndex < NUM_LEDS; lightIndex++)
  {
    byte red = 0;
    byte green = 0;
    byte blue = 0;

    // Player 1
    float distanceToPlayer = fabs(playerPositionOne - (float)lightIndex);
    if (distanceToPlayer < 1)
    {
      red += playerColorOne.red;
      green += playerColorOne.green;
      blue += playerColorOne.blue;
    }

    // Player 2
    distanceToPlayer = fabs(playerPositionTwo - (float)lightIndex);
    if (distanceToPlayer < 1)
    {
      red += playerColorTwo.red;
      green += playerColorTwo.green;
      blue += playerColorTwo.blue;
    }

    // Clamp light max value
    if (red > LED_ON_POWER)
    {
      red = LED_ON_POWER;
    }
    if (green > LED_ON_POWER)
    {
      green = LED_ON_POWER;
    }
    if (blue > LED_ON_POWER)
    {
      blue = LED_ON_POWER;
    }

    leds[lightIndex].red = red;
    leds[lightIndex].green = green;
    leds[lightIndex].blue = blue;
  }

  // Send 'leds' data from arduino to led strip.
  FastLED.show();
}

// #endregion
