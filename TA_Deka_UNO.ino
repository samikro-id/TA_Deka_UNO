#include <ArduinoJson.h>
#include <Wire.h>
#include <DS3231.h>         //  DS3231 Andrew Wickert V1.0.2

#define SOFTWARE_VERSI  1
#define EEPROM_ADDRESS  0x57

#define TEGANGAN_PIN    A0
#define ARUS_PIN        A1

#define RELAY1_PIN      8
#define RELAY2_PIN      9
bool relay_state        = false;

uint32_t led_time;
bool led_state          = false;

#define VOLT_FULL       13    // V
#define VOLT_EMPTY      10    // V      
float volt;

#define ARUS_SENSITIVITY  185   // mV  tegantung sensor arus yang digunakan, yang ini 5A
#define ARUS_OFFSET       2.5   // V
double arus;

#define ENERGY_FULL     48
#define ENERGY_EMPTY    0
float energy = 0;

uint8_t persen;
uint32_t sensor_time;

DS3231 clock;
struct time{
  uint8_t hour;
  uint8_t minute;
  uint8_t second;
};
struct time time_now;
uint32_t time_time;
bool h12;
bool PM;

#define SERIAL_BAUD     115200
#define SERIAL_LEN      1000
String serial_buff;
bool serial_complete    = false;

struct schedule{
  uint8_t start_hour;
  uint8_t start_minute;
  uint8_t finish_hour;
  uint8_t finish_minute;
};

struct get_schedule{
  bool success;
  uint8_t start_hour;
  uint8_t start_minute;
  uint8_t finish_hour;
  uint8_t finish_minute;
};

void setup(){
  delay(100);

  Serial.begin(SERIAL_BAUD);        
  Wire.begin();      
  initIo();

  // if(!checkSoftwareVersi()){
  //   setSoftwareVersi();
  //   resetScheduleAll();
  // }
  
  serial_buff.reserve(SERIAL_LEN);
  
  sensor_time = millis();
  led_time = millis();
  time_time = millis();
}

void loop(){
  if((millis() - sensor_time) > 200){
    bacaSensor();
    sensor_time = millis();
  }

  if((millis() - led_time) > 200){
    toggleLed();
    led_time = millis();
  }

  if((millis() - time_time) > 200){
    time_now = getClock();
  }

  if(time_now.second == 0){
    checkSchedule();
  }

  if(serial_complete){
    serial_complete = false;
    
    prosesData();
    
    serial_buff = "";
  }
}

void bacaSensor(){
  /* BACA TEGANGAN */
  uint16_t voltRaw = analogRead(TEGANGAN_PIN);      
  float pinvolt = (float) (5 * voltRaw) / 1024;   
  volt = (float) pinvolt * 5;

  /* HITUNG PERSEN */
  if(volt > VOLT_EMPTY && volt < VOLT_FULL ){
    persen = ((volt - VOLT_EMPTY) / (VOLT_FULL - VOLT_EMPTY)) * 100;
  }
  else{
    persen = 0;
  }

  /* BACA arus */
  uint16_t arusRaw = analogRead(ARUS_PIN);
  double pinArus = 5000 * (arusRaw / 1024.0);
  arus = (float) ((pinArus - ARUS_OFFSET) / ARUS_SENSITIVITY);

  /* HITUNG ENERGY */
  energy += float((volt * arus) / 3600);         // 3600 detik dalam 1 jam
  if(energy > ENERGY_FULL){    energy = ENERGY_FULL;  }
  if(energy <= ENERGY_EMPTY){  energy = ENERGY_EMPTY; }

}

void prosesData(){
  StaticJsonBuffer<SERIAL_LEN> JSONBuffer;

  JsonObject& root = JSONBuffer.parseObject(serial_buff);
  if(root.success()){
    const char * op = root["op"];
    
    if(strcmp(op, "data") == 0){
      serial_buff = "{\"tegangan\":" + String(volt, 1) +",\"arus\":"+ String(arus, 2) + ",\"energy\":" + String(energy, 2) + ",\"on\":" + String(relay_state, DEC) + "}";
      Serial.print(serial_buff);
    }
    else if(strcmp(op, "on") == 0){
      relayState(root["state"]);

      serial_buff = "{\"tegangan\":" + String(volt, 1) +",\"arus\":"+ String(arus, 2) + ",\"energy\":" + String(energy, 2) + ",\"on\":" + String(relay_state, DEC) + "}";
      Serial.print(serial_buff);
    }
    else if(strcmp(op, "wh") == 0){
      const char * set = root["set"];
      if(strcmp(set, "full") == 0){
        energy = ENERGY_FULL;
      }
      else if(strcmp(set, "empty") == 0){
        energy = ENERGY_EMPTY;
      }

      serial_buff = "{\"tegangan\":" + String(volt, 1) +",\"arus\":"+ String(arus, 2) + ",\"energy\":" + String(energy, 2) + ",\"on\":" + String(relay_state, DEC) + "}";
      Serial.print(serial_buff);
    }
    else if(strcmp(op, "clock") == 0){
      bool set = root["set"];
      if(set){
        struct time set_time;

        set_time.hour = root["hour"];
        set_time.minute = root["minute"];
        set_time.second = 0;

        setClock(set_time);
        time_now = getClock();

        serial_buff = "";
        serial_buff = "{\"op\":\"clock\",\"time\":\"" + String(time_now.hour, DEC) + ":" + String(time_now.minute, DEC) + "\"}";

        Serial.print(serial_buff);
      }else{
        serial_buff = "";
        serial_buff = "{\"op\":\"clock\",\"time\":\"" + String(time_now.hour, DEC) + ":" + String(time_now.minute, DEC) + "\"}";

        Serial.print(serial_buff);
      }
    }
    else if(strcmp(op,"schedule") == 0){
      bool set = root["set"];
      if(set){
        struct schedule set_schedule;
        uint8_t id;

        set_schedule.start_hour = root["start_hour"];
        set_schedule.start_minute = root["start_minute"];
        set_schedule.finish_hour = root["finish_hour"];
        set_schedule.finish_minute = root["finish_minute"];

        id = setSchedule(set_schedule);

        serial_buff = "";
        serial_buff = "{\"op\":\"schedule\",\"id\":" + String(id, DEC) + "}"; 
        
        Serial.print(serial_buff);
      }else{
        uint8_t last_id;

        serial_buff = "";
        serial_buff = "[";

        /*** get first schedule ****/
        for(uint8_t i=1; i<33; i++){
          struct get_schedule first_schedule;

          last_id = i;

          first_schedule = getSchedule(i);
          if(first_schedule.success){
            serial_buff += "{\"id\":" + String(i, DEC) 
                          + ",\"start\":\"" + String(first_schedule.start_hour, DEC) + ":" + String(first_schedule.start_minute, DEC) 
                          + "\",\"finish\":\"" + String(first_schedule.finish_hour, DEC) + ":" + String(first_schedule.finish_minute, DEC)
                          + "\"}";
            
            break;
          }
        }

        for(uint8_t i=last_id+1; i<33; i++){
          struct get_schedule first_schedule;

          first_schedule = getSchedule(i);
          if(first_schedule.success){
            serial_buff += ",{\"id\":" + String(i, DEC) 
                          + ",\"start\":\"" + String(first_schedule.start_hour, DEC) + ":" + String(first_schedule.start_minute, DEC) 
                          + "\",\"finish\":\"" + String(first_schedule.finish_hour, DEC) + ":" + String(first_schedule.finish_minute, DEC)
                          + "\"}";
          }
        }

        serial_buff += "]";

        Serial.print(serial_buff);
        serial_buff = "";
      }
    }
  }
}


/******* Clock section ******/
void setClock(struct time set){
  clock.setClockMode(false);    // set to 24h

  clock.setHour(set.hour);
  clock.setMinute(set.minute);
  clock.setSecond(set.second);
}

struct time getClock(){
  struct time get_time;

  get_time.hour = clock.getHour(h12, PM);
  get_time.minute = clock.getMinute();
  get_time.second = clock.getSecond();

  return get_time;
}


/******* EEPROM 24C32 section ******/
bool checkSoftwareVersi(){
  byte b = i2c_eeprom_read_byte(EEPROM_ADDRESS, 0);       // software version in register 0
  
  return (b == SOFTWARE_VERSI)? true:false;
}

void setSoftwareVersi(){
  i2c_eeprom_write_byte(EEPROM_ADDRESS, 0, SOFTWARE_VERSI);  // software version in register 0 length 1
  delay(100); //add a small delay
}

void i2c_eeprom_write_byte( int deviceaddress, unsigned int eeaddress, byte data ) {
  int rdata = data;
  Wire.beginTransmission(deviceaddress);
  Wire.write((int)(eeaddress >> 8)); // MSB
  Wire.write((int)(eeaddress & 0xFF)); // LSB
  Wire.write(rdata);
  Wire.endTransmission();

  delay(10);
}

byte i2c_eeprom_read_byte( int deviceaddress, unsigned int eeaddress ) {
  byte rdata = 0xFF;
  Wire.beginTransmission(deviceaddress);
  Wire.write((int)(eeaddress >> 8)); // MSB
  Wire.write((int)(eeaddress & 0xFF)); // LSB
  Wire.endTransmission();
  Wire.requestFrom(deviceaddress,1);
  if (Wire.available()) rdata = Wire.read();
  return rdata;
}


/******** Schedule section ******/
void checkSchedule(){
  struct get_schedule schedule_now;

  for(uint8_t i=0; i<32; i++){
    schedule_now = getSchedule(i+1);
    if(schedule_now.success){
      if(schedule_now.start_hour == time_now.hour && schedule_now.start_minute == time_now.minute){
        relayState(true);
      }

      if(schedule_now.finish_hour == time_now.hour && schedule_now.finish_minute == time_now.minute){
        relayState(false);
      }
    }
  }
}

void resetScheduleAll(){
  i2c_eeprom_write_byte(EEPROM_ADDRESS, 1, 0);  // reset schedule flag from register 1 to 4 (4bytes = 32bit)
  i2c_eeprom_write_byte(EEPROM_ADDRESS, 2, 0);  // reset schedule flag from register 1 to 4 (4bytes = 32bit)
  i2c_eeprom_write_byte(EEPROM_ADDRESS, 3, 0);  // reset schedule flag from register 1 to 4 (4bytes = 32bit)
  i2c_eeprom_write_byte(EEPROM_ADDRESS, 4, 0);  // reset schedule flag from register 1 to 4 (4bytes = 32bit)
}

struct get_schedule getSchedule(uint8_t id){
  struct get_schedule get_instance;
  uint32_t flag_check = 0x00000001;
  uint32_t flag;

  get_instance.success = false;

  for(uint8_t i=0; i<(id-1); i++){
    flag_check<<=1;
  }

  flag = getScheduleFlag();
  if((flag & flag_check)){
    get_instance.start_hour = i2c_eeprom_read_byte(EEPROM_ADDRESS, 5 + (4 * (id-1)));
    get_instance.start_minute = i2c_eeprom_read_byte(EEPROM_ADDRESS, 6 + (4 * (id-1)));

    get_instance.finish_hour = i2c_eeprom_read_byte(EEPROM_ADDRESS, 7 + (4 * (id-1)));
    get_instance.finish_minute = i2c_eeprom_read_byte(EEPROM_ADDRESS, 8 + (4 * (id-1)));

    get_instance.success = true;
  }

  return get_instance;
}

uint8_t setSchedule(struct schedule set_schedule){
  uint32_t schedule_id = 0;
  uint32_t flag_begin = 0x00000001;
  uint32_t flag;
  
  flag = getScheduleFlag();
  for(uint8_t i=0; i<32; i++){
    if((flag & flag_begin) == 0){
      
      /*** set new flag ***/
      flag |= flag_begin;
      setScheduleFlag(flag);

      i2c_eeprom_write_byte(EEPROM_ADDRESS, 5 + (4 * i), set_schedule.start_hour);
      i2c_eeprom_write_byte(EEPROM_ADDRESS, 6 + (4 * i), set_schedule.start_minute);

      i2c_eeprom_write_byte(EEPROM_ADDRESS, 7 + (4 * i), set_schedule.finish_hour);
      i2c_eeprom_write_byte(EEPROM_ADDRESS, 8 + (4 * i), set_schedule.finish_minute);

      schedule_id = i + 1;
    }

    flag_begin <<=1;
  }

  return schedule_id;
}

void setScheduleFlag(uint32_t flag){
  byte b4 = (flag & 0xFF);  flag >>= 8;    // LSB
  byte b3 = (flag & 0xFF);  flag >>= 8;
  byte b2 = (flag & 0xFF);  flag >>= 8;
  byte b1 = (flag & 0xFF);                // MSB

  i2c_eeprom_write_byte(EEPROM_ADDRESS, 1, b1); // MSB
  i2c_eeprom_write_byte(EEPROM_ADDRESS, 2, b2);
  i2c_eeprom_write_byte(EEPROM_ADDRESS, 3, b3);
  i2c_eeprom_write_byte(EEPROM_ADDRESS, 4, b4); // LSB
}

uint32_t getScheduleFlag(){
  uint32_t flag = 0;

  byte b1 = i2c_eeprom_read_byte(EEPROM_ADDRESS, 1);       // schedule flag resigter 1 (MSB)
  byte b2 = i2c_eeprom_read_byte(EEPROM_ADDRESS, 2);       // schedule flag resigter 2
  byte b3 = i2c_eeprom_read_byte(EEPROM_ADDRESS, 3);       // schedule flag resigter 2
  byte b4 = i2c_eeprom_read_byte(EEPROM_ADDRESS, 4);       // schedule flag resigter 4 (LSB)

  flag += b1;  flag <<= 8;
  flag += b2;  flag <<= 8;
  flag += b3;  flag <<= 8;
  flag += b4;

  return flag;
}


/******* Serial Interrupt Event Callback ********/
void serialEvent(){
  while(Serial.available()){
    char inChar = (char) Serial.read();
    if(inChar == '\n'){
      serial_complete = true;
    }else if(inChar == '\r'){
      // do nothing
    }else{
      serial_buff += inChar;
    }
  }
}


/****** Relay function*******/
void relayState(bool state){
  if(state){  digitalWrite(RELAY1_PIN, HIGH);  }
  else{       digitalWrite(RELAY1_PIN, LOW);   }
  relay_state = state;
}


/****** LED function ******/
void ledState(bool state){
  if(state){  digitalWrite(LED_BUILTIN, HIGH);  }
  else{       digitalWrite(LED_BUILTIN, LOW);   }
  led_state = state;
}

void toggleLed(){
  if(led_state){
    digitalWrite(LED_BUILTIN, LOW);
    led_state = false;
  }else{
    digitalWrite(LED_BUILTIN, HIGH);
    led_state = true;
  }
}


/****** Initialize *******/
void initIo(){
  pinMode(RELAY1_PIN, OUTPUT);
  digitalWrite(RELAY1_PIN, HIGH);

  pinMode(RELAY2_PIN, OUTPUT);
  digitalWrite(RELAY2_PIN, HIGH);

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);
  led_state = false;
}

/* data schedule example
[
  {
    "id":1,
    "start":"255:255",
    "finish":"255:255"
  },
  {
    "id":2,
    "start":"255:255",
    "finish":"255:255"
  },
  {
    "id":3,
    "start":"255:255",
    "finish":"255:255"
  },
  {
    "id":4,
    "start":"255:255",
    "finish":"255:255"
  },
  {
    "id":5,
    "start":"255:255",
    "finish":"255:255"
  },
  {
    "id":6,
    "start":"255:255",
    "finish":"255:255"
  },
  {
    "id":7,
    "start":"255:255",
    "finish":"255:255"
  },
  {
    "id":8,
    "start":"255:255",
    "finish":"255:255"
  },
  {
    "id":9,
    "start":"255:255",
    "finish":"255:255"
  },
  {
    "id":10,
    "start":"255:255",
    "finish":"255:255"
  },
  {
    "id":11,
    "start":"255:255",
    "finish":"255:255"
  },
  {
    "id":12,
    "start":"255:255",
    "finish":"255:255"
  },
  {
    "id":13,
    "start":"255:255",
    "finish":"255:255"
  },
  {
    "id":14,
    "start":"255:255",
    "finish":"255:255"
  },
  {
    "id":15,
    "start":"255:255",
    "finish":"255:255"
  },
  {
    "id":16,
    "start":"255:255",
    "finish":"255:255"
  },
  {
    "id":17,
    "start":"255:255",
    "finish":"255:255"
  },
  {
    "id":18,
    "start":"255:255",
    "finish":"255:255"
  },
  {
    "id":19,
    "start":"255:255",
    "finish":"255:255"
  },
  {
    "id":20,
    "start":"255:255",
    "finish":"255:255"
  },
  {
    "id":21,
    "start":"255:255",
    "finish":"255:255"
  },
  {
    "id":22,
    "start":"255:255",
    "finish":"255:255"
  },
  {
    "id":23,
    "start":"255:255",
    "finish":"255:255"
  },
  {
    "id":24,
    "start":"255:255",
    "finish":"255:255"
  },
  {
    "id":25,
    "start":"255:255",
    "finish":"255:255"
  },
  {
    "id":26,
    "start":"255:255",
    "finish":"255:255"
  },
  {
    "id":27,
    "start":"255:255",
    "finish":"255:255"
  },
  {
    "id":28,
    "start":"255:255",
    "finish":"255:255"
  },
  {
    "id":29,
    "start":"255:255",
    "finish":"255:255"
  },
  {
    "id":30,
    "start":"255:255",
    "finish":"255:255"
  },
  {
    "id":31,
    "start":"255:255",
    "finish":"255:255"
  },
  {
    "id":32,
    "start":"255:255",
    "finish":"255:255"
  }
]
*/