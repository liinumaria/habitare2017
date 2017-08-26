#include <SPI.h>           // SPI library
#include <SdFat.h>         // SDFat Library
#include <SFEMP3Shield.h>  // Mp3 Shield Library

SdFat sd; // Create object to handle SD functions

SFEMP3Shield MP3player; // Create Mp3 library object
// These variables are used in the MP3 initialization to set up
// some stereo options:
const uint8_t volume = 70; // MP3 Player volume 0=max, 255=lowest (off)
const uint16_t monoMode = 1;  // Mono setting 0=off, 3=max

// Pins
const int ledPin = 5;
const int pirPin1 = A1;
const int pirPin2 = A2;
const int pirPin3 = A3;

// States
const int STATE_OFF = 0;
const int STATE_STARTING = 1;
const int STATE_ON = 2;
const int STATE_STOPPING = 3;

const int STARTING_TIMEOUT = 2 * 1000;
const int ON_TIMEOUT = 30 * 1000;
const int STOPPING_TIMEOUT = 3 * 1000;

int state = STATE_OFF;
int stateStartTime = 0;
int elapsed = 0;
int currentTrack = 1;

void setup() {
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(ledPin, OUTPUT);
  pinMode(pirPin1, INPUT);
  pinMode(pirPin2, INPUT);
  pinMode(pirPin3, INPUT);

  initSD();  // Initialize the SD card
  initMP3Player(); // Initialize the MP3 Shield
}

// initSD() initializes the SD card and checks for an error.
void initSD()
{
  //Initialize the SdCard.
  if(!sd.begin(SD_SEL, SPI_FULL_SPEED)) // was: SPI_HALF_SPEED
    sd.initErrorHalt();
  if(!sd.chdir("/")) 
    sd.errorHalt("sd.chdir");
}

// initMP3Player() sets up all of the initialization for the
// MP3 Player Shield. It runs the begin() function, checks
// for errors, applies a patch if found, and sets the volume/
// stero mode.
void initMP3Player()
{
  uint8_t result = MP3player.begin(); // init the mp3 player shield
  if(result != 0 && result != 6) // check result, see readme for error codes.
  {
    Serial.print("Error calling MP3player.begin(): ");
    Serial.println(result);
  }
  MP3player.setVolume(volume, volume);
  MP3player.setMonoMode(monoMode);
}

void loop() {
  int pirState = digitalRead(pirPin1);
  int now = millis();
  elapsed = now - stateStartTime;
  //Serial.println(millis());

  // State transitions
  if (state == STATE_OFF && pirState == HIGH) {
    toTrack(2);
    setState(STATE_STARTING, now);
  } else if (state == STATE_STARTING && elapsed > STARTING_TIMEOUT) {
    toTrack(3);
    setState(STATE_ON, now);
  } else if (state == STATE_ON && elapsed > ON_TIMEOUT) {
    MP3player.stopTrack();
    setState(STATE_STOPPING, now);
  } else if (state == STATE_STOPPING && elapsed > STOPPING_TIMEOUT) {
    toTrack(1);
    setState(STATE_OFF, now);
  }

  // LED effects
  if (state == STATE_OFF) {
    toTrack(1);
    leds(0);
  } else if (state == STATE_STARTING) {
    leds(random(255));
  } else if (state == STATE_ON) {
    toTrack(3);
    if (elapsed < 5000) {
      int power = 255 * (elapsed/5000.0);
      leds(power);
    } else {
      leds(255);
    }
  } else if (state == STATE_STOPPING) {
    leds(random(255));
    MP3player.stopTrack();
  }
}

void setState(int newState, int now) {
  state = newState;
  stateStartTime = now;
  elapsed = 0;
}

void toTrack(int number) {
  if (currentTrack != number || !MP3player.isPlaying()) {
    MP3player.stopTrack();

    /* Use the playTrack function to play a numbered track: */
    currentTrack = number;
    uint8_t result = MP3player.playTrack(currentTrack);
  
    if (result == 0)  // playTrack() returns 0 on success
    {
      Serial.print("Playing track ");
      Serial.println(currentTrack);
    }
    else // Otherwise there's an error, check the code
    {
      Serial.print("Error playing track: ");
      Serial.println(result);
    }
  }
}

void leds(int amount) {
  analogWrite(ledPin, amount);
  digitalWrite(LED_BUILTIN, amount > 127 ? 1 : 0);
}

