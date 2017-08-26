
const int pirPin = 4;
const int ledPin = 9;

// States
const int STATE_OFF = 0;
const int STATE_STARTING = 1;
const int STATE_ON = 2;
const int STATE_STOPPING = 3;

const int STARTING_TIMEOUT = 5 * 1000;
const int ON_TIMEOUT = 15 * 1000;
const int STOPPING_TIMEOUT = 5 * 1000;

int state = STATE_OFF;
int stateStartTime = 0;

void setup() {
  Serial.begin(9600);
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(ledPin, OUTPUT);
  pinMode(pirPin, INPUT);
}

void loop() {
  int pirState = digitalRead(pirPin);
  int now = millis();
  int elapsed = now - stateStartTime;
  //Serial.println(millis());

  // State transitions
  if (state == STATE_OFF && pirState == HIGH) {
    state = STATE_STARTING;
    stateStartTime = now;
  } else if (state == STATE_STARTING && elapsed > STARTING_TIMEOUT) {
    state = STATE_ON;
    stateStartTime = now;
  } else if (state == STATE_ON && elapsed > ON_TIMEOUT) {
    state = STATE_STOPPING;
    stateStartTime = now;
  } else if (state == STATE_STOPPING && elapsed > STOPPING_TIMEOUT) {
    state = STATE_OFF;
    stateStartTime = now; 
  }

  // State controls leds
  if (state == STATE_OFF) {
    leds(LOW);
  } else if (state == STATE_STARTING) {
    leds((elapsed / 100) % 2);
  } else if (state == STATE_ON) {
    leds(HIGH);
  } else if (state == STATE_STOPPING) {
    leds((elapsed / 100) % 2);
  }
}

void leds(int highOrLow) {
  digitalWrite(ledPin, highOrLow);
  digitalWrite(LED_BUILTIN, highOrLow);
}

