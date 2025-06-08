#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Wire.h>
#include "DHTesp.h"
#include <WiFi.h>

#define Screen_width 128
#define Screen_height 64
#define OLED_reset -1
#define Screen_address 0x3C

#define Buzzer 5
#define LED_1 15
#define LED_2 18
#define PB_Cancel 34
#define PB_OK 32
#define PB_UP 33
#define PB_Down 35
#define DHT_PIN 12

#define NTP_SERVER     "pool.ntp.org"
#define UTC_OFFSET     0
#define UTC_OFFSET_DST 0

int current_mode = 0;
int max_mode = 6;
String modes[] = {"1-Set Time","2-Set Alarm_01","3-Set Alarm_02","4-Set Alarm_03","5-Disable Alarms","6-Enable Alarms"};

bool Alarm_enabled = true;
bool Time_setby_user = false;

int hours = 0;
int minutes = 0;
int seconds = 0;

unsigned long timenow = 0;
unsigned long timelast = 0;

int alarm_hours[] = {0, 1, 2};
int alarm_minutes[] = {1, 30, 10};
bool alarm_triggered[] = {false, false, false};
int n_alarms = 3;

Adafruit_SSD1306 display(Screen_width, Screen_height, &Wire, OLED_reset);
DHTesp dhtsensor;

void setup() {
  pinMode(Buzzer, OUTPUT);
  pinMode(LED_1, OUTPUT);
  pinMode(LED_2, OUTPUT);
  pinMode(PB_Cancel, INPUT);
  pinMode(PB_OK, INPUT);
  pinMode(PB_UP, INPUT);
  pinMode(PB_Down, INPUT);

  dhtsensor.setup(DHT_PIN, DHTesp::DHT22);

  Serial.begin(9600);
  if (!display.begin(SSD1306_SWITCHCAPVCC, Screen_address)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;);
  }

  display.display();
  display.clearDisplay();

  printline("Welcome to MediBox!", 2, 0, 10);
  delay(2000);
  display.clearDisplay();

  WiFi.begin("Wokwi-GUEST", "", 6);
  while (WiFi.status() != WL_CONNECTED) {
    printline("Connecting to Wifi..", 2, 0, 0);
    delay(1000);
  }

  display.clearDisplay();
  printline("Connected to Wifi!", 2, 0, 0);

  configTime(UTC_OFFSET, UTC_OFFSET_DST, NTP_SERVER);
}

void loop() {
  if (!Time_setby_user)
    updatetime_internet();
  else
    updatetime_manual();

  display.clearDisplay();
  printtime();

  if (Alarm_enabled) {
    for (int i = 0; i < n_alarms; i++) {
      if (alarm_hours[i] == hours && alarm_minutes[i] == minutes && !alarm_triggered[i]) {
        ring_alarm();
        alarm_triggered[i] = true;
      }
    }
  }

  if (digitalRead(PB_OK) == LOW) {
    delay(200);
    go_to_menu();
  }

  check_temp(); // Optional to implement later
}

void printline(String text, int size, int column, int row) {
  display.setTextSize(size);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(column, row);
  display.println(text);
  display.display();
}

void printtime() {
  printline(String(hours), 2, 20, 0);
  printline(":", 2, 40, 0);
  printline(String(minutes), 2, 50, 0);
  printline(":", 2, 70, 0);
  printline(String(seconds), 2, 80, 0);
}

void updatetime_internet() {
  struct tm timeinfo;
  getLocalTime(&timeinfo);

  hours = timeinfo.tm_hour;
  minutes = timeinfo.tm_min;
  seconds = timeinfo.tm_sec;
}

void updatetime_manual() {
  timenow = millis() / 1000;
  seconds = timenow - timelast;

  if (seconds >= 60) {
    seconds = 0;
    timelast += 60;
    minutes++;
  }

  if (minutes >= 60) {
    minutes = 0;
    hours++;
  }

  if (hours >= 24) {
    hours = 0;
    minutes = 0;
    seconds = 0;
  }
}

void ring_alarm() {
  bool break_h = false;

  display.clearDisplay();
  printline("Time to   take", 2, 0, 10);
  printline("Medicine !", 2, 0, 40);

  while (digitalRead(PB_Cancel) == HIGH && !break_h) {
    digitalWrite(LED_1, HIGH);
    tone(Buzzer, 1000);
    delay(1000);
    noTone(Buzzer);
    digitalWrite(LED_1, LOW);
    delay(1000);

    if (digitalRead(PB_Cancel) == LOW) {
      delay(200);
      break_h = true;
    }
  }
}

int wait_for_user_press() {
  while (true) {
    if (digitalRead(PB_OK) == LOW) {
      delay(200);
      return PB_OK;
    }
    if (digitalRead(PB_UP) == LOW) {
      delay(200);
      return PB_UP;
    }
    if (digitalRead(PB_Down) == LOW) {
      delay(200);
      return PB_Down;
    }
    if (digitalRead(PB_Cancel) == LOW) {
      delay(200);
      return PB_Cancel;
    }

    if (!Time_setby_user)
      updatetime_internet();
    else
      updatetime_manual();
  }
}

void go_to_menu() {
  while (digitalRead(PB_Cancel) == HIGH) {
    display.clearDisplay();
    printline("Menu", 2, 0, 0);
    printline(modes[current_mode], 2, 0, 20);

    int pressed = wait_for_user_press();

    if (pressed == PB_OK) {
      run_mode(current_mode);
    } else if (pressed == PB_UP) {
      current_mode = (current_mode + 1) % max_mode;
    } else if (pressed == PB_Down) {
      current_mode--;
      if (current_mode < 0) current_mode = max_mode - 1;
    } else if (pressed == PB_Cancel) {
      break;
    }
  }
}

void run_mode(int mode) {
  if (mode == 0) set_time();
  else if (mode >= 1 && mode <= 3) set_alarm(mode - 1);
  else if (mode == 4) {
    Alarm_enabled = false;
    display.clearDisplay();
    printline("Alarms Disabled", 2, 0, 10);
    delay(500);
  } else if (mode == 5) {
    Alarm_enabled = true;
    display.clearDisplay();
    printline("Alarms Enabled", 2, 0, 10);
    delay(500);
  }
}

void set_time() {
  int temp_hour = hours;

  while (true) {
    display.clearDisplay();
    printline("Enter hour :" + String(temp_hour), 2, 0, 10);
    int pressed = wait_for_user_press();

    if (pressed == PB_OK) {
      hours = temp_hour;
      break;
    } else if (pressed == PB_UP) {
      temp_hour = (temp_hour + 1) % 24;
    } else if (pressed == PB_Down) {
      temp_hour = (temp_hour - 1 + 24) % 24;
    } else if (pressed == PB_Cancel) {
      break;
    }
  }

  Time_setby_user = true;
  int temp_minute = minutes;

  while (true) {
    display.clearDisplay();
    printline("Enter min :" + String(temp_minute), 2, 0, 10);
    int pressed = wait_for_user_press();

    if (pressed == PB_OK) {
      minutes = temp_minute;
      break;
    } else if (pressed == PB_UP) {
      temp_minute = (temp_minute + 1) % 60;
    } else if (pressed == PB_Down) {
      temp_minute = (temp_minute - 1 + 60) % 60;
    } else if (pressed == PB_Cancel) {
      break;
    }
  }

  display.clearDisplay();
  printline("Time is", 2, 0, 10);
  printline("Set!", 2, 0, 30);
  delay(1000);
}

void set_alarm(int alarm) {
  int temp_hour = hours;

  while (true) {
    display.clearDisplay();
    printline("Enter Alarm hour :" + String(temp_hour), 2, 0, 10);
    int pressed = wait_for_user_press();

    if (pressed == PB_OK) {
      alarm_hours[alarm] = temp_hour;
      break;
    } else if (pressed == PB_UP) {
      temp_hour = (temp_hour + 1) % 24;
    } else if (pressed == PB_Down) {
      temp_hour = (temp_hour - 1 + 24) % 24;
    } else if (pressed == PB_Cancel) {
      break;
    }
  }

  int temp_minute = minutes;

  while (true) {
    display.clearDisplay();
    printline("Enter Alarm min :" + String(temp_minute), 2, 0, 10);
    int pressed = wait_for_user_press();

    if (pressed == PB_OK) {
      alarm_minutes[alarm] = temp_minute;
      break;
    } else if (pressed == PB_UP) {
      temp_minute = (temp_minute + 1) % 60;
    } else if (pressed == PB_Down) {
      temp_minute = (temp_minute - 1 + 60) % 60;
    } else if (pressed == PB_Cancel) {
      break;
    }
  }

  display.clearDisplay();
  printline("Alarm Set!", 2, 0, 20);
  delay(1000);
}

void check_temp() {
  TempAndHumidity data = dhtsensor.getTempAndHumidity();
  Serial.println("Temp: " + String(data.temperature, 2) + "C");
  Serial.println("Humidity: " + String(data.humidity, 1) + "%");
  Serial.println("---");
}
