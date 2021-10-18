#include "Wire.h"  //for LCD (may need to switch to TinyWireS for Attiny)
#include "Adafruit_LiquidCrystal.h"
#include "RTClib.h"

int cycle    = 2;      //button to cycle through menu options
int ok       = 3;      //button to enter selected menu option
int l_relay  = 4;      //relay for lights
int f_relay  = 5;      //relay for fans

Adafruit_LiquidCrystal lcd(3); //LCD object
RTC_DS3231 rtc;                //RTC object
DateTime now;                  //current time

enum menus {WAIT, TEMP, TIME, LIGHTS, FANS, DURATIONS};  //enum for state machine
int current_state = WAIT;  //current place in state machine
int loop_counter = 0;      //timing variable

bool cycle_val, ok_val = false;             //button values read in
float temperature = 0.0;                    //temperature read in
int time_h, time_m, time_s, last_sec = 0;   //current time seperated by hours, mins and secs (with timing variable)
String light_mode = "AUTO";                 //current mode for light (AUTO, ON, OFF)
String fan_mode   = "OFF";                  //current mode for fan (AUTO, ON, OFF)
int lights_on = 7, lights_off = 19, fans_on = 5, fans_off = 3;  //scheduling values for (de)activating lights/fans
bool lights_are_on, fans_are_on = false;    //current state of lights/fans
DateTime fan_change_time;                   //how long to wait before (de)activating fans (start off)
bool fans_time_set = false;                 //used when calculating wait time

//function prototypes
void read_buttons();
void wait_for_button();
void update_time();
void check_lights();
void check_fans();

//menu function prototypes
void edit_time();
void edit_light_mode();
void edit_fan_mode();
void edit_durations();

void setup()
{
  Serial.begin(9600);
  
  //setup a 16x2 LCD display
  lcd.begin(16, 2);
  lcd.setBacklight(LOW);

  //setup the RTC with builtin temperature sensor
  if(!rtc.begin()) { Serial.println("Couldn't find RTC"); Serial.flush(); abort(); }
  if (rtc.lostPower()) { rtc.adjust(DateTime(F(__DATE__), F(__TIME__))); }

  //setup buttons and relays
  pinMode(cycle, INPUT);
  pinMode(ok, INPUT);
  pinMode(l_relay, OUTPUT);
  pinMode(f_relay, OUTPUT);
  digitalWrite(l_relay, LOW);
  digitalWrite(f_relay, LOW);
}

void loop()
{
  if(current_state == WAIT)
  {
    if(cycle_val || ok_val)
    {
      lcd.setBacklight(HIGH);
      lcd.display();
      ++current_state;
    }
  }

  else if(current_state == TEMP)
  {
    if(loop_counter % 10 == 0 || loop_counter == 2)  //once a second
    {
      temperature = rtc.getTemperature();        //in Celsius
      temperature = (temperature * 9 / 5) + 32;  //in Fahrenheit

      lcd.setCursor(2, 0);  // set the cursor to column 2, line 0 (first row)
      lcd.print("Temperature:");
      lcd.setCursor(4, 1);  // set the cursor to column 4, line 1 (second row)
      lcd.print(String(temperature) + " F");
    }
    
    if(cycle_val) { ++current_state; lcd.clear(); }
    else if(ok_val) { ; }

  }

  else if(current_state == TIME)
  {
    if(last_sec != time_s)
    {
      last_sec = time_s;
      
      lcd.clear();
      lcd.setCursor(5, 0);
      lcd.print("Time:");
      lcd.setCursor(2, 1);

      if(time_h < 10) { lcd.print("0"); }
      lcd.print(String(time_h) + " : ");
      if(time_m < 10) { lcd.print("0"); }
      lcd.print(String(time_m) + " : ");
      if(time_s < 10) { lcd.print("0"); }
      lcd.print(String(time_s));
    }
    
    if(cycle_val) { ++current_state; lcd.clear(); }
    else if(ok_val) { edit_time(); lcd.clear(); }
  }

  else if(current_state == LIGHTS)
  {
    if(loop_counter == 2)
    {
      lcd.setCursor(0,0);
      lcd.print("LIGHTS:");
      lcd.setCursor(0,1);
      lcd.print(light_mode);
    }
    
    if(cycle_val) { ++current_state; lcd.clear(); }
    else if(ok_val) { edit_light_mode(); lcd.clear(); }
  }

  else if(current_state == FANS)
  {
    if(loop_counter == 2)
    {
      lcd.setCursor(0,0);
      lcd.print("FANS:");
      lcd.setCursor(0,1);
      lcd.print(fan_mode);
    }
    
    if(cycle_val) { ++current_state; lcd.clear(); }
    else if(ok_val) { edit_fan_mode(); lcd.clear(); }
  }

  else if(current_state == DURATIONS)
  {
    if(loop_counter == 2)
    {
      lcd.setCursor(0,0);
      lcd.print("LIGHTS: " + String(lights_on) + " - " + String(lights_off));  //on time + off time (hours only)
      lcd.setCursor(0,1);
      lcd.print("FANS: " + String(fans_on) + "m / " + String(fans_off) + "h"); //minutes on + hours off
    }
    if(cycle_val) { current_state = TEMP; lcd.clear(); }
    else if(ok_val) { edit_durations(); lcd.clear(); }
  }

  update_time();
  check_lights();
  check_fans();

  read_buttons();

  delay(100);

  ++loop_counter;

  if(loop_counter > 300)  //turn off after 30 second of inactivity
  {
    if(current_state != WAIT)
    {
      lcd.setBacklight(LOW);
      lcd.clear();
      lcd.noDisplay();
      current_state = WAIT;
    }
    loop_counter = 0;
  }
  
}


void read_buttons()
{
  bool cycle_button = bool(digitalRead(cycle));
  bool ok_button    = bool(digitalRead(ok));

  if(cycle_button || ok_button)
  {
    cycle_val = cycle_button;
    ok_val    = ok_button;

    loop_counter = 0;

    while(cycle_button || ok_button)
    {
      delay(10);
      cycle_button = bool(digitalRead(cycle));
      ok_button    = bool(digitalRead(ok));
    }
    delay(200);
  }
  else
  {
    cycle_val = false;
    ok_val    = false;
  }
}

void wait_for_button()
{
  cycle_val = false;
  ok_val    = false;
  while(!cycle_val && !ok_val)
  {
    delay(10);
    cycle_val = bool(digitalRead(cycle));
    ok_val    = bool(digitalRead(ok));
  }
  while(bool(digitalRead(cycle)) || bool(digitalRead(ok)))
  { delay(10); }
  delay(100);
}

void update_time()
{
  now = rtc.now();
  
  time_h = int(now.hour());
  time_m = int(now.minute());
  time_s = int(now.second());
}

void check_lights()
{
  if(light_mode == "ON") { if(!lights_are_on) { digitalWrite(l_relay, HIGH); lights_are_on = true; } }
  if(light_mode == "OFF") { if(lights_are_on) { digitalWrite(l_relay, LOW); lights_are_on = false; } }
  if(light_mode == "AUTO")
  {
    if(time_h >= lights_on && time_h <= lights_off)
    {
      if(!lights_are_on) { digitalWrite(l_relay, HIGH); lights_are_on = true; }
    }
    else
    {
      if(lights_are_on) { digitalWrite(l_relay, LOW); lights_are_on = false; }
    }
  }
}

void check_fans()
{
  if(fan_mode == "ON") { if(!fans_are_on) { digitalWrite(f_relay, HIGH); fans_are_on = true; } }
  if(fan_mode == "OFF") { if(fans_are_on) { digitalWrite(f_relay, LOW); fans_are_on = false; } }
  if(fan_mode == "AUTO")
  {
    if(fans_time_set)
    {
      if(now >= fan_change_time) { fans_time_set = false; }
    }
    else
    {
      if(fans_are_on)
      {
        digitalWrite(f_relay, LOW);
        fans_are_on = false;
        fan_change_time = rtc.now() + TimeSpan(0, fans_off, 0, 0);
      }
      else
      {
        digitalWrite(f_relay, HIGH);
        fans_are_on = true;
        fan_change_time = rtc.now() + TimeSpan(0, 0, fans_on, 0);
      }
      fans_time_set = true;
    }
  }
}


void edit_time()
{
  /*
  these hold the max value of the digit place
  (ex. if h0 below is 2, then the only options are 20, 21, 22 and 23, so 3 is the max)
  int h0_max = 2;  //tens place for hours
  int h1_max = 9;  //ones place for hours
  int h2_max = 3;  //ones place for hours if tens place is 2
  int m0_max = 5;  //tens place for minutes
  int m1_max = 9;  //ones place for minutes
  */
  //This array replaces the above to compress the code into the for loop below
  byte max_arr[5] = {2, 9, 3, 5, 9};

  /*
  these hold the individual digits, which will be mashed into time_h and time_m at the end
  int h0 = time_h / 10;  //tens place for hours
  int h1 = time_h % 10;  //ones place for hours
  int m0 = time_m / 10;  //tens place for minutes
  int m1 = time_m % 10;  //ones place for minutes
  */
  //This array replaces the above to compress the code into the for loop below (use the right h1/h2)
  byte place_arr[5] = {(time_h / 10), (time_h % 10), (time_h % 10), (time_m / 10), (time_m % 10)};

  byte cursor_arr[5] = {4, 5, 5, 9, 10};
  
  lcd.clear();
  lcd.setCursor(3, 0);
  lcd.print("Edit Time:");
  lcd.setCursor(4, 1);

  if(time_h < 10) { lcd.print("0"); }
  lcd.print(String(time_h) + " : ");
  if(time_m < 10) { lcd.print("0"); }
  lcd.print(String(time_m) + " : ");

  lcd.blink();

  for(int i = 0; i < 5; ++i)
  {
    cycle_val = false;
    ok_val    = false;
    
    if(i == 1 && place_arr[0] == 2)
    {
      ++i;
      if(place_arr[i] > max_arr[i])
      {
        place_arr[i] = 0;
        lcd.setCursor(cursor_arr[i], 1);
        lcd.print(String(place_arr[i]));
        lcd.setCursor(cursor_arr[i], 1);
      }
    }
    if(i == 2 && place_arr[0] != 2) { ++i; }

    lcd.setCursor(cursor_arr[i], 1);
    
    while(!ok_val)
    {
      if(cycle_val)
      {
        ++place_arr[i];
        if(place_arr[i] > max_arr[i]) { place_arr[i] = 0; }
        lcd.print(String(place_arr[i]));
        lcd.setCursor(cursor_arr[i], 1);
      }
      wait_for_button();
    }
  }

  lcd.noBlink();

  if(place_arr[0] == 2)
  { time_h = (place_arr[0] * 10) + place_arr[2]; }
  else
  { time_h = (place_arr[0] * 10) + place_arr[1]; }

  time_m = (place_arr[3] * 10) + place_arr[4];

  rtc.adjust(DateTime(2020, 11, 12, time_h, time_m, 0));
  delay(200);
}

void edit_light_mode()
{
  String modes[3] = {"AUTO", "ON", "OFF"};
  
  lcd.blink();

  cycle_val = false;
  ok_val    = false;
  int i = 0;

  while(!ok_val)
  {
    if(cycle_val)
    {
      ++i;
      if(i > 2) { i = 0; }
      light_mode = modes[i];
    }
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Edit Light Mode:");
    lcd.setCursor(0, 1);
    lcd.print(light_mode);
    lcd.setCursor(0, 1);
    
    wait_for_button();
  }

  lcd.noBlink();
}

void edit_fan_mode()
{
  String modes[3] = {"AUTO", "ON", "OFF"};
  
  lcd.blink();

  cycle_val = false;
  ok_val    = false;
  int i = 0;

  while(!ok_val)
  {
    if(cycle_val)
    {
      ++i;
      if(i > 2) { i = 0; }
      fan_mode = modes[i];
    }
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Edit Fan Mode:");
      lcd.setCursor(0, 1);
      lcd.print(fan_mode);
      lcd.setCursor(0, 1);
  
    wait_for_button();
  }

  lcd.noBlink();
  fans_time_set = false;
}

void edit_durations()
{
  int fan_minutes_on[6] = {1, 2, 3, 5, 10, 15};
  //int fan_max_hours_off = 8;
  int vals[4] = {lights_on, lights_off, fans_on, fans_off};
  
  lcd.blink();

  int i = 0;
  int off_min = 0;             //used in the light menu to ensure time-off > time-on time
  int compare1, compare2 = 0;  //used in the fan menu between hours and minutes

  for(int j = 0; j < 2; ++j)
  {
    cycle_val = false;
    ok_val    = false;
  
    i = vals[j];
    while(!ok_val)
    {
      if(cycle_val)
      {
        ++i;
        if(i > 23) { i = off_min; }
        vals[j] = i;
      }
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Lights on: ");
      lcd.setCursor(14, 0);
      if(vals[0] < 10) { lcd.print("0"); }
      lcd.print(String(vals[0]));
      lcd.setCursor(0, 1);
      lcd.print("Lights off: ");
      lcd.setCursor(14, 1);
      if(vals[1] < 10) { lcd.print("0"); }
      lcd.print(String(vals[1]));

      if(j == 0) { lcd.setCursor(14, 0); }
      else { lcd.setCursor(14, 1); }
  
      wait_for_button();
    }
    off_min = vals[0] + 1;
    if(vals[1] < off_min) {vals[1] = off_min; }
  }

  for(int j = 2; j < 4; ++j)
  {
    cycle_val = false;
    ok_val    = false;
    
    i = vals[j];
    if(j == 2) { compare1 = 5; compare2 = 0; }
    else { compare1 = 8; compare2 = 1; }
    
    while(!ok_val)
    {
      if(cycle_val)
      {
        ++i;
        if(i > compare1) { i = compare2; }
        if(j == 2) { vals[j] = fan_minutes_on[i]; }
        else { vals[j] = i; }
      }
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Fans on: ");
      lcd.setCursor(14, 0);
      if(vals[2] < 10) { lcd.print("0"); }
      lcd.print(String(vals[2]));
      lcd.setCursor(0, 1);
      lcd.print("Fans off: ");
      lcd.setCursor(14, 1);
      if(vals[3] < 10) { lcd.print("0"); }
      lcd.print(String(vals[3]));
      
      if(j == 2) { lcd.setCursor(14, 0); }
      else { lcd.setCursor(14, 1); }
  
      wait_for_button();
    }
  }

  lights_on  = vals[0];
  lights_off = vals[1];
  fans_on    = vals[2];
  fans_off   = vals[3];

  lcd.noBlink();
  fans_time_set = false;
}
