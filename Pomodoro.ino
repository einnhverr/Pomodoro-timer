#if !defined( ESP32 )
  #error Runs only on ESP32 platform!
#endif

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_NeoPixel.h>
#include <ESPDateTime.h>
#include <Fsm.h>
#include <Wire.h>

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

#define RING_LED GPIO_NUM_4
#define RING_LED_SIZE 12
#define RING_LED_BRIGHTNESS 50
Adafruit_NeoPixel ring_led(RING_LED_SIZE, RING_LED, NEO_GRB + NEO_KHZ800);

// ESP32 Timer
ESP32Timer ITimer(1);

// Buttons
volatile unsigned long last_interrupt_time = 0L;
volatile unsigned long interrupt_time;
#define BUTTON_LEFT GPIO_NUM_5
#define BUTTON_MIDDLE GPIO_NUM_18
#define BUTTON_RIGHT GPIO_NUM_19

// app variables
int cfg_focus_time = 60;//25 * 60;
int cfg_break_short_time = 12;//5 * 60;
int cfg_break_long_time = 36;//15 * 60;
int cfg_break_counter = 4;
enum Progress {CLEAR, FOCUS_STDBY, FOCUS, FOCUS_PAUSE, BREAK_STDBY, BREAK};
Progress progress;

// time with 1s resolution
time_t max_time_counter = cfg_focus_time;
volatile time_t time_counter = cfg_focus_time;
volatile int focus_counter = 0;

static const uint8_t pomodoro_bmp[] = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0xe0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1f, 0xf0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf0, 0x1f, 0xe0, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x07, 0xf8, 0x0f, 0xc0, 0x47, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x1f, 0xfc, 0x07, 0x80, 0xbf, 0xf0, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x7f, 0xfe, 0x00, 0x00, 0xff, 0xfc, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0xfe, 0x03, 0x00, 0x01, 0x81, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x01, 0xf0, 0x00, 0x00, 0x00, 0x00, 0x1f, 0x80, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x70, 0x00, 0x00, 0x00, 0x00, 0x0f, 0xc0, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x07, 0xff, 0xe0, 0x00, 0x00, 0x00, 0x07, 0xe0, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x1f, 0xff, 0xfe, 0x00, 0x00, 0x0f, 0xc3, 0xf0, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x3f, 0xff, 0xfc, 0x00, 0x00, 0x7f, 0xff, 0xf0, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x3f, 0xff, 0xf8, 0x00, 0x00, 0x3f, 0xff, 0xf0, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x3f, 0xff, 0xf0, 0x00, 0x00, 0x1f, 0xff, 0xc8, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x7f, 0xff, 0xe0, 0x07, 0xe0, 0x0f, 0xff, 0xf0, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x7f, 0xff, 0xc0, 0xff, 0xf8, 0x0f, 0xff, 0xf8, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x7f, 0xff, 0x87, 0xff, 0xfc, 0x07, 0xff, 0xfc, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x7f, 0xff, 0x9f, 0xff, 0xff, 0x07, 0xff, 0xfc, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0x87, 0xff, 0xfc, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0xff, 0xfe, 0xff, 0xff, 0xff, 0xc7, 0xff, 0xfc, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0xff, 0xfc, 0xff, 0xff, 0xff, 0xe7, 0xff, 0xfc, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0xff, 0xfd, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0xff, 0xf9, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0xff, 0xfb, 0xff, 0xff, 0xff, 0xfb, 0xff, 0xfc, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0xff, 0xf3, 0xff, 0xff, 0xff, 0xfd, 0xff, 0xfc, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0xff, 0xf7, 0xff, 0xff, 0xff, 0xfd, 0xff, 0xfc, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x7f, 0xf7, 0xff, 0xff, 0xff, 0xfd, 0xff, 0xfc, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x7f, 0xff, 0xff, 0xff, 0xff, 0xfc, 0xff, 0xfc, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x2f, 0xff, 0xff, 0xff, 0xff, 0xfc, 0xff, 0xfc, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x8f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xf8, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x4f, 0xff, 0xff, 0xf1, 0x8f, 0xff, 0xff, 0xf0, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x4b, 0xff, 0xff, 0xe1, 0x0f, 0xff, 0xff, 0xe4, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x63, 0xff, 0xff, 0xf9, 0x0f, 0xff, 0xff, 0xcc, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x72, 0xff, 0xff, 0xf9, 0x0f, 0xff, 0xff, 0x98, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x3c, 0xbf, 0xff, 0xe1, 0x0f, 0xff, 0xfe, 0x38, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x3e, 0x3f, 0xff, 0xe0, 0x0f, 0xff, 0xfc, 0xf8, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x3f, 0x16, 0x7f, 0xff, 0xff, 0xff, 0xf1, 0xf0, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x1f, 0xc2, 0x5f, 0xfe, 0xff, 0xff, 0x87, 0xf0, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x1f, 0xf0, 0x4b, 0xfe, 0xff, 0xfc, 0x3f, 0xf0, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x0f, 0xfe, 0x09, 0x36, 0xff, 0xc1, 0xff, 0xe0, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x0f, 0xff, 0xc0, 0x04, 0x00, 0x1f, 0xff, 0xe0, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x07, 0xff, 0xff, 0x00, 0x07, 0xff, 0xff, 0xc0, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x07, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xc0, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x03, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x80, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x01, 0xff, 0xff, 0xfe, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x01, 0xff, 0xff, 0xfc, 0x7f, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xf8, 0x3f, 0xff, 0xfe, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x7f, 0xff, 0xf0, 0x1f, 0xff, 0xfc, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x3f, 0xff, 0xff, 0xff, 0xff, 0xf8, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x1f, 0xff, 0xff, 0xff, 0xff, 0xf0, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x07, 0xff, 0xff, 0xff, 0xff, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0xff, 0xff, 0xff, 0xff, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xfe, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3f, 0xff, 0xff, 0xf8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07, 0xff, 0xff, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7f, 0xf8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

// finite state machine
// event ids
#define BUTTON_LEFT_EVENT 1
#define BUTTON_MIDDLE_EVENT 2
#define BUTTON_RIGHT_EVENT 3
#define STATE_FINISHED_EVENT 4
#define STATE_CLEAR_EVENT 5

State state_clear(clear_on_enter, &clear_on_state, &clear_on_exit);
State state_focus_stdby(focus_stdby_on_enter, &focus_stdby_on_state, &focus_stdby_on_exit);
State state_focus(focus_on_enter, &focus_on_state, &focus_on_exit);
State state_focus_pause(focus_pause_on_enter, &focus_pause_on_state, &focus_pause_on_exit);
State state_break_stdby(break_stdby_on_enter, &break_stdby_on_state, &break_stdby_on_exit);
State state_break(break_on_enter, &break_on_state, &break_on_exit);
Fsm fsm_timer(&state_clear);

#define RINGLED_OFF_EVENT 6
#define RINGLED_ON_EVENT 7
#define RINGLED_SWITCHCOLOR_EVENT 8
#define RINGLED_PROGRESSBAR_EVENT 9
State state_ringled_off(ringled_off_on_enter, &ringled_off_on_state, &ringled_off_on_exit);
State state_ringled_on(ringled_on_on_enter, &ringled_on_on_state, &ringled_on_on_exit);
State state_ringled_baseColor(ringled_baseColor_on_enter, &ringled_baseColor_on_state,
				  &ringled_baseColor_on_exit);
State state_ringled_fillColor(ringled_fillColor_on_enter, &ringled_fillColor_on_state,
				  &ringled_fillColor_on_exit);
State state_ringled_progress(ringled_progress_on_enter, &ringled_progress_on_state, &ringled_progress_on_exit);
Fsm fsm_ringled(&state_ringled_off);
uint32_t last_progress_time = 0L;
uint32_t progress_time;

// ISR
bool IRAM_ATTR countDown(void *timerNo) {
  time_counter--;

  return true;
}

void IRAM_ATTR push_left() {
  interrupt_time = millis();
  if (interrupt_time - last_interrupt_time > 250) {
    fsm_timer.trigger(BUTTON_LEFT_EVENT);
  }
  last_interrupt_time = interrupt_time;
}

void IRAM_ATTR push_middle() {
  interrupt_time = millis();
  if (interrupt_time - last_interrupt_time > 250) {
    fsm_timer.trigger(BUTTON_MIDDLE_EVENT);
    }
  last_interrupt_time = interrupt_time;
}

void IRAM_ATTR push_right() {
  interrupt_time = millis();
  if (interrupt_time - last_interrupt_time > 250) {
    fsm_timer.trigger(BUTTON_RIGHT_EVENT);
  }
  last_interrupt_time = interrupt_time;
}

// FSM callback functions
void clear_on_enter() {
  progress = CLEAR;
  focus_counter = 0;
  // initial counter value
}

void clear_on_state() {
  fsm_timer.trigger(STATE_CLEAR_EVENT);
}

void clear_on_exit() {
}

void focus_stdby_on_enter() {
  progress = FOCUS_STDBY;
  time_counter = cfg_focus_time;
  max_time_counter = cfg_focus_time;
}

void focus_stdby_on_state() {
  fsm_ringled.trigger(RINGLED_ON_EVENT);
}

void focus_stdby_on_exit() {
}

void focus_on_enter() {
  progress = FOCUS;
  ITimer.restartTimer();
}

void focus_on_state() {
  if (time_counter == 1) {
    fsm_ringled.trigger(RINGLED_SWITCHCOLOR_EVENT);
  }
  if (time_counter == 0) {
    ITimer.stopTimer();
    focus_counter++;
    fsm_timer.trigger(STATE_FINISHED_EVENT);
  }
  fsm_ringled.trigger(RINGLED_PROGRESSBAR_EVENT);
}

void focus_on_exit() {
  ITimer.stopTimer();
}

void focus_pause_on_enter() {
  progress = FOCUS_PAUSE;
}

void focus_pause_on_state() {
}

void focus_pause_on_exit() {
}

void break_stdby_on_enter() {
  progress = BREAK_STDBY;
  if (focus_counter < 4) {
    time_counter = cfg_break_short_time;
    max_time_counter = cfg_break_short_time;
  } else {
    time_counter = cfg_break_long_time;
    max_time_counter = cfg_break_long_time;
  }
}

void break_stdby_on_state() {
  fsm_ringled.trigger(RINGLED_ON_EVENT);
}

void break_stdby_on_exit() {
}

void break_on_enter() {
  progress = BREAK;
  ITimer.restartTimer();
}

void break_on_state() {
  if (time_counter == 1) {
    fsm_ringled.trigger(RINGLED_SWITCHCOLOR_EVENT);
  }
  if (time_counter == 0) {
    ITimer.stopTimer();
    fsm_timer.trigger(STATE_FINISHED_EVENT);
  }
  fsm_ringled.trigger(RINGLED_PROGRESSBAR_EVENT);
}

void break_on_exit() {
  if (focus_counter == 4) {
    focus_counter = 0;
  }
  ITimer.stopTimer();
}

void ringled_off_on_enter() {
}

void ringled_off_on_state() {
  fsm_ringled.trigger(RINGLED_ON_EVENT);
}

void ringled_off_on_exit() {
}

void ringled_on_on_enter() {
  uint32_t baseColor = ring_led.Color(0, 0, 255); // Blue
  setColor(baseColor);
}

void ringled_on_on_state() {
}

void ringled_on_on_exit() {
}

void ringled_baseColor_on_enter() {
  uint32_t baseColor = ring_led.Color(0, 0, 255); // Blue
  setColor(baseColor);
}

void ringled_baseColor_on_state() {
}

void ringled_baseColor_on_exit() {
}

void ringled_fillColor_on_enter() {
  uint32_t fillColor;
  switch (progress) {
  case FOCUS:
    fillColor = ring_led.Color(255, 0, 0); // Red
    break;
  case BREAK:
    fillColor = ring_led.Color(255, 255, 255, 255); // True white
    break;
  default:
    fillColor = ring_led.Color(0, 0, 0, 255); // True white
    break;
  }
  setColor(fillColor);
}

void ringled_fillColor_on_state() {
}

void ringled_fillColor_on_exit() {
}

void ringled_progress_on_enter() {
  progress_time = millis();
  if (progress_time - last_progress_time > 50) {
    uint32_t baseColor = ring_led.Color(0, 0, 255); // Blue
    uint32_t fillColor;

    switch (progress) {
    case FOCUS:
      fillColor = ring_led.Color(255, 0, 0); // Red
      break;
    case FOCUS_PAUSE:
      fillColor = ring_led.Color(255, 255, 255, 255); // RGB white
      break;
    case BREAK:
      fillColor = ring_led.Color(255, 255, 255, 255); // RGB white
      break;
    default:
      fillColor = ring_led.Color(0, 0, 0, 255); // True white
      break;
    }
    fillProgressBar(baseColor, fillColor, time_counter);
  }
  last_progress_time = progress_time;
}

void ringled_progress_on_state() {
}

void ringled_progress_on_exit() {
}

// initial hw setup
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
  delay(1000);

  // clear buffer
  display.clearDisplay();
  // display logo
  display.drawBitmap(0, 0, pomodoro_bmp, 128, 64, 1);
  display.display();
  delay(5000);

  // setup buttons with interrupts
  pinMode(BUTTON_LEFT, INPUT_PULLUP);
  attachInterrupt(BUTTON_LEFT, push_left, FALLING);

  pinMode(BUTTON_MIDDLE, INPUT_PULLUP);
  attachInterrupt(BUTTON_MIDDLE, push_middle, FALLING);

  pinMode(BUTTON_RIGHT, INPUT_PULLUP);
  attachInterrupt(BUTTON_RIGHT, push_right, FALLING);

  // setup interrupt timer for 1s
  ITimer.attachInterruptInterval(1000000, countDown);
  ITimer.stopTimer();

  // setup led ring
  ring_led.begin();
  ring_led.setBrightness(RING_LED_BRIGHTNESS);
  ring_led.show();

  // FSM transitions
  // timer transitions
  fsm_timer.add_transition(&state_clear, &state_focus_stdby, STATE_CLEAR_EVENT, NULL);

  fsm_timer.add_transition(&state_focus_stdby, &state_focus, BUTTON_LEFT_EVENT, NULL);
  fsm_timer.add_transition(&state_focus_stdby, &state_clear, BUTTON_RIGHT_EVENT, NULL);

  fsm_timer.add_transition(&state_focus, &state_focus_pause, BUTTON_MIDDLE_EVENT, NULL);
  fsm_timer.add_transition(&state_focus, &state_break_stdby, BUTTON_LEFT_EVENT, NULL);
  fsm_timer.add_transition(&state_focus, &state_clear, BUTTON_RIGHT_EVENT, NULL);
  fsm_timer.add_transition(&state_focus, &state_break_stdby, STATE_FINISHED_EVENT, NULL);

  fsm_timer.add_transition(&state_focus_pause, &state_clear, BUTTON_RIGHT_EVENT, NULL);
  fsm_timer.add_transition(&state_focus_pause, &state_focus, BUTTON_MIDDLE_EVENT, NULL);
  fsm_timer.add_transition(&state_focus_pause, &state_break_stdby, BUTTON_LEFT_EVENT, NULL);
  
  fsm_timer.add_transition(&state_break_stdby, &state_break, BUTTON_LEFT_EVENT, NULL);
  fsm_timer.add_transition(&state_break_stdby, &state_clear, BUTTON_RIGHT_EVENT, NULL);

  fsm_timer.add_transition(&state_break, &state_focus_stdby, BUTTON_LEFT_EVENT, NULL);
  fsm_timer.add_transition(&state_break, &state_clear, BUTTON_RIGHT_EVENT, NULL);
  fsm_timer.add_transition(&state_break, &state_focus_stdby, STATE_FINISHED_EVENT, NULL);

  // ringled transitions
  fsm_ringled.add_transition(&state_ringled_off, &state_ringled_on, RINGLED_ON_EVENT, NULL);

  fsm_ringled.add_transition(&state_ringled_on, &state_ringled_off, RINGLED_OFF_EVENT, NULL);
  fsm_ringled.add_transition(&state_ringled_on, &state_ringled_progress, RINGLED_PROGRESSBAR_EVENT, NULL);

  fsm_ringled.add_timed_transition(&state_ringled_progress, &state_ringled_progress, 200, NULL);
  fsm_ringled.add_transition(&state_ringled_progress, &state_ringled_baseColor, RINGLED_SWITCHCOLOR_EVENT, NULL);
  fsm_ringled.add_transition(&state_ringled_progress, &state_ringled_on, RINGLED_ON_EVENT, NULL);

  fsm_ringled.add_timed_transition(&state_ringled_baseColor, &state_ringled_fillColor, 200, NULL);
  fsm_ringled.add_transition(&state_ringled_baseColor, &state_ringled_on, RINGLED_ON_EVENT, NULL);

  fsm_ringled.add_timed_transition(&state_ringled_fillColor, &state_ringled_baseColor, 200, NULL);
  fsm_ringled.add_transition(&state_ringled_fillColor, &state_ringled_on, RINGLED_ON_EVENT, NULL);
}

void drawDisplay() {
  display.clearDisplay();
  int offset = display.width()/8;
  drawTimer(offset*2+2, 18);
  drawSessionCounter(offset*4, 0, focus_counter);
  switch (progress) {
  case CLEAR:
    break;
  case FOCUS_STDBY:
    drawPlayButton(offset*3-1, 53, 10);
    drawStopButton(offset*5-1, 53, 10);
    break;
  case FOCUS:
    drawSkipButton(offset+16, 53, 10);
    drawPauseButton(offset*3+16, 53, 10);
    drawStopButton(offset*5+16, 53, 10);
    break;
  case FOCUS_PAUSE:
    drawSkipButton(offset+16, 53, 10);
    drawPlayButton(offset*3+16, 53, 10);
    drawStopButton(offset*5+16, 53, 10);
    break;
  case BREAK_STDBY:
    drawPlayButton(offset*3-1, 53, 10);
    drawStopButton(offset*5-1, 53, 10);
    break;
  case BREAK:
    drawSkipButton(offset*3-1, 53, 10);
    drawStopButton(offset*5-1, 53, 10);
    break;
  default:
    break;
  }
  display.display();
  delay(1);
}

void drawTimer(int x, int y) {
  display.setCursor(x, y);
  display.setTextSize(2);
  display.setTextColor(WHITE);
  time_t t = time_counter;
  struct tm *timeinfo;
  timeinfo = gmtime(&t);
  char buffer[80];
  strftime(buffer, 80, "%M:%S", timeinfo);
  display.println(buffer);
}

void drawPlayButton(int x, int y, int size) {
  float margin = 0.2 * size;
  float b, offset;
  b = (2*size) * 0.71 - margin;
  offset = b / 2;
  display.drawCircle(x, y, size, WHITE);
  display.fillTriangle(x-(int)offset, y-(int)offset,
		       x+(int)offset, y,
		       x-(int)offset, y+(int)offset, WHITE);
}

void drawSkipButton(int x, int y, int size) {
  float margin = 0.2 * size;
  float b, offset, width, height;
  b = (2*size) * 0.71 - margin;
  offset = b / 2;
  width = b / 4;
  height = b;
  display.drawCircle(x, y, size, WHITE);
  display.fillRect(x+(int)offset/2, y-(int)offset, (int)width, (int)height, WHITE);
  display.fillTriangle(x-(int)offset, y-(int)offset,
		       x+(int)offset/2-1, y,
		       x-(int)offset, y+(int)offset, WHITE);
}

void drawStopButton(int x, int y, int size) {
  float margin = 0.2 * size;
  float b, offset;
  b = (2*size) * 0.71 - margin;
  offset = b / 2;
  display.drawCircle(x, y, size, WHITE);
  display.fillRect(x-(int)offset, y-(int)offset, (int)b, (int)b, WHITE);
}

void drawPauseButton(int x, int y, int size) {
  float margin = 0.2 * size;
  float b, offset, width, height;
  b = (2*size) * 0.71 - margin;
  offset = b / 4;
  width = b / 4;
  height = b;
  display.drawCircle(x, y, size, WHITE);
  display.fillRect(x-(int)offset, y-(int)offset*2, (int)width, (int)height, WHITE);
  display.fillRect(x+(int)offset, y-(int)offset*2, (int)width, (int)height, WHITE);
}

void drawSessionCounter(int x, int y, int sessionNo) {
  int width, height, radius, iteration;
  width = 12;
  height = 10;
  radius = 2;
  iteration = width + 5;
  display.drawRoundRect(x, y, width, height, radius, WHITE);
  display.drawRoundRect(x+iteration*1, y, width, height, radius, WHITE);
  display.drawRoundRect(x+iteration*2, y, width, height, radius, WHITE);
  display.drawRoundRect(x+iteration*3, y, width, height, radius, WHITE);
  for (int i = 0; i < sessionNo; ++i) {
    display.fillRoundRect(x+(iteration*i), y, width, height, radius, WHITE);
  }
}

void fillProgressBar(uint32_t baseColor, uint32_t fillColor, uint32_t value) {
  int i, j;
  uint32_t a, b, min, max, normalized;

  a = 0;
  b = ring_led.numPixels();
  min = 0;
  max = max_time_counter;
  normalized = a + ((value - min) * (b - a)) / (max - min);

  Serial.println(b);
  Serial.println(b - normalized);

  for (i = 0; i < b - normalized + 1; ++i) {
    ring_led.setPixelColor(i, fillColor);
  }
  ring_led.show();
}

void setColor(uint32_t color) {
  int size = ring_led.numPixels();
  for (int i = 0; i < size; ++i) {
    ring_led.setPixelColor(i, color);
  }
  ring_led.show();
}

void loop() {
  fsm_timer.run_machine();
  fsm_ringled.run_machine();
  drawDisplay();
}
