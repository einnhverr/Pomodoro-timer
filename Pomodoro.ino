#if !defined( ESP32 )
  #error Runs only on ESP32 platform!
#endif

#include <Wire.h>
#include <Adafruit_SSD1306.h>
#include <Fsm.h>
#include <ESPDateTime.h>

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

// Buttons
volatile unsigned long last_interrupt_time = 0L;
volatile unsigned long interrupt_time;
#define BUTTON_LEFT GPIO_NUM_5
#define BUTTON_MIDDLE GPIO_NUM_18
#define BUTTON_RIGHT GPIO_NUM_19

// app variables
int cfg_focus_time = 25 * 60;
int cfg_break_short_time = 5 * 60;
int cfg_break_long_time = 15 * 60;
int cfg_break_counter = 4;


long break_timer;
volatile time_t time_counter = cfg_focus_time;
volatile int focus_counter = 0;

// Finite state machine
// event ids
#define BUTTON_LEFT_EVENT 1
#define BUTTON_MIDDLE_EVENT 2
#define BUTTON_RIGHT_EVENT 3
#define STATE_FINISHED_EVENT 4
#define STATE_CLEAR_EVENT 5

void clear_on_enter();
void clear_on_state();
void clear_on_exit();
void focus_stdby_on_enter();
void focus_stdby_on_state();
void focus_stdby_on_exit();
void focus_on_enter();
void focus_on_state();
void focus_on_exit();
void focus_pause_on_enter();
void focus_pause_on_state();
void focus_pause_on_exit();
void break_stdby_on_enter();
void break_stdby_on_state();
void break_stdby_on_exit();
void break_on_enter();
void break_on_state();
void break_on_exit();

State state_clear(clear_on_enter, &clear_on_state, &clear_on_exit);
State state_focus_stdby(focus_stdby_on_enter, &focus_stdby_on_state, &focus_stdby_on_exit);
State state_focus(focus_on_enter, &focus_on_state, &focus_on_exit);
State state_focus_pause(focus_pause_on_enter, &focus_pause_on_state, &focus_pause_on_exit);
State state_break_stdby(break_stdby_on_enter, &break_stdby_on_state, &break_stdby_on_exit);
State state_break(break_on_enter, &break_on_state, &break_on_exit);
Fsm fsm(&state_clear);

// ISR
bool IRAM_ATTR countDown(void *timerNo) {
  time_counter--;

  return true;
}

void IRAM_ATTR push_left() {
  interrupt_time = millis();
  if (interrupt_time - last_interrupt_time > 250) {
    fsm.trigger(BUTTON_LEFT_EVENT);
  }
  last_interrupt_time = interrupt_time;
}

void IRAM_ATTR push_middle() {
  interrupt_time = millis();
  if (interrupt_time - last_interrupt_time > 250) {
    fsm.trigger(BUTTON_MIDDLE_EVENT);
    }
  last_interrupt_time = interrupt_time;
}

void IRAM_ATTR push_right() {
  interrupt_time = millis();
  if (interrupt_time - last_interrupt_time > 250) {
    fsm.trigger(BUTTON_RIGHT_EVENT);
  }
  last_interrupt_time = interrupt_time;
}

// FSM callback functions
void clear_on_enter() {
  focus_counter = 0;
}

void clear_on_state() {
  fsm.trigger(STATE_CLEAR_EVENT);
}

void clear_on_exit() {
}

void focus_stdby_on_enter() {
  time_counter = cfg_focus_time;
}

void focus_stdby_on_state() {
}

void focus_stdby_on_exit() {
}

void focus_on_enter() {
  ITimer.restartTimer();
}

void focus_on_state() {
  if (time_counter == 0) {
    ITimer.stopTimer();
    focus_counter++;
    fsm.trigger(STATE_FINISHED_EVENT);
  }
}

void focus_on_exit() {
  ITimer.stopTimer();
}

void focus_pause_on_enter() {
}

void focus_pause_on_state() {
}

void focus_pause_on_exit() {
}

void break_stdby_on_enter() {
  if (focus_counter < 4) {
    time_counter = cfg_break_short_time;
  } else {
    time_counter = cfg_break_long_time;
  }
}

void break_stdby_on_state() {

}

void break_stdby_on_exit() {
}

void break_on_enter() {
  ITimer.restartTimer();
}

void break_on_state() {
  if (time_counter == 0) {
    ITimer.stopTimer();
    fsm.trigger(STATE_FINISHED_EVENT);
  }
}

void break_on_exit() {
  if (focus_counter == 4) {
    focus_counter = 0;
  }
  ITimer.stopTimer();
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
  delay(2000);

  // clear buffer
  display.clearDisplay();

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

  // FSM transitions
  fsm.add_transition(&state_clear, &state_focus_stdby, STATE_CLEAR_EVENT, NULL);

  fsm.add_transition(&state_focus_stdby, &state_focus, BUTTON_LEFT_EVENT, NULL);
  fsm.add_transition(&state_focus_stdby, &state_clear, BUTTON_RIGHT_EVENT, NULL);

  fsm.add_transition(&state_focus, &state_focus_pause, BUTTON_MIDDLE_EVENT, NULL);
  fsm.add_transition(&state_focus, &state_break_stdby, BUTTON_LEFT_EVENT, NULL);
  fsm.add_transition(&state_focus, &state_clear, BUTTON_RIGHT_EVENT, NULL);
  fsm.add_transition(&state_focus, &state_break_stdby, STATE_FINISHED_EVENT, NULL);
  
  fsm.add_transition(&state_focus_pause, &state_clear, BUTTON_RIGHT_EVENT, NULL);
  fsm.add_transition(&state_focus_pause, &state_focus, BUTTON_MIDDLE_EVENT, NULL);
  fsm.add_transition(&state_focus_pause, &state_break_stdby, BUTTON_LEFT_EVENT, NULL);
  
  fsm.add_transition(&state_break_stdby, &state_break, BUTTON_LEFT_EVENT, NULL);
  fsm.add_transition(&state_break_stdby, &state_clear, BUTTON_RIGHT_EVENT, NULL);

  fsm.add_transition(&state_break, &state_focus_stdby, BUTTON_LEFT_EVENT, NULL);
  fsm.add_transition(&state_break, &state_focus_stdby, STATE_FINISHED_EVENT, NULL);

  // initial counter value
  time_counter = cfg_focus_time;
}

void drawDisplay() {
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.setCursor(0,0);
  time_t t = time_counter;
  struct tm *timeinfo;
  timeinfo = gmtime(&t);
  char buffer[80];
  strftime(buffer, 80, "%M:%S", timeinfo);
  display.println(buffer);
  display.println(focus_counter);
  display.display();
}

void loop() {
  fsm.run_machine();
  drawDisplay();
  delay(40);
}
