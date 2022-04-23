#include <avr/pgmspace.h>
#include <SPI.h>
#include "HUB08SPI.h"
#include "TimerOne.h"
#include "Buffer.h"
#include "RTClib.h"
#include <stdlib.h>
#include <EEPROM.h>
#include <PrayerTimes.h>
#include <Rotary.h>


#define SW_PIN A3
#define BUZZER_PIN 8
#define WIDTH   64 // width of led matrix (pixel)
#define HEIGHT  16 // height of led matrix
#define LONG_PRESS_DURATION 600
#define SEN_PIN A0


#define RST_TAMPILAN digitalWrite(BUZZER_PIN, LOW); \
  first_run_clock = true; \
  first_run_jadwal = true; \
  buff.clear(); \
  ls_cursor_pos = 50; \
  update_time();

HUB08SPI display;
uint8_t displaybuf[WIDTH * HEIGHT / 8];
Buffer buff(displaybuf, 64, 16);

//make sure to put after create buffer
#include "ronnAnimation.h"

uint8_t n_jam, n_menit, n_detik, n_tanggal, n_bulan, n_temperature;
uint16_t n_tahun = 0;
uint8_t ls_tanggal = 0;
uint8_t n_suhu;
uint8_t cursor_pos = 1;
uint8_t ls_cursor_pos = 0;
uint8_t ls_detik = 0;
bool menu_on = false;
int hours;
int minutes;
bool setting_menu = true;
uint16_t count_time = 0;
uint8_t show_step = 0;
uint8_t ls_show_step = 1;
char str_buffer[20];
bool first_run_clock = true;
bool first_run_jadwal = true;
uint8_t show_pos = 0;
uint8_t ls_show_pos = 0;
uint32_t pvTime = false;

// PrayerTimes
double times[sizeof(TimeName) / sizeof(char*)];

Rotary rotary = Rotary(A1, A2);
RTC_DS3231 rtc;


uint8_t waktu_sholat[5][2] = {{9, 9}, {9, 9}, {9, 9}, {9, 9}, {0, 0}};

typedef struct ALARM_STRUCT {
  uint8_t minute = 0;
  uint8_t hour = 0;
  bool state = false;
} ALARM_STRUCT;

struct SETTING_DATA {
  ALARM_STRUCT alarm1;
  ALARM_STRUCT alarm2;
  float latitude_val = -8.121262f;
  float longitude_val = 112.205429f;
  uint8_t bright = 100;
  bool alarm_sholat_state;
  bool tampilkan_jadwal_state;
  bool beep_state;
};

SETTING_DATA set_data;



void refresh() {
  display.scan();  //refresh a single line of the display
}


void setup()
{

  //  Serial.begin(9600);
  //EEPROM_WRITE();
  EEPROM_READ();

  display.begin(displaybuf, WIDTH, HEIGHT);
  Timer1.initialize(100);  //800 us -> refresh rate 1250 Hz per line / 16 = 78 fps
  Timer1.attachInterrupt(refresh);
  display.setBrightness(set_data.bright);     //low brightness to save energy
  buff.clear();                   //clear display led matrix

  pinMode(SW_PIN, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);

  if (! rtc.begin()) {
    abort();
  }

  //  rtc.adjust(DateTime(2022, 4, 22, 17, 25, 55));

}

void loop()
{

  update_time();
  get_temperature();

  check_alarm();
  check_alarm_sholat();

  if (show_pos != ls_show_pos)
  {
    buff.clear();
    ls_show_pos = show_pos;
  }

  switch (show_pos)
  {
    case 0:
      display_s_clock(first_run_clock);
      if (set_data.tampilkan_jadwal_state == true)
      {
        if (millis() - pvTime >= 20000)
        {
          show_pos = 1;
          pvTime = millis();
          first_run_jadwal = true;
        }
      } else
      {
        pvTime = millis();
      }
      break;
    case 1:
      pvTime = millis();
      if (set_data.tampilkan_jadwal_state == false)
      {
        show_pos = 0;
      }
      display_jadwal(show_pos);
      break;
  }

  if (digitalRead(SW_PIN) == LOW)
  {
    set_buzzer(2, 50, 100);
    buff.clear();
    while (digitalRead(SW_PIN) == LOW);
    delay(10);
    setting_menu = true;

    while (setting_menu)
    {
      unsigned char result = rotary.process();
      if (result == DIR_CW) {
        if (cursor_pos == 11)
        {
          cursor_pos = 1;
        } else
        {
          cursor_pos++;
        }
      } else if (result == DIR_CCW) {
        if (cursor_pos == 1)
        {
          cursor_pos = 11;
        } else
        {
          cursor_pos--;
        }
      }

      if (digitalRead(SW_PIN) == LOW)
      {
        menu_on = true;
        count_time = 0;
        while (digitalRead(SW_PIN) == LOW)
        {
          count_time++;
          if (count_time > LONG_PRESS_DURATION)
          {
            cursor_pos = 11;
            break;
          }
          delay(1);
        }
        delay(10);
      }

      if (menu_on)
      {
        menu_display();
        buff.clear();
      }

      if (cursor_pos != ls_cursor_pos )
      {
        ls_cursor_pos = cursor_pos;
        set_buzzer(1, 50, 0 );
        ronn.clear_L(0, 0, 64, 16, SCROLL);
        menu_display();
      }

      special_menu_display();

      if (menu_on == true && cursor_pos == 11)
      {
        first_run_clock = true;
        delay(100);
        cursor_pos = 1;
        ls_cursor_pos = 0;
        menu_on = false;
        buff.clear();
        pvTime = millis();
        break;
      }

      check_alarm();
      check_alarm_sholat();
    }
  }
}

void EEPROM_READ()
{
  EEPROM.get(0, set_data);

  //  Serial.println(set_data.alarm1.minute);
  //  Serial.println(set_data.alarm1.hour);
  //  Serial.println(set_data.alarm1.state);
  //  Serial.println(set_data.alarm2.minute);
  //  Serial.println(set_data.alarm2.hour);
  //  Serial.println(set_data.alarm2.state);
  //  Serial.println(set_data.latitude_val, 6);
  //  Serial.println(set_data.longitude_val, 6);
  //  Serial.println(set_data.bright);
  //  Serial.println(set_data.alarm_sholat_state);
  //  Serial.println(set_data.tampilkan_jadwal_state);
  //  Serial.println(set_data.beep_state);
}

void EEPROM_WRITE()
{
  EEPROM.put(0, set_data);
}

void get_temperature()
{
  static uint32_t pvTime;
  uint32_t curTime = millis();
  static double data_temp;
  static uint8_t i;

  if (curTime - pvTime >= 50)
  {
    pvTime = curTime;
    if (i == 0)
    {
      data_temp = 0;
    }

    if (i < 100)
    {
      ++i;
      data_temp += ((double)analogRead(SEN_PIN) / 1023.0) * 5000.0 / 10.0;
    }
    else
    {
      i = 0;
      n_suhu = data_temp / 100.0;
      data_temp = 0;
    }
  }
}

void check_alarm_sholat()
{
  static bool sudah_on;
  static uint8_t ls_menit;

  DateTime now = rtc.now();
  n_menit = now.minute();

  if (n_menit != ls_menit)
  {
    ls_menit = n_menit;
    sudah_on = false;
  }

  if (set_data.alarm_sholat_state && sudah_on == false)
  {
    if (waktu_sholat[0][0] == n_jam && waktu_sholat[0][1] == n_menit)
    {
      uint32_t pvTime2 = millis();
      uint32_t pvTime = millis();

      bool show = true;
      buff.clear();
      ronn.setFont(font_DEFAULT);
      ronn.printText("MASUK WAKTU", 0, 0);
      ronn.printText("SUBUH", 0, 8);
      while (millis() - pvTime2 < 7000)
      {
        if (millis() - pvTime >= 300)
        {
          pvTime = millis();
          digitalWrite(BUZZER_PIN, !digitalRead(BUZZER_PIN));
        }
        if (digitalRead(SW_PIN) == LOW) {
          delay(200);
          break;
        }
      }
      sudah_on = true;
      RST_TAMPILAN
    }
    else if (waktu_sholat[1][0] == n_jam && waktu_sholat[1][1] == n_menit)
    {
      uint32_t pvTime2 = millis();
      uint32_t pvTime = millis();

      buff.clear();
      ronn.setFont(font_DEFAULT);
      ronn.printText("MASUK WAKTU", 0, 0);
      ronn.printText("DZUHUR", 0, 8);
      while (millis() - pvTime2 < 7000)
      {
        if (millis() - pvTime >= 300)
        {
          pvTime = millis();
          digitalWrite(BUZZER_PIN, !digitalRead(BUZZER_PIN));
        }
        if (digitalRead(SW_PIN) == LOW) {
          delay(200);
          break;
        }
      }
      sudah_on = true;
      RST_TAMPILAN
    }
    else if (waktu_sholat[2][0] == n_jam && waktu_sholat[2][1] == n_menit)
    {
      uint32_t pvTime2 = millis();
      uint32_t pvTime = millis();

      buff.clear();
      ronn.setFont(font_DEFAULT);
      ronn.printText("MASUK WAKTU", 0, 0);
      ronn.printText("ASHAR", 0, 8);
      while (millis() - pvTime2 < 7000)
      {
        if (millis() - pvTime >= 300)
        {
          pvTime = millis();
          digitalWrite(BUZZER_PIN, !digitalRead(BUZZER_PIN));
        }
        if (digitalRead(SW_PIN) == LOW) {
          delay(200);
          break;
        }
      }
      sudah_on = true;
      RST_TAMPILAN
    }
    else if (waktu_sholat[3][0] == n_jam && waktu_sholat[3][1] == n_menit)
    {
      uint32_t pvTime2 = millis();
      uint32_t pvTime = millis();
      buff.clear();
      ronn.setFont(font_DEFAULT);
      ronn.printText("MASUK WAKTU", 0, 0);
      ronn.printText("MAGRIB", 0, 8);
      while (millis() - pvTime2 < 7000)
      {
        if (millis() - pvTime >= 300)
        {
          pvTime = millis();
          digitalWrite(BUZZER_PIN, !digitalRead(BUZZER_PIN));
        }
        if (digitalRead(SW_PIN) == LOW) {
          delay(200);
          break;
        }
      }
      sudah_on = true;
      RST_TAMPILAN
    }
    else if (waktu_sholat[4][0] == n_jam && waktu_sholat[4][1] == n_menit)
    {
      uint32_t pvTime2 = millis();
      uint32_t pvTime = millis();
      buff.clear();
      ronn.setFont(font_DEFAULT);
      ronn.printText("MASUK WAKTU", 0, 0);
      ronn.printText("ISYA'", 0, 8);
      while (millis() - pvTime2 < 7000)
      {
        if (millis() - pvTime >= 300)
        {
          pvTime = millis();
          digitalWrite(BUZZER_PIN, !digitalRead(BUZZER_PIN));
        }
        if (digitalRead(SW_PIN) == LOW) {
          delay(200);
          break;
        }
      }
      sudah_on = true;
      RST_TAMPILAN
    }
  }
}

void update_time()
{
  DateTime now = rtc.now();
  n_tanggal = now.day();
  n_bulan = now.month();
  n_tahun = now.year();
  n_detik = now.second();
  n_jam = now.hour();
  n_menit = now.minute();
}

void check_alarm()
{
  static bool sudah_on;
  static uint8_t ls_menit;

  DateTime now = rtc.now();
  n_menit = now.minute();

  if (n_menit != ls_menit)
  {
    ls_menit = n_menit;
    sudah_on = false;
  }

  if (set_data.alarm1.state && sudah_on == false)
  {
    if (set_data.alarm1.hour == n_jam)
    {
      if (set_data.alarm1.minute == n_menit)
      {
        uint32_t pvTime2 = millis();
        uint32_t pvTime = millis();

        bool show = true;
        buff.clear();
        ronn.setFont(B_7SEGMENT);
        sprintf(str_buffer, "%02d:%02d", n_jam, n_menit);

        ronn.printText(str_buffer, 9, 0);
        while (millis() - pvTime2 < 20000)
        {
          if (millis() - pvTime >= 800)
          {
            pvTime = millis();
            digitalWrite(BUZZER_PIN, !digitalRead(BUZZER_PIN));

            show = !show;

            if (show)
            {
              buff.clear();
              ronn.setFont(B_7SEGMENT);
              ronn.printText(str_buffer, 9, 0);
            } else {
              buff.clear();
              ronn.setFont();
              ronn.printText("ALARM 1 ON", 4, 4);
            }
          }
          if (digitalRead(SW_PIN) == LOW) {
            delay(200);
            break;
          }
        }
        ronn.setFont(font_DEFAULT);
        sudah_on = true;
        digitalWrite(BUZZER_PIN, LOW);
        first_run_clock = true;
        first_run_jadwal = true;
        buff.clear();
        update_time();
        ls_cursor_pos = 50;
      }

    }
  }

  if (set_data.alarm2.state && sudah_on == false)
  {
    if (set_data.alarm2.hour == n_jam)
    {
      if (set_data.alarm2.minute == n_menit)
      {
        uint32_t pvTime2 = millis();
        uint32_t pvTime = millis();

        bool show = true;
        buff.clear();
        ronn.setFont(B_7SEGMENT);
        sprintf(str_buffer, "%02d:%02d", n_jam, n_menit);

        ronn.printText(str_buffer, 9, 0);
        while (millis() - pvTime2 < 20000)
        {
          if (millis() - pvTime >= 800)
          {
            pvTime = millis();
            digitalWrite(BUZZER_PIN, !digitalRead(BUZZER_PIN));

            show = !show;

            if (show)
            {
              buff.clear();
              ronn.setFont(B_7SEGMENT);
              ronn.printText(str_buffer, 9, 0);
            } else {
              buff.clear();
              ronn.setFont();
              ronn.printText("ALARM 2 ON", 4, 4);
            }
          }
          if (digitalRead(SW_PIN) == LOW) {
            delay(200);
            break;
          }
        }
        ronn.setFont(font_DEFAULT);
        sudah_on = true;
        digitalWrite(BUZZER_PIN, LOW);
        first_run_clock = true;
        first_run_jadwal = true;
        buff.clear();
        update_time();
        ls_cursor_pos = 50;
      }
    }
  }
}

void special_menu_display()
{
  if (cursor_pos == 4 && menu_on == false)
  {

    DateTime now = rtc.now();
    n_detik = now.second();
    n_jam = now.hour();
    n_menit = now.minute();

    if (n_detik != ls_detik)
    {
      ls_detik = n_detik;
      sprintf(str_buffer, "%02d:%02d:%02d", n_jam, n_menit, n_detik);
      ronn.printText(str_buffer, 0, 8);
    }
  }

  if (cursor_pos == 5 && menu_on == false)
  {

    DateTime now = rtc.now();
    n_tanggal = now.day();
    n_bulan = now.month();
    n_tahun = now.year();

    if (n_tanggal != ls_tanggal)
    {
      ls_tanggal = n_tanggal;
      sprintf(str_buffer, "%02d-%02d-%02d", n_tanggal, n_bulan, n_tahun % 100);
      ronn.printText(str_buffer, 0, 8);
    }
  }
}

void menu_display()
{

  switch (cursor_pos)
  {
    case 1:
      ronn.printText("ALARM 1", 0, 0);
      if (menu_on)
      {
        ronn.printText("->", 0, 8);
        set_alarm_1();
        ls_cursor_pos = 0;
        menu_on = false;
      } else {
        if (set_data.alarm1.state == 1)
        {
          sprintf(str_buffer, "%02d:%02d", set_data.alarm1.hour, set_data.alarm1.minute);
          ronn.printText(str_buffer, 0, 8);
        } else {
          ronn.printText("OFF", 0, 8);
        }
      }
      break;
    case 2:
      ronn.printText("ALARM 2", 0, 0);
      if (menu_on)
      {
        ronn.printText("->", 0, 8);
        set_alarm_2();
        ls_cursor_pos = 0;
        menu_on = false;
      } else {
        if (set_data.alarm2.state == 1)
        {
          sprintf(str_buffer, "%02d:%02d", set_data.alarm2.hour, set_data.alarm2.minute);
          ronn.printText(str_buffer, 0, 8);
        } else {
          ronn.printText("OFF", 0, 8);
        }
      }
      break;
    case 3:
      ronn.printText("ALRM SHOLAT", 0, 0);
      if (menu_on)
      {
        ronn.printText("->;", 0, 8);
        set_alarm_sholat();
        ls_cursor_pos = 0;
        menu_on = false;
      } else {
        if (set_data.alarm_sholat_state)
        {
          ronn.printText("ON;", 0, 8);
        }
        else
        {
          ronn.printText("OFF", 0, 8);
        }
      }

      break;
    case 4:
      ronn.printText("SET WAKTU", 0, 0);
      if (menu_on) {
        ronn.printText("->", 0, 8);
        set_clock();
        menu_on = false;
        ls_cursor_pos = 0;
      } else {
        DateTime now = rtc.now();
        n_detik = now.second();
        n_jam = now.hour();
        n_menit = now.minute();

        ls_detik = n_detik;
        sprintf(str_buffer, "%02d:%02d:%02d", n_jam, n_menit, n_detik);
        ronn.printText(str_buffer, 0, 8);
      }
      break;
    case 5:
      ronn.printText("SET TANGGAL", 0, 0);
      if (menu_on) {
        ronn.printText("->", 0, 8);
        set_date();
        menu_on = false;
        ls_cursor_pos = 0;
      } else {
        DateTime now = rtc.now();
        n_tanggal = now.day();
        n_bulan = now.month();
        n_tahun = now.year();

        sprintf(str_buffer, "%02d-%02d-%02d", n_tanggal, n_bulan, n_tahun % 100);
        ronn.printText(str_buffer, 0, 8);
      }

      break;
    case 6:
      ronn.printText("Tmpilkn Jdwl.", 0, 0);
      if (menu_on) {
        ronn.printText("->", 0, 8);
        set_jadwal();
        menu_on = false;
        ls_cursor_pos = 0;
      }
      else {
        if (set_data.tampilkan_jadwal_state) {
          ronn.printText("ON;", 0, 8);
        }
        else {
          ronn.printText("OFF", 0, 8);
        }
      }

      break;
    case 7:
      ronn.printText("SET Latitude", 0, 0);
      if (menu_on) {
        set_koordinat(set_data.latitude_val);
        menu_on = false;
        ls_cursor_pos = 0;
      } else {
        show_koordinat(set_data.latitude_val);
      }
      break;
    case 8:
      ronn.printText("SET Longitude", 0, 0);
      if (menu_on) {
        set_koordinat(set_data.longitude_val);
        menu_on = false;
        ls_cursor_pos = 0;
      } else {
        show_koordinat(set_data.longitude_val);
      }
      break;
    case 9:
      ronn.printText("SET Bright", 0, 0);
      if (menu_on) {
        ronn.printText("->", 0, 8);
        set_brightness(set_data.bright);
        menu_on = false;
        ls_cursor_pos = 0;
      } else {
        sprintf(str_buffer, "%03d", set_data.bright);
        ronn.printText(str_buffer, 0, 8);
      }
      break;
    case 10:
      ronn.printText("SET Beep", 0, 0);
      if (menu_on) {
        ronn.printText("->", 0, 8);
        set_beep(set_data.beep_state);
        menu_on = false;
        ls_cursor_pos = 0;
      } else {
        if (set_data.beep_state) {
          ronn.printText("ON;", 0, 8);
        }
        else {
          ronn.printText("OFF", 0, 8);
        }
      }
      break;
    case 11:
      ronn.printText("KELUAR", 0, 0);
      break;
  }
}

void set_beep(bool &value)
{
  uint16_t count_time = 0;
  bool exit_state = false;

  if (value) {
    ronn.printText("ON;", 9, 8);
  }
  else {
    ronn.printText("OFF", 9, 8);
  }

  while (1)
  {

    if (digitalRead(SW_PIN) == LOW)
    {
      count_time = 0;
      while (digitalRead(SW_PIN) == LOW)
      {
        count_time++;
        if (count_time > 600)
        {
          exit_state = true;
          break;
        }
        delay(1);
      }
      delay(10);
      if (exit_state)
      {
        buff.clear();
        ronn.printText("OK", 0, 0);
        set_buzzer(1, 100, 0);
        delay(100);
        buff.clear();
        break;
      }
    }


    unsigned char result = rotary.process();
    if (result == DIR_CW || result == DIR_CCW)
    {
      value = !value;

      if (value) {
        ronn.printText("ON;", 9, 8);
      }
      else {
        ronn.printText("OFF", 9, 8);
      }
      EEPROM_WRITE();
    }
  }

}

void set_brightness(uint8_t &value)
{
  uint16_t count_time = 0;
  bool exit_state = false;
  uint8_t ls_value = 0;

  while (1)
  {
    if (digitalRead(SW_PIN) == LOW)
    {
      count_time = 0;


      while (digitalRead(SW_PIN) == LOW)
      {
        count_time++;
        if (count_time > 600)
        {
          exit_state = true;
          break;
        }
        delay(1);
      }
      delay(10);
      if (exit_state)
      {
        buff.clear();
        ronn.printText("OK", 0, 0);
        set_buzzer(1, 100, 0);
        delay(100);
        buff.clear();
        break;
      }
    }

    unsigned char result = rotary.process();

    if (result == DIR_CW && value < 255) {
      value++;
    } else if (result == DIR_CCW && value > 50) {
      value--;
    }

    if (value != ls_value) {
      display.setBrightness(set_data.bright);
      sprintf(str_buffer, "%03d", value);
      ronn.printText(str_buffer, 9, 8);
      EEPROM_WRITE();
      ls_value = value;
    }
  }

}

void show_koordinat(float &value)
{
  String dtFloat = String((value < 0.0 ? value * -1.0 : value), 6);

  if (value < 0.0)
  {
    ronn.printText("-", 0, 8);
  } else {
    ronn.printText("+", 0, 8);
  }

  if ((value < 0.0 ? value * -1.0 : value) < 10.0)
  {
    dtFloat = String("00") + dtFloat;
    ronn.printText(dtFloat.c_str(), 6, 8);
  }
  else if ((value < 0.0 ? value * -1.0 : value) < 100.0)
  {
    dtFloat = String("0") + dtFloat;
    ronn.printText(dtFloat.c_str(), 6, 8);
  }
  else
  {
    ronn.printText(dtFloat.c_str(), 6, 8);
  }
}

void set_koordinat(float & value)
{
  uint16_t count_time = 0;
  bool exit_state = false;
  uint8_t digit = 0;
  double select_val = 0.0;
  bool show_state = 1;
  bool ls_show_state = 0;
  uint32_t pvTime = 0;
  char str_buff[20];
  char str_temp[20];

  while (1)
  {
    uint32_t n_time = millis();
    if (digitalRead(SW_PIN) == LOW)
    {
      count_time = 0;

      if (digit == 9)
      {
        digit = 0;
      } else {
        digit++;
      }

      while (digitalRead(SW_PIN) == LOW)
      {
        count_time++;
        if (count_time > 600)
        {
          exit_state = true;
          break;
        }
        delay(1);
      }
      delay(10);
      if (exit_state)
      {
        buff.clear();
        ronn.printText("OK", 0, 0);
        set_buzzer(1, 100, 0);
        delay(100);
        buff.clear();
        break;
      }
    }

    if (n_time - pvTime >= 250)
    {
      show_state = false;
      if (n_time - pvTime >= 500)
      {
        pvTime = n_time;
      }
    } else {
      show_state = true;
    }


    unsigned char result = rotary.process();

    if (digit == 0) {
      select_val = 0.000001;
    }
    else if (digit == 1) {
      select_val = 0.00001;
    }
    else if (digit == 2) {
      select_val = 0.0001;
    }
    else if (digit == 3) {
      select_val = 0.001;
    }
    else if (digit == 4) {
      select_val = 0.01;
    }
    else if (digit == 5) {
      select_val = 0.1;
    }
    else if (digit == 6) {
      select_val = 1.0;
    }
    else if (digit == 7) {
      select_val = 10.0;
    }
    else if (digit == 8) {
      select_val = 100.0;
    } else {
      select_val = 0;
    }


    if (result == DIR_CW)
    {
      if (digit == 9)
      {
        value *= -1.0;
      }
      else
      {
        value += select_val;
      }
    } else if (result == DIR_CCW) {

      if (digit == 9)
      {
        value *= -1.0;
      }
      else
      {
        value = value - select_val;
      }
    }

    //Serial.println(value,6);

    if (show_state != ls_show_state || result)
    {
      //buff.clear();
      if (show_state || result)
      {
        show_koordinat(value);
      }
      else {
        switch (digit)
        {
          case 0: ronn.printText(";", 57, 8); break;
          case 1: ronn.printText(";", 51, 8); break;
          case 2: ronn.printText(";", 45, 8); break;
          case 3: ronn.printText(";", 39, 8); break;
          case 4: ronn.printText(";", 33, 8); break;
          case 5: ronn.printText(";", 27, 8); break;
          case 6: ronn.printText(";", 18, 8); break;
          case 7: ronn.printText(";", 12, 8); break;
          case 8: ronn.printText(";", 6, 8); break;
          case 9: ronn.printText(";", 0, 8); break;
        }
      }

      EEPROM_WRITE();
      ls_show_state = show_state;
    }
  }
}

void set_jadwal()
{
  uint32_t pvTime;
  uint16_t count_time = 0;
  bool exit_state = false;

  if (set_data.tampilkan_jadwal_state) {
    ronn.printText("ON;", 9, 8);
  }
  else {
    ronn.printText("OFF", 9, 8);
  }

  while (1)
  {
    uint32_t n_time = millis();

    if (digitalRead(SW_PIN) == LOW)
    {

      pvTime = n_time;
      count_time = 0;
      while (digitalRead(SW_PIN) == LOW)
      {
        count_time++;
        if (count_time > 600)
        {
          exit_state = true;
          break;
        }
        delay(1);
      }
      delay(10);
      if (exit_state)
      {
        buff.clear();
        ronn.printText("OK", 0, 0);
        set_buzzer(1, 100, 0);
        delay(100);
        buff.clear();
        break;
      }
    }


    unsigned char result = rotary.process();
    if (result == DIR_CW || result == DIR_CCW)
    {
      set_data.tampilkan_jadwal_state = !set_data.tampilkan_jadwal_state;

      if (set_data.tampilkan_jadwal_state) {
        ronn.printText("ON;", 9, 8);
      }
      else {
        ronn.printText("OFF", 9, 8);
      }
      EEPROM_WRITE();
    }
  }
}

void set_date()
{
  uint8_t cursor_select_pos = 0;
  uint8_t ls_cursor_select_pos = 1;
  uint32_t pvTime;
  bool show_state = true;
  bool ls_show_state = false;
  bool exit_state = false;
  uint16_t count_time = 0;


  DateTime now = rtc.now();
  n_tanggal = now.day();
  n_bulan = now.month();
  n_tahun = now.year();


  uint8_t set_tanggal = n_tanggal;
  uint8_t set_bulan = n_bulan;
  uint16_t set_tahun = n_tahun;

  while (1)
  {
    uint32_t n_time = millis();

    if (digitalRead(SW_PIN) == LOW)
    {

      pvTime = n_time;

      if (cursor_select_pos == 2)
      {
        cursor_select_pos = 0;
      } else
      {
        cursor_select_pos++;
      }
      count_time = 0;
      while (digitalRead(SW_PIN) == LOW)
      {
        count_time++;
        if (count_time > 600)
        {
          exit_state = true;
          break;
        }
        delay(1);
      }
      delay(10);
      if (exit_state)
      {
        buff.clear();
        ronn.printText("OK", 0, 0);
        set_buzzer(1, 100, 0);
        delay(100);
        buff.clear();
        break;
      }
    }

    unsigned char result = rotary.process();
    if (result == DIR_CW) {
      switch (cursor_select_pos)
      {
        case 0:

          if (set_tanggal == 31) {
            set_tanggal = 0;
          }
          else {
            set_tanggal++;
          }
          break;
        case 1:
          if (set_bulan == 12) {
            set_bulan = 0;
          }
          else {
            set_bulan++;
          }
          break;
        case 2:
          if (set_tahun == 2099) {
            set_tahun = 2022;
          }
          else {
            set_tahun++;
          }
          break;
      }
    }
    else if (result == DIR_CCW)
    {
      switch (cursor_select_pos)
      {
        case 0:
          if (set_tanggal == 0) {
            set_tanggal = 31;
          }
          else {
            set_tanggal--;
          }
          break;
        case 1:
          if (set_bulan == 0) {
            set_bulan = 12;
          }
          else {
            set_bulan--;
          }
          break;
        case 2:
          if (set_tahun == 2022) {
            set_tahun = 2099;
          }
          else {
            set_tahun--;
          }
          break;
      }
    }

    if (n_time - pvTime >= 400)
    {
      show_state = false;
      if (n_time - pvTime >= 800)
      {
        pvTime = n_time;
      }
    } else {
      show_state = true;
    }

    if (show_state != ls_show_state || result > 0)
    {
      if (show_state || result > 0)
      {
        sprintf(str_buffer, "%02d-%02d-%02d", set_tanggal, set_bulan, set_tahun % 100);
        ronn.printText(str_buffer, 9, 8);
      } else {
        switch (cursor_select_pos)
        {
          case 0:
            sprintf(str_buffer, ";;-%02d-%02d", set_bulan, set_tahun % 100);
            ronn.printText(str_buffer, 9, 8);
            break;
          case 1:
            sprintf(str_buffer, "%02d-;;-%02d", set_tanggal, set_tahun % 100);
            ronn.printText(str_buffer, 9, 8);
            break;
          case 2:
            sprintf(str_buffer, "%02d-%02d-;;", set_tanggal, set_bulan);
            ronn.printText(str_buffer, 9, 8);
            break;
        }
      }

      DateTime now = rtc.now();
      n_jam = now.hour();
      n_menit = now.minute();
      n_detik = now.second();

      rtc.adjust(DateTime(set_tahun, set_bulan, set_tanggal, n_jam, n_menit, n_detik));
      ls_show_state = show_state;

    }
  }
}


void set_clock()
{
  uint8_t cursor_select_pos = 0;
  uint8_t ls_cursor_select_pos = 1;
  uint32_t pvTime;
  bool show_state = true;
  bool ls_show_state = false;
  bool exit_state = false;
  uint16_t count_time = 0;



  DateTime now = rtc.now();

  n_detik = now.second();
  n_jam = now.hour();
  n_menit = now.minute();

  uint8_t set_hour = n_jam;
  uint8_t set_minute = n_menit;
  uint8_t set_second = n_detik;

  while (1)
  {
    uint32_t n_time = millis();

    if (digitalRead(SW_PIN) == LOW)
    {

      pvTime = n_time;

      if (cursor_select_pos == 2)
      {
        cursor_select_pos = 0;
      } else
      {
        cursor_select_pos++;
      }
      count_time = 0;
      while (digitalRead(SW_PIN) == LOW)
      {
        count_time++;
        if (count_time > 600)
        {
          exit_state = true;
          break;
        }
        delay(1);
      }
      delay(10);
      if (exit_state)
      {
        buff.clear();
        ronn.printText("OK", 0, 0);
        set_buzzer(1, 100, 0);
        delay(100);
        buff.clear();
        break;
      }
    }

    unsigned char result = rotary.process();
    if (result == DIR_CW) {
      switch (cursor_select_pos)
      {
        case 0:
          if (set_second == 59)
          {
            if (set_minute == 59) {
              set_minute = 0;
              if (set_hour < 23) {
                set_hour++;
              }
            }
            else {
              set_minute++;
            }
            set_second = 0;
          } else
          {
            set_second++;
          }
          break;
        case 1:
          if (set_minute == 59)
          {
            if (set_hour < 23) {
              set_hour++;
            }
            set_minute = 0;
          } else
          {
            set_minute++;
          }
          break;
        case 2:
          if (set_hour == 23) {
            set_hour = 0;
          }
          else {
            set_hour++;
          }
          break;
      }
    }
    else if (result == DIR_CCW)
    {
      switch (cursor_select_pos)
      {
        case 0:
          if (set_second == 0)
          {
            set_second = 59;
            if (set_minute > 0) {
              set_minute--;
            }
            else {
              if (set_hour > 0) {
                set_minute = 59;
                set_hour--;
              }
            }
          } else
          {
            set_second--;
          }
          break;
        case 1:
          if (set_minute == 0)
          {
            if (set_hour > 0) {
              set_hour--;
              set_minute = 59;
            }
          } else
          {
            set_minute--;
          }
          break;
        case 2:
          if (set_hour == 0) {
            set_hour = 23;
          }
          else {
            set_hour--;
          }
          break;
      }
    }

    if (n_time - pvTime >= 400)
    {
      show_state = false;
      if (n_time - pvTime >= 800)
      {
        pvTime = n_time;
      }
    } else {
      show_state = true;
    }

    if (show_state != ls_show_state || result > 0)
    {
      if (show_state || result > 0)
      {
        sprintf(str_buffer, "%02d:%02d:%02d", set_hour, set_minute, set_second);
        ronn.printText(str_buffer, 9, 8);
      } else {
        switch (cursor_select_pos)
        {
          case 0:
            sprintf(str_buffer, "%02d:%02d:;;", set_hour, set_minute);
            ronn.printText(str_buffer, 9, 8);
            break;
          case 1:
            sprintf(str_buffer, "%02d:;;:%02d", set_hour, set_second);
            ronn.printText(str_buffer, 9, 8);
            break;
          case 2:
            sprintf(str_buffer, ";;:%02d:%02d", set_minute, set_second);
            ronn.printText(str_buffer, 9, 8);
            break;
        }
      }

      rtc.adjust(DateTime(n_tahun, n_bulan, n_tanggal, set_hour, set_minute, set_second));

      ls_show_state = show_state;

    }
  }
}

void set_alarm_sholat()
{
  uint8_t cursor_select_pos = set_data.alarm_sholat_state;
  uint8_t ls_cursor_select_pos = 2;
  bool exit_state = false;
  uint16_t count_time = 0;

  while (1)
  {

    if (digitalRead(SW_PIN) == LOW)
    {
      while (digitalRead(SW_PIN) == LOW)
      {
        count_time++;
        if (count_time > 600)
        {
          exit_state = true;
          break;
        }
        delay(1);
      }
      delay(10);
      if (exit_state)
      {
        buff.clear();
        ronn.printText("OK", 0, 0);
        set_buzzer(1, 100, 0);
        delay(100);
        buff.clear();
        break;
      }
    }


    unsigned char result = rotary.process();

    if (result == DIR_CW) {
      if (cursor_select_pos == 1)
      {
        cursor_select_pos = 0;
      } else {
        cursor_select_pos++;
      }
      set_data.alarm_sholat_state = true;
    }
    else if (result == DIR_CCW)
    {
      if (cursor_select_pos == 0)
      {
        cursor_select_pos = 1;
      } else {
        cursor_select_pos--;
      }
      set_data.alarm_sholat_state = false;
    }

    if (cursor_select_pos != ls_cursor_select_pos || result > 0)
    {
      switch (cursor_select_pos)
      {
        case 0:
          set_data.alarm_sholat_state = false;
          ronn.printText("OFF", 9, 8);
          break;
        case 1:
          set_data.alarm_sholat_state = true;
          ronn.printText("ON;", 9, 8);
          break;
      }
      EEPROM_WRITE();
      ls_cursor_select_pos = cursor_select_pos;

    }
  }
}

void set_alarm_1()
{
  uint8_t cursor_select_pos = set_data.alarm1.state ? 1 : 0;
  uint32_t pvTime;
  bool show_state = true;
  bool ls_show_state;
  bool exit_state = false;
  uint16_t count_time = 0;

  while (1)
  {
    uint32_t n_time = millis();
    unsigned char result = rotary.process();

    if (digitalRead(SW_PIN) == LOW)
    {

      pvTime = n_time;
      cursor_select_pos++;
      if (cursor_select_pos > 2)
      {
        cursor_select_pos = 0;
      }
      count_time = 0;
      while (digitalRead(SW_PIN) == LOW)
      {
        count_time++;
        if (count_time > 600)
        {
          exit_state = true;
          break;
        }
        delay(1);
      }
      delay(10);
      if (exit_state)
      {
        buff.clear();
        ronn.printText("OK", 0, 0);
        set_buzzer(1, 100, 0);
        delay(100);
        buff.clear();
        break;
      }
    }

    if (result == DIR_CW && cursor_select_pos != 0)
    {
      pvTime = n_time;
      if (cursor_select_pos == 1)
      {
        if (set_data.alarm1.minute == 59)
        {
          set_data.alarm1.minute = 0;
          set_data.alarm1.hour++;
        }
        else
        {
          set_data.alarm1.minute++;
        }
      }
      else if (cursor_select_pos == 2)
      {
        if (set_data.alarm1.hour == 23)
        {
          set_data.alarm1.hour = 0;
        }
        else
        {
          set_data.alarm1.hour++;
        }
      }
    } else if (result == DIR_CCW && cursor_select_pos != 0)
    {
      pvTime = n_time;
      if (cursor_select_pos == 1)
      {
        if (set_data.alarm1.minute == 0)
        {
          set_data.alarm1.minute = 59;
          if (set_data.alarm1.hour > 0)
          {
            set_data.alarm1.hour--;
          }
        }
        else
        {
          set_data.alarm1.minute--;
        }
      } else if (cursor_select_pos == 2)
      {
        if (set_data.alarm1.hour == 0)
        {
          set_data.alarm1.hour = 23;
        }
        else
        {
          set_data.alarm1.hour--;
        }
      }

    }

    if (n_time - pvTime >= 400)
    {
      show_state = false;
      if (n_time - pvTime >= 800)
      {
        pvTime = n_time;
      }
    } else {
      show_state = true;
    }

    if (show_state != ls_show_state || result > 0)
    {
      if (show_state || result > 0)
      {
        switch (cursor_select_pos)
        {
          case 0:
            set_data.alarm1.state  = 0;
            ronn.printText("OFF;;;", 9, 8);
            break;
          case 1:
          case 2:
            set_data.alarm1.state  = 1;
            sprintf(str_buffer, "%02d:%02d", set_data.alarm1.hour, set_data.alarm1.minute);
            ronn.printText(str_buffer, 9, 8);
            break;
        }

      } else
      {
        switch (cursor_select_pos)
        {
          case 0:
            ronn.printText("OFF;;;", 9, 8);
            break;
          case 1:
            sprintf(str_buffer, "%02d:;;", set_data.alarm1.hour);
            ronn.printText(str_buffer, 9, 8);
            break;
          case 2:
            sprintf(str_buffer, ";;:%02d", set_data.alarm1.minute);
            ronn.printText(str_buffer, 9, 8);
            break;
        }
      }
      EEPROM_WRITE();
      ls_show_state = show_state;
    }
  }
}

void set_alarm_2()
{
  uint8_t cursor_select_pos = set_data.alarm2.state ? 1 : 0;
  uint32_t pvTime;
  bool show_state = true;
  bool ls_show_state;
  bool exit_state = false;
  uint16_t count_time = 0;

  while (1)
  {
    uint32_t n_time = millis();
    unsigned char result = rotary.process();

    if (digitalRead(SW_PIN) == LOW)
    {

      pvTime = n_time;
      cursor_select_pos++;
      if (cursor_select_pos > 2)
      {
        cursor_select_pos = 0;
      }
      count_time = 0;
      while (digitalRead(SW_PIN) == LOW)
      {
        count_time++;
        if (count_time > 600)
        {
          exit_state = true;
          break;
        }
        delay(1);
      }
      delay(10);
      if (exit_state)
      {
        buff.clear();
        ronn.printText("OK", 0, 0);
        set_buzzer(1, 100, 0);
        delay(100);
        buff.clear();
        break;
      }
    }

    if (result == DIR_CW && cursor_select_pos != 0)
    {
      pvTime = n_time;
      if (cursor_select_pos == 1)
      {
        if (set_data.alarm2.minute == 59)
        {
          set_data.alarm2.minute = 0;
          set_data.alarm2.hour++;
        }
        else
        {
          set_data.alarm2.minute++;
        }
      }
      else if (cursor_select_pos == 2)
      {
        if (set_data.alarm2.hour == 23)
        {
          set_data.alarm2.hour = 0;
        }
        else
        {
          set_data.alarm2.hour++;
        }
      }
    } else if (result == DIR_CCW && cursor_select_pos != 0)
    {
      pvTime = n_time;
      if (cursor_select_pos == 1)
      {
        if (set_data.alarm2.minute == 0)
        {
          set_data.alarm2.minute = 59;
          if (set_data.alarm2.hour > 0)
          {
            set_data.alarm2.hour--;
          }
        }
        else
        {
          set_data.alarm2.minute--;
        }
      } else if (cursor_select_pos == 2)
      {
        if (set_data.alarm2.hour == 0)
        {
          set_data.alarm2.hour = 23;
        }
        else
        {
          set_data.alarm2.hour--;
        }
      }

    }

    if (n_time - pvTime >= 400)
    {
      show_state = false;
      if (n_time - pvTime >= 800)
      {
        pvTime = n_time;
      }
    } else {
      show_state = true;
    }

    if (show_state != ls_show_state || result > 0)
    {
      if (show_state || result > 0)
      {
        switch (cursor_select_pos)
        {
          case 0:
            set_data.alarm2.state  = 0;
            ronn.printText("OFF;;;", 9, 8);
            break;
          case 1:
          case 2:
            set_data.alarm2.state  = 1;
            sprintf(str_buffer, "%02d:%02d", set_data.alarm2.hour, set_data.alarm2.minute);
            ronn.printText(str_buffer, 9, 8);
            break;
        }

      } else
      {
        switch (cursor_select_pos)
        {
          case 0:
            ronn.printText("OFF;;;", 9, 8);
            break;
          case 1:
            sprintf(str_buffer, "%02d:;;", set_data.alarm2.hour);
            ronn.printText(str_buffer, 9, 8);
            break;
          case 2:
            sprintf(str_buffer, ";;:%02d", set_data.alarm2.minute);
            ronn.printText(str_buffer, 9, 8);
            break;
        }
      }
      EEPROM_WRITE();
      ls_show_state = show_state;
    }
  }
}

void set_buzzer(uint8_t i, uint16_t delay_on, uint16_t delay_off)
{
  if (set_data.beep_state == 0) {
    return;
  }

  while (i != 0)
  {
    digitalWrite(BUZZER_PIN, 1);
    delay(delay_on);
    digitalWrite(BUZZER_PIN, 0);
    delay(delay_off);
    i--;
  }
}

void calc_time()
{
  /*
    CALCULATION METHOD
    ------------------
    Jafari,   // Ithna Ashari
    Karachi,  // University of Islamic Sciences, Karachi
    ISNA,     // Islamic Society of North America (ISNA)
    MWL,      // Muslim World League (MWL)
    Makkah,   // Umm al-Qura, Makkah
    Egypt,    // Egyptian General Authority of Survey
    Custom,   // Custom Setting
    JURISTIC
    --------
    Shafii,    // Shafii (standard)
    Hanafi,    // Hanafi
    ADJUSTING METHOD
    ----------------
    None,        // No adjustment
    MidNight,   // middle of night
    OneSeventh, // 1/7th of night
    AngleBased, // angle/60th of night
    TIME IDS
    --------
    Fajr,
    Sunrise,
    Dhuhr,
    Asr,
    Sunset,
    Maghrib,
    Isha

  */
  //----------------------------------------------------
  int zonawaktu = 7;

  set_calc_method(ISNA);
  set_asr_method(Shafii);
  set_high_lats_adjust_method(AngleBased);
  set_fajr_angle(20);
  set_isha_angle(18);

  get_prayer_times(n_tahun, n_bulan, n_tanggal, set_data.latitude_val, set_data.longitude_val, zonawaktu, times);
  //-----------------------------------------------------
}
void display_jadwal(uint8_t &show_pos)
{
  static bool dot_state = 0;
  static bool clear_state = 0;
  static uint8_t x = 40;
  static uint32_t pvTime = 0;
  static uint32_t pvScroll = 0;
  static uint8_t show_step = 0;
  static uint8_t ls_show_step = 1;

  uint32_t n_time = millis();

  if (n_time - pvScroll >= 5 && x >= 1 && clear_state == true)
  {
    pvScroll = n_time;
    buff.scrollLeft(0, 0, 40, 16);
    x--;
    if (x == 0)
    {
      x = 40;
      clear_state = false;
    }
  }

  if (n_detik != ls_detik)
  {
    dot_state = !dot_state;
    ronn.setFont(font_DEFAULT);


    sprintf(str_buffer, "%02d", n_jam);
    ronn.printText(str_buffer, 50, 0);

    sprintf(str_buffer, "%02d ", n_menit);
    ronn.printText(str_buffer, 50, 8);

    if (dot_state)
    {
      ronn.setFont(font_DEFAULT);
      ronn.printText(".", 47, -3);
      ronn.printText(".", 47, 6);

    } else {
      buff.fillRect(47, 0, 2, 16, 0);
    }
    ls_detik = n_detik;
  }

  if (n_time - pvTime >= 1800)
  {
    clear_state = true;
    if (n_time - pvTime >= 2600)
    {
      clear_state = false;
      if (show_step == 5)
      {
        show_step = 0;
      } else {
        show_step++;
      }

      pvTime = n_time;
    }
  }

  if (clear_state == 0 && show_step != ls_show_step)
  {
    pvScroll = n_time;
    ronn.setFont(font_DEFAULT);
    switch (show_step)
    {
      case 0:
        ronn.printText_R("SUBUH", 0, 0);
        get_waktu_sholat(0);
        waktu_sholat[0][0] = hours;
        waktu_sholat[0][1] = minutes;
        break;
      case 1:
        ronn.printText_R("DZUHUR", 0, 0);
        get_waktu_sholat(2);
        waktu_sholat[1][0] = hours;
        waktu_sholat[1][1] = minutes;
        break;
      case 2:
        ronn.printText_R("ASHAR", 0, 0);
        get_waktu_sholat(3);
        waktu_sholat[2][0] = hours;
        waktu_sholat[2][1] = minutes;
        break;
      case 3:
        ronn.printText_R("MAGHRIB", 0, 0);
        get_waktu_sholat(5);
        waktu_sholat[3][0] = hours;
        waktu_sholat[3][1] = minutes;
        break;
      case 4:
        ronn.printText_R("ISYA'", 0, 0);
        get_waktu_sholat(6);
        waktu_sholat[4][0] = hours;
        waktu_sholat[4][1] = minutes;
        break;
      case 5: show_pos = 0; first_run_clock = true; return;
    }
    ls_show_step = show_step;
  }
}

void get_waktu_sholat(uint8_t i)
{
  calc_time();

  get_float_time_parts(times[i], hours, minutes);

  minutes = minutes + 10;
  if (minutes < 11) {
    minutes = 60 - minutes;
    hours --;
  } else {
    minutes = minutes - 10 ;
  }

  sprintf(str_buffer, "%02d:%02d", hours, minutes);
  ronn.printText(str_buffer, 0, 8);
}

void display_s_clock(bool &first_run_flag)
{

  ronn.setFont(font_DEFAULT);

  static uint8_t ls_tanggal;
  static bool show_option;

  static uint8_t ls_suhu;

  if (n_detik != ls_detik || first_run_flag)
  {

    uint8_t num_1 = n_detik / 10;
    uint8_t num_2 = n_detik % 10;

    if (num_2 == 0 || first_run_flag)
    {
      ronn.scrollText_D(String(num_2), 49, 0, 5);
      ronn.scrollText_D(String(num_1), 43, 0, 5);

      if (num_1 == 0 && num_2 == 0 || first_run_flag)
      {
        num_1 = n_menit / 10;
        num_2 = n_menit % 10;

        if (num_2 == 0 || first_run_flag)
        {
          ronn.scrollText_D(String(num_2), 32, 0, 5);
          ronn.scrollText_D(String(num_1), 26, 0, 5);

          if (num_1 == 0 && num_2 == 0 || first_run_flag)
          {
            num_1 = n_jam / 10;
            num_2 = n_jam % 10;

            if (num_2 == 0 || first_run_flag)
            {
              ronn.scrollText_D(String(num_2), 15, 0, 5);
              ronn.scrollText_D(String(num_1), 9, 0, 5);
            } else {
              ronn.scrollText_D(String(num_2), 15, 0, 5);
            }
          }
        } else {
          ronn.scrollText_D(String(num_2), 32, 0, 5);
        }
      }
    } else {
      ronn.scrollText_D(String(num_2), 49, 0, 5);
    }



    if ((ls_detik != n_detik && n_detik % 5 == 0) || first_run_flag)
    {
      //buff.fillRect(0, 8, 64, 8, 0);
      ronn.clear_D(0, 8, 64, 8);
      show_option = !show_option;

      if (show_option == 0) {

        sprintf(str_buffer, "SUHU:%d%cC", n_suhu, (char)144);
        ronn.scrollText_D(str_buffer, 8, 8);
      }
      else {
        sprintf(str_buffer, "%02d-%02d-%04d", n_tanggal, n_bulan, n_tahun);
        ronn.scrollText_D(str_buffer, 3, 8);
      }
    }

    if (show_option == 0 )
    {
      if (ls_suhu != n_suhu)
      {
        sprintf(str_buffer, "SUHU:%d%cC", n_suhu, (char)144);
        ronn.printText(str_buffer, 8, 8);
        ls_suhu = n_suhu;
      }
    }
    else if (show_option == 1 )
    {
      if ((n_tanggal != ls_tanggal))
      {
        ls_tanggal = n_tanggal;
        sprintf(str_buffer, "%02d-%02d-%04d", n_tanggal, n_bulan, n_tahun);
        ronn.printText(str_buffer, 3, 8);
      }
    }
    ls_detik = n_detik;
  }

  if (first_run_flag == true) {
    ronn.printText(":", 22, -1);
    ronn.printText(":", 39, -1);
    first_run_flag = false;
  }
}
