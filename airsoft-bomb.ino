#include <avr/sleep.h>

#include <Keypad.h>
#include <LiquidCrystal.h>
#include <Tone.h>

#include "strings/en.h"

#define ARRSIZE(a) ((sizeof a) / (sizeof a[0]))
#define STRLEN(s) (ARRSIZE(s) - 1)

#define BAUD_RATE 9600

#define SPEAKER_PIN 9
#define TIMER_NOTE NOTE_A2
#define INPUT_NOTE NOTE_C6
#define ERROR_NOTE NOTE_C2
#define JINGLE_NOTE_1 NOTE_A2
#define JINGLE_NOTE_2 NOTE_C2

Tone speaker;

#define LCD_W 16
#define LCD_H 2
#define LCD_CENTER(w) ((LCD_W - (w)) / 2)

#define PIN_SIZE 4
#define TIME_SIZE 8
#define TIME_SHORT_SIZE 5

#define PIN_POS LCD_CENTER(PIN_SIZE)
#define TIME_POS LCD_CENTER(TIME_SIZE)
#define TIME_SHORT_POS LCD_CENTER(TIME_SHORT_SIZE)
#define TIMEOUT_POS (STRLEN(S_TIME) + 1)

LiquidCrystal lcd(7, 8, 10, 11, 12, 13);

#define ROWS 4
#define COLS 3

char keys[ROWS][COLS] = {
  {'1', '2', '3'},
  {'4', '5', '6'},
  {'7', '8', '9'},
  {'*', '0', '#'}
};

byte rowPins[ROWS] = {5, A5, A4, A2};
byte colPins[COLS] = {A1, A0, A3};

Keypad keypad(
  makeKeymap(keys),
  rowPins,
  colPins,
  ROWS,
  COLS
);

byte h, m, s;

byte dec_hms()
{
  if (s == 0) {
    if (m == 0) {
      if (h == 0) {
        return 1;
      }

      m = 60;
      h--;
    }

    s = 60;
    m--;
  }

  s--;

  return 0;
}

unsigned long last_update = 0;
unsigned long last_beep = 0;
unsigned long beep_interval;

struct {
  byte h, m, s;
  unsigned long interval;
} beep_table[] = {
  {0,  0, 10,  250},
  {0,  0, 30,  500},
  {0,  0, 60, 1000},
  {0,  5,  0, 2000}
};

#define DEFAULT_BEEP_INTERVAL 4000

void update_beep_interval()
{
  byte i;
  for (i = 0; i < ARRSIZE(beep_table); i++) {
    if (h > beep_table[i].h) continue;
    if (h < beep_table[i].h) break;

    if (m > beep_table[i].m) continue;
    if (m < beep_table[i].m) break;

    if (s < beep_table[i].s) break;
  }

  beep_interval = i < ARRSIZE(beep_table)
                ? beep_table[i].interval
                : DEFAULT_BEEP_INTERVAL;
}

#define INPUT_DONE      0
#define INPUT_NONE      1
#define INPUT_STORE     2
#define INPUT_REJECTED  3

byte len;

byte get_input(char *buf, byte size, const char *mask)
{
  char key = keypad.getKey();

  switch (key) {
  case NO_KEY:
    return INPUT_NONE;

  case '*':
    if (len > 0) {
      buf[--len] = '\0';
      return INPUT_STORE;
    }
    break;

  case '#':
    if (len == size) {
      return INPUT_DONE;
    }
    break;

  default:
    if (len < size &&
        (mask == NULL || key <= mask[len])) {

      buf[len++] = key;
      return INPUT_STORE;
    }
  }

  return INPUT_REJECTED;
}

byte parse_hms(const char *buf)
{
  sscanf(buf, "%2hhu%2hhu", &h, &m);
  s = 0;
}

char pin[PIN_SIZE + 1];
char input[PIN_SIZE + 1];

void setup()
{
  Serial.begin(BAUD_RATE);
  speaker.begin(SPEAKER_PIN);
  lcd.begin(LCD_W, LCD_H);

  char buf[TIME_SIZE + 1];

  lcd.noCursor();
  lcd.clear();
  lcd.print(S_ENTER_TIMEOUT);

  len = 0;
  memset(buf, '\0', sizeof buf);
  print_time_input(buf);

  byte ret;
  while ((ret = get_input(buf, 4, "9959"))
         || (parse_hms(buf), dec_hms())) {
    if (ret == INPUT_STORE) {
      speaker.play(INPUT_NOTE, 200);
      print_time_input(buf);
    } else if (ret != INPUT_NONE) {
      speaker.play(ERROR_NOTE, 200);
    }
  }

  speaker.play(INPUT_NOTE, 1000);
  lcd.noCursor();
  delay(500);

  lcd.clear();
  delay(500);

  lcd.print(S_TIMEOUT_ENTERED);
  sprintf(buf, "%02hhu:%02hhu", h, m);
  lcd.setCursor(TIME_SHORT_POS, 1);
  lcd.print(buf);

  lcd.clear();
  lcd.print(S_ENTER_PIN);

  len = 0;
  memset(pin, '\0', sizeof pin);
  print_pin_input(pin);

  while ((ret = get_input(pin, PIN_SIZE, NULL))) {
    if (ret == INPUT_STORE) {
      speaker.play(INPUT_NOTE, 200);
      print_pin_input(pin);
    } else if (ret == INPUT_REJECTED) {
      speaker.play(ERROR_NOTE, 200);
    }
  }

  speaker.play(INPUT_NOTE, 1000);
  lcd.noCursor();
  delay(500);

  lcd.clear();
  delay(500);

  lcd.print(S_PIN_ENTERED);
  lcd.setCursor(PIN_POS, 1);
  lcd.print(pin);

  delay(3000);

  beep_interval = 4000;
  len = 0;
  memset(input, '\0', sizeof pin);

  lcd.clear();
  lcd.print(S_TIME);
}

void print_time_input(const char *time)
{
  lcd.noCursor();

  char buf[TIME_SIZE + 1];
  sprintf(buf, "%-2.2s:%-2.2s", time, time+2);
  lcd.setCursor(TIME_SHORT_POS, 1);
  lcd.print(buf);

  byte l = strlen(time);
  lcd.setCursor(TIME_SHORT_POS + (l >= 2 ? l+1 : l), 1);
  lcd.cursor();
}

void loop()
{
  byte ret = get_input(input, PIN_SIZE, NULL);
  if (ret == INPUT_STORE) {
    speaker.play(INPUT_NOTE, 200);
    print_pin_input(input);
  } else if (ret == INPUT_REJECTED) {
    speaker.play(ERROR_NOTE, 200);
  } else if (ret == INPUT_DONE) {
    if (strcmp(input, pin) != 0) {
      gameover();
    } else {
      win();
    }
  }

  unsigned long time = millis();

  if (time - last_update >= 1000) {
    update_timer();
    last_update = time;
  }

  if (time - last_beep >= beep_interval) {
    speaker.play(TIMER_NOTE, 200);
    last_beep = time;
  }
}

void print_pin_input(const char *buf)
{
  lcd.noCursor();

  lcd.setCursor(PIN_POS, 1);
  byte n = lcd.print(buf);

  byte i = PIN_SIZE - n;
  while (i-- > 0) {
      lcd.print(' ');
  }

  lcd.setCursor(PIN_POS + n, 1);
  lcd.cursor();
}

void update_timer()
{
  if (dec_hms()) {
    gameover();
  }

  update_beep_interval();

  char output[9];
  sprintf(output, "%02hhu:%02hhu:%02hhu", h, m, s);

  Serial.println(output);

  lcd.noCursor();
  lcd.setCursor(TIMEOUT_POS, 0);
  lcd.print(output);

  lcd.setCursor(PIN_POS + len, 1);
  lcd.cursor();
}

void win()
{
  lcd.noCursor();
  lcd.clear();
  lcd.print(S_DEFUSED_1);
  lcd.setCursor(0, 1);
  lcd.print(S_DEFUSED_2);

  speaker.play(JINGLE_NOTE_1, 250);
  delay(250);
  speaker.play(JINGLE_NOTE_2, 250);
  delay(250);
  speaker.play(JINGLE_NOTE_1, 250);
  delay(250);
  speaker.play(JINGLE_NOTE_2, 750);
  delay(750);

  delay(5750);

  poweroff();
}

void gameover()
{
  lcd.noCursor();
  lcd.clear();
  lcd.print(S_WENT_OFF_1);
  lcd.setCursor(0, 1);
  lcd.print(S_WENT_OFF_2);

  speaker.play(TIMER_NOTE, 5000);
  delay(10000);

  poweroff();
}

void poweroff()
{
  sleep_enable();
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  digitalWrite(LED_BUILTIN, LOW);
  delay(500);

  cli();
  for (;;) {
    sleep_cpu();
  }
}
