#include <SPI.h>           // SPI library
#include <SdFat.h>         // SDFat Library
#include <SFEMP3Shield.h>  // Mp3 Shield Library

SdFat sd; // Create object to handle SD functions

SFEMP3Shield MP3player; // Create Mp3 library object
// These variables are used in the MP3 initialization to set up
// some stereo options:
const uint8_t VOLUME = 70; // MP3 Player volume 0=max, 255=lowest (off)
const uint8_t SILENT = 254; // MP3 Player volume 0=max, 255=lowest (off)
const uint16_t monoMode = 1;  // Mono setting 0=off, 3=max

// Pins
const int ledPin = 5;

const int pirPins[] = {A0, A1, A2, A3};
const int PIR_SENSOR_COUNT = 4;
const int MOVEMENT_THRESHOLD = 1; // How many pirPins need to be HIGH to signal movement

// States
const int STATE_OFF = 0;
const int STATE_STARTING = 1;
const int STATE_ON = 2;
const int STATE_STOPPING = 3;

const unsigned long STARTING_TIMEOUT = 10 * 1000LU; // Duration of STARTING state: 10 seconds
const unsigned long ON_TIMEOUT = 5 * 60 * 1000LU; // Max duration of ON state: 5 minutes
const unsigned long NO_MOVEMENT_TIMEOUT = 1*60*1000LU; // ON state ends in this time if no movement: 1 minute
const unsigned long STOPPING_TIMEOUT = 15 * 1000LU; // Duration of STOPPING state: 15 seconds

const unsigned long CALIBRATION_TIME = 30 * 1000LU; // Time to wait in the beginning

int state = STATE_OFF;
int subState = 0;
unsigned long stateStartTime = 0;
unsigned long elapsed = 0;
unsigned long lastMovementSeenAt = 0;
int currentTrack = 1;

void setup() {
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(ledPin, OUTPUT);
  for (int i=0; i < PIR_SENSOR_COUNT; i++) {
    pinMode(pirPins[i], INPUT);
  }
  
  Serial.println("Initializing SD card and MP3 player...");
  initSD();  // Initialize the SD card
  initMP3Player(); // Initialize the MP3 Shield

  // PIR calibration period
  for (int i=0; i<CALIBRATION_TIME; i+=1000) {
    leds((i/1000 % 2) * 255);
    delay(1000);
  }
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
  MP3player.setVolume(VOLUME, VOLUME);
  MP3player.setMonoMode(monoMode);
}

boolean isMovement() {
  int count = 0;
  for (int i=0; i < PIR_SENSOR_COUNT; i++) {
    if (digitalRead(pirPins[i]) == HIGH) {
      Serial.print(pirPins[i]);
      Serial.print(", ");
      count++;
    }
  }

  if (count > 0) {
    Serial.println("");
  }

  return count >= MOVEMENT_THRESHOLD;
}

boolean movementReported = false;
void loop() {
  boolean movement = isMovement();
  unsigned long now = millis();
  elapsed = now - stateStartTime;

  if (movement) {
    lastMovementSeenAt = now;

    // Wait for no movement before printing the message again
    if (!movementReported) {
      Serial.println("Movement!");
      movementReported = true;
    }
  } else if (movementReported) {
    movementReported = false;
  }

  // State transitions
  if (state == STATE_OFF && movement) {
    setState(STATE_STARTING, now);
  } else if (state == STATE_STARTING && elapsed > STARTING_TIMEOUT) {
    // Little silent period before going full ON
    MP3player.stopTrack();
    leds(0);
    delay(3000);

    now = millis();
    toTrack(3);
    setState(STATE_ON, now);
    startAnimation(VOLUME, VOLUME, 0, 255, 5000, now);
    lastMovementSeenAt = now;
  } else if (state == STATE_ON && (elapsed > ON_TIMEOUT || now - lastMovementSeenAt > NO_MOVEMENT_TIMEOUT)) {
    if (now - lastMovementSeenAt > NO_MOVEMENT_TIMEOUT) {
      Serial.println("No movement seen in a while, stopping...");
    } else {
      Serial.println("Reached timeout for ON state, stopping...");
    }
    
    setState(STATE_STOPPING, now);
  } else if (state == STATE_STOPPING && elapsed > STOPPING_TIMEOUT) {
    toTrack(1);
    setState(STATE_OFF, now);
  }

  // LED effects
  if (state == STATE_OFF) {
    toTrack(1);

    if (subState == 0 && isAnimationFinished(now)) {
      startAnimation(VOLUME, VOLUME, 0, 0, random(15 * 1000), now);
      subState = 1;
    } else if (subState == 1 && isAnimationFinished(now)) {
      int power = random(10, 127);
      startAnimation(VOLUME, VOLUME, power, power, random(50, 200), now);
      subState = 0;      
    }
  } else if (state == STATE_STARTING) {
    if (subState == 0) {
      // Fade out sound
      startAnimation(VOLUME, SILENT, 0, 0, 2000, now);
      subState = 1;
    } else if (subState == 1 && isAnimationFinished(now)) {
      // Silence a few seconds
      MP3player.stopTrack();
      toTrack(2);
      delay(500);
      now = millis();
      subState = 2;
    } else if (subState == 2 && isAnimationFinished(now)) {
      double at = ((double) elapsed) / STARTING_TIMEOUT;
      int maxTime = 2000 - (int)(at*2000);
      startAnimation(SILENT, SILENT, 0, 0, random(maxTime), now);
      subState = 3;
    } else if (subState == 3 && isAnimationFinished(now)) {
      startAnimation(VOLUME, VOLUME, 255, 255, random(50, 200), now);
      subState = 2;      
    }
  } else if (state == STATE_ON) {
    toTrack(3);

    // Make sure the leds are set to full power in the end of animation
    if (isAnimationFinished(now)) {
      leds(255);
    }
  } else if (state == STATE_STOPPING) {

    if (subState == 0 && isAnimationFinished(now)) {
      double at = ((double) elapsed) / STARTING_TIMEOUT;
      int maxTime = 3000 - (int)(at*3000);
      startAnimation(VOLUME, VOLUME, 255, 255, random(maxTime), now);
      subState = 1;
    } else if (subState == 1 && isAnimationFinished(now)) {
      startAnimation(VOLUME, VOLUME, 0, 0, random(50, 200), now);
      subState = 0;      
    }
  }

  animate(now);
}

void setState(int newState, unsigned long now) {
  state = newState;
  subState = 0;
  stateStartTime = now;
  elapsed = 0;
  stopAnimation();
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

// State variables for the animation
unsigned long animationStartTime = 0;
unsigned long animationEndTime = 0;
unsigned long animationDuration = 0;
int animationVolumeStart;
int animationVolumeDelta;
int animationLedStart;
int animationLedDelta;

void startAnimation(int volumeStart, int volumeEnd, int ledStart, int ledEnd, unsigned long duration, unsigned long startTime) {
  animationVolumeStart = volumeStart;
  animationVolumeDelta = volumeEnd - volumeStart;
  animationLedStart = ledStart;
  animationLedDelta = ledEnd - ledStart;
  animationDuration = duration;
  animationStartTime = startTime;
  animationEndTime = startTime + duration;
}

void stopAnimation() {
  animationEndTime = 0;
}

void animate(unsigned long now) {
  if (!isAnimationFinished(now)) {
    double atRatio = ((double)(now - animationStartTime))/(double)animationDuration;    
    int vol = animationVolumeStart + (int)(atRatio * animationVolumeDelta);
    int light = animationLedStart + (int)(atRatio * animationLedDelta);
    MP3player.setVolume(vol, vol);
    leds(light);

    /*
    Serial.print("Animation is at ");
    Serial.print(atRatio);
    Serial.print(", volume: ");
    Serial.print(vol);
    Serial.print(", leds: ");
    Serial.println(light);
    */
  }
}

boolean isAnimationFinished(unsigned long now) {
  return now > animationEndTime;
}

