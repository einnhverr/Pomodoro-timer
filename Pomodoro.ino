#if !defined( ESP32 )
  #error Runs only on ESP32 platform!
#endif

#include <Wire.h>
#include <Adafruit_SSD1306.h>

#define _TIMERINTERRUPT_LOGLEVEL_ 0
#include <ESP32_New_TimerInterrupt.h>

// hw variables
#ifndef LED_BUILTIN
#define LED_BUILTIN GPIO_NUM_2
#endif

#define OLED_WIDTH 128
#define OLED_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(OLED_WIDTH, OLED_HEIGHT, &Wire, OLED_RESET);


// ESP32 Timer
ESP32Timer ITimer(1);
ESP32Timer StatusTimer(2);

// Buttons
volatile unsigned long last_interrupt_time = 0;
volatile unsigned long interrupt_time;
#define BUTTON_LEFT GPIO_NUM_5
#define BUTTON_MIDDLE GPIO_NUM_18
#define BUTTON_RIGHT GPIO_NUM_19

// app variables
int cfg_focus_time = 25;
int cfg_short_break = 5;
int cfg_long_break = 15;
int cfg_break_counter = 4;

long break_timer;
volatile int counter = cfg_focus_time;
volatile int break_counter = 0;

enum State {FOCUS_STBY, FOCUS, FOCUS_PAUSE, BREAK_STBY, BREAK};
State current_state = FOCUS_STBY;

bool IRAM_ATTR runClock(void *timerNo) {
  counter--;

  return true;
}

bool IRAM_ATTR checkStatus(void *timerNo) {
  switch (current_state) {
  case FOCUS_STBY:
    break;
  case FOCUS:
    if (counter <= 0) {
      ITimer.stopTimer();
      counter = cfg_short_break;
      current_state = BREAK_STBY;
    }
    break;
  case FOCUS_PAUSE:
    break;
  case BREAK_STBY:
    break;
  case BREAK:
    if (counter <= 0) {
      ITimer.stopTimer();
      counter = cfg_focus_time;
      current_state = FOCUS_STBY;
    }
    break;
  default:
    break;
  }

  return true;
}

void IRAM_ATTR push_left() {
  interrupt_time = millis();
  if (interrupt_time - last_interrupt_time > 250) {
    switch (current_state) {
    case FOCUS_STBY:
      counter = cfg_focus_time;
      ITimer.restartTimer();
      current_state = FOCUS;
      break;
    case FOCUS:
      break_counter++;
      ITimer.stopTimer();
      current_state = BREAK_STBY;
      break;
    case FOCUS_PAUSE:
      break_counter++;
      current_state = BREAK_STBY;
      break;
    case BREAK_STBY:
      if (break_counter == 4) {
	counter = cfg_long_break;
      } else {
	counter = cfg_short_break;
      }
      ITimer.restartTimer();
      current_state = BREAK;
      break;
    case BREAK:
      if (break_counter == 4) {
	break_counter = 0;
      }
      current_state = FOCUS_STBY;
      break;
    default:
      break;
    }
  }
  last_interrupt_time = interrupt_time;
}

void IRAM_ATTR push_middle() {
  interrupt_time = millis();
  if (interrupt_time - last_interrupt_time > 250) {
    switch (current_state) {
    case FOCUS_STBY:
      counter = cfg_focus_time;
      break;
    case FOCUS:
      ITimer.stopTimer();
      current_state = FOCUS_PAUSE;
      break;
    case FOCUS_PAUSE:
      ITimer.restartTimer();
      current_state = FOCUS;
      break;
    case BREAK_STBY:
      break;
    case BREAK:
      break;
    default:
      break;
    }
  }
  last_interrupt_time = interrupt_time;
}

void IRAM_ATTR push_right() {
  interrupt_time = millis();
  if (interrupt_time - last_interrupt_time > 250) {
    switch (current_state) {
    case FOCUS_STBY:
      counter += 1;
      break;
    case FOCUS:
      break;
    case FOCUS_PAUSE:
      break;
    case BREAK_STBY:
      break;
    case BREAK:
      break;
    default:
      break;
    }
  }
  last_interrupt_time = interrupt_time;
}

void setup() {
  // serial for initial debug
  Serial.begin(115200);

  // setup inital OLED display voltage(internal 3.3V and address(
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("SSD1306 not found");
    // stop proceeding further
    for (;;);
  }
  // let OLED properly initialize
  delay(2000);

  // clear buffer
  display.clearDisplay();

  // setup buttons
  pinMode(BUTTON_LEFT, INPUT_PULLUP);
  attachInterrupt(BUTTON_LEFT, push_left, FALLING);

  pinMode(BUTTON_MIDDLE, INPUT_PULLUP);
  attachInterrupt(BUTTON_MIDDLE, push_middle, FALLING);

  pinMode(BUTTON_RIGHT, INPUT_PULLUP);
  attachInterrupt(BUTTON_RIGHT, push_right, FALLING);


  // setup interrupt timer for 1s
  ITimer.attachInterruptInterval(1000000, runClock);
  ITimer.stopTimer();

  // setup interrupt for status timer 50ms
  StatusTimer.attachInterruptInterval(50000, checkStatus);
  
  // initial counter value
  counter = cfg_focus_time;
}

void drawDisplay() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0,0);
  display.println(counter);
  display.display();
}

void loop() {
  switch (current_state) {
  case FOCUS_STBY:
    Serial.println("STATE: FOCUS_STBY");
    Serial.println(counter);
    Serial.println(break_counter);
    drawDisplay();
    break;
  case FOCUS:
    Serial.println("STATE: FOCUS");
    Serial.println(counter);
    Serial.println(break_counter);
    drawDisplay();
    break;
  case FOCUS_PAUSE:
    Serial.println("STATE: FOCUS_PAUSE");
    Serial.println(counter);
    Serial.println(break_counter);
    drawDisplay();
    break;
  case BREAK_STBY:
    Serial.println("STATE: BREAK_STBY");
    Serial.println(counter);
    Serial.println(break_counter);
    drawDisplay();
    break;
  case BREAK:
    Serial.println("STATE: BREAK");
    Serial.println(counter);
    Serial.println(break_counter);
    drawDisplay();
    break;
  default:
    Serial.println("STATE: default. Invalid state.");
    break;
  }
  delay(40);
}
