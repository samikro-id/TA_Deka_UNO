#include <ArduinoJson.h>
#include <Wire.h>
#include <DS3231.h>         //  DS3231 Andrew Wickert V1.0.2

#define SOFTWARE_VERSI  2
#define EEPROM_ADDRESS  0x57

#define TEGANGAN_PIN    A0
#define ARUS_PIN        A2

#define RELAY1_PIN      8
#define RELAY2_PIN      9
bool relay_state        = false;

uint32_t led_time;
bool led_state          = false;

#define SENSOR_LOOP     200

#define VOLT_FULL       13.5    // V
#define VOLT_EMPTY      10.5    // V      
float volt;

#define ARUS_SENSITIVITY  0.185 // V  tegantung sensor arus yang digunakan, yang ini 5A
#define ARUS_OFFSET       2.5   // mV
float arus;

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

#define SERIAL_BAUD     9600
#define SERIAL_LEN      400
String serial_buff;
bool serial_complete    = false;

#define SCHEDULE_MAX    9
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

  if(!checkSoftwareVersi()){
    setSoftwareVersi();
    resetScheduleAll();
  }
  
  serial_buff.reserve(SERIAL_LEN);
  
  sensor_time = millis();
  led_time = millis();
  time_time = millis();
}

void loop(){
  if((millis() - sensor_time) > 1000){
    bacaSensor();
    sensor_time = millis();
  }

  if((millis() - led_time) > 200){
    toggleLed();
    led_time = millis();
  }

  if((millis() - time_time) > 200){
    time_now = getClock();
    
    time_time = millis();
  }

  if(time_now.second == 0){
    checkSchedule();
  }

  if(serial_complete){
    prosesData();
    
    serial_complete = false;
    serial_buff = "";
  }
}

void bacaSensor(){
  float pinvolt=0;
  float pinArus=0;

  for(uint8_t n=0; n<SENSOR_LOOP; n++){
    uint16_t voltRaw = analogRead(TEGANGAN_PIN);      
    pinvolt += (float) (5.0 * voltRaw) / 1024.0;  

    uint16_t arusRaw = analogRead(ARUS_PIN);
    pinArus += (float) (5.0 * arusRaw) / 1024.0;  
  }
  /* BACA TEGANGAN */   
  volt = (float) (pinvolt / SENSOR_LOOP) * 5;

  /* BACA arus */
  arus = (float) ((pinArus / SENSOR_LOOP) - ARUS_OFFSET) / ARUS_SENSITIVITY;

  /* HITUNG PERSEN */
  if(volt > VOLT_EMPTY && volt < VOLT_FULL ){
    persen = ((volt - VOLT_EMPTY) / (VOLT_FULL - VOLT_EMPTY)) * 100;
  }
  else{
    persen = 0;
  }

  /* HITUNG ENERGY */
  energy += float((volt * arus) / 3600);        // 3600 detik dalam 1 jam
  if(energy > ENERGY_FULL){    energy = ENERGY_FULL;  }
  if(energy <= ENERGY_EMPTY){  energy = ENERGY_EMPTY; }

  if(volt >= VOLT_FULL){    energy = ENERGY_FULL;   }
  if(volt <= VOLT_EMPTY){   energy = ENERGY_EMPTY;  }
}

void prosesData(){
  StaticJsonBuffer<SERIAL_LEN> JSONBuffer;

  JsonObject& root = JSONBuffer.parseObject(serial_buff);
  if(root.success()){
    const char * op = root["op"];
    
    if(strcmp(op, "data") == 0){
      const char * cmd = root["cmd"];

      if(strcmp(cmd, "get") == 0){
        serial_buff = "";
        serial_buff = "{\"op\":\"data\",\"tegangan\":" + String(volt, 1) +",\"arus\":"+ String(arus, 3) + ",\"energy\":" + String(energy, 2) + ",\"state\":";
        serial_buff += (relay_state == true)?"ON":"OFF";
        serial_buff += ",\"time\":\"" + String(time_now.hour) + ":" + String(time_now.minute) + "\"";
        serial_buff += "}";

        Serial.print(serial_buff);
      }
    }
    else if(strcmp(op, "control") == 0){
      const char * cmd = root["cmd"];

      if(strcmp(cmd, "set") == 0){
        relayState(root["state"]);

        serial_buff = "";
        serial_buff = "{\"op\":\"data\",\"tegangan\":" + String(volt, 1) +",\"arus\":"+ String(arus, 3) + ",\"energy\":" + String(energy, 2) + ",\"state\":";
        serial_buff += (relay_state == true)?"ON":"OFF";
        serial_buff += ",\"time\":\"" + String(time_now.hour) + ":" + String(time_now.minute) + "\"";
        serial_buff += "}";
        
        Serial.print(serial_buff);
      }
    }
    else if(strcmp(op, "wh") == 0){
      const char * cmd = root["cmd"];

      if(strcmp(cmd, "set") == 0){
        const char * state = root["state"];

        if(strcmp(state, "full") == 0){
          energy = ENERGY_FULL;
        }
        else if(strcmp(state, "empty") == 0){
          energy = ENERGY_EMPTY;
        }

        serial_buff = "";
        serial_buff = "{\"op\":\"data\",\"tegangan\":" + String(volt, 1) +",\"arus\":"+ String(arus, 3) + ",\"energy\":" + String(energy, 2) + ",\"state\":";
        serial_buff += (relay_state == true)?"ON":"OFF";
        serial_buff += ",\"time\":\"" + String(time_now.hour) + ":" + String(time_now.minute) + "\"";
        serial_buff += "}";

        Serial.print(serial_buff);
      }
    }
    else if(strcmp(op, "clock") == 0){
      const char * cmd = root["cmd"];

      if(strcmp(cmd, "set") == 0){
        struct time set_time;
        uint8_t index;

        const char *get_clock = root["time"];

        index = String(get_clock).indexOf(':');  
        set_time.hour = String(get_clock).substring(0, index).toInt();
        set_time.minute = String(get_clock).substring(index+1).toInt();

        setClock(set_time);
        time_now = getClock();

        serial_buff = "";
        serial_buff = "{\"op\":\"clock\",\"time\":\"" + String(time_now.hour, DEC) + ":" + String(time_now.minute, DEC) + "\"}";
        // serial_buff = "{\"op\":\"clock\",\"time\":\"" + String(set_time.hour, DEC) + ":" + String(set_time.minute, DEC) + "\"}";

        Serial.print(serial_buff);
      }
      else if(strcmp(cmd,"get") == 0){
        serial_buff = "";
        serial_buff = "{\"op\":\"clock\",\"time\":\"" + String(time_now.hour, DEC) + ":" + String(time_now.minute, DEC) + "\"}";

        Serial.print(serial_buff);
      }
    }
    else if(strcmp(op,"schedule") == 0){
      const char * cmd = root["cmd"];

      if(strcmp(cmd, "delete") == 0){
        uint8_t id = root["id"];

        if(id == 0){
          resetScheduleAll();
        }else{
          deleteSchedule(id);
        }

        serial_buff = "";
        serial_buff = "{\"op\":\"schedule\",\"cmd\":\"delete\",\"id\":" + String(id, DEC) + "}";

        Serial.print(serial_buff);
      }
      else if(strcmp(cmd, "set") == 0){
        struct schedule set_schedule;
        uint8_t index;
        uint8_t id;

        const char *get_start = root["start"];
        index = String(get_start).indexOf(':');
        set_schedule.start_hour = String(get_start).substring(0, index).toInt();
        set_schedule.start_minute = String(get_start).substring(index+1).toInt();

        const char *get_finish = root["finish"];
        index = String(get_finish).indexOf(':');
        set_schedule.finish_hour = String(get_finish).substring(0, index).toInt();
        set_schedule.finish_minute = String(get_finish).substring(index+1).toInt();

        id = setSchedule(set_schedule);

        serial_buff = "";
        serial_buff = "{\"op\":\"schedule\",\"cmd\":\"set\",\"id\":" + String(id, DEC) + "}";
        
        Serial.print(serial_buff);
      }
      else if(strcmp(cmd, "get") == 0){
        uint8_t id = root["id"];

        if(id == 0){          // get all schedule
          bool first = false;

          serial_buff = "{\"op\":\"schedule\",\"cmd\":\"getAll\",\"data\":";
          serial_buff += "[";

          /*** get first schedule ****/
          for(uint8_t i=1; i<=SCHEDULE_MAX; i++){
            struct get_schedule first_schedule;

            first_schedule = getSchedule(i);
            if(first_schedule.success){
              if(!first){
                first = true;
              }else{
                serial_buff += ",";
              }

              serial_buff += "{\"id\":"; 
              serial_buff += String(i); 
              serial_buff += ",\"start\":\"" + String(first_schedule.start_hour, DEC) + ":" + String(first_schedule.start_minute, DEC);
              serial_buff += "\",\"finish\":\"" + String(first_schedule.finish_hour, DEC) + ":" + String(first_schedule.finish_minute, DEC);
              serial_buff += "\"}";
            }
          }

          serial_buff += "]}";
          Serial.print(serial_buff);
          serial_buff = "";
        }
        else{
          serial_buff = "";

          struct get_schedule first_schedule;

          first_schedule = getSchedule(id);
          if(first_schedule.success){
            serial_buff += "{\"op\":\"schedule\",\"cmd\":\"get\",\"id\":" + String(id, DEC); 
            serial_buff += ",\"start\":\"" + String(first_schedule.start_hour, DEC) + ":" + String(first_schedule.start_minute, DEC);
            serial_buff += "\",\"finish\":\"" + String(first_schedule.finish_hour, DEC) + ":" + String(first_schedule.finish_minute, DEC);
            serial_buff += "\"}";
          }else{
            serial_buff = "{}";
          }

          Serial.print(serial_buff);
          serial_buff = "";
        }
      }
    }
  }
}


/******* Clock section ******/
void setClock(struct time set){
  clock.setClockMode(false);    // set to 24h

  if(time_now.hour != set.hour || time_now.minute != set.minute){
    clock.setHour(set.hour);
    clock.setMinute(set.minute);
    clock.setSecond(set.second);
  }
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

  for(uint8_t i=1; i<=SCHEDULE_MAX; i++){
    schedule_now = getSchedule(i);
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
  uint32_t flag = 0;

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
  for(uint8_t i=1; i<=SCHEDULE_MAX; i++){
    if((flag & flag_begin) == 0){
      
      /*** set new flag ***/
      flag |= flag_begin;
      setScheduleFlag(flag);

      i2c_eeprom_write_byte(EEPROM_ADDRESS, 5 + (4 * (i-1)), set_schedule.start_hour);
      i2c_eeprom_write_byte(EEPROM_ADDRESS, 6 + (4 * (i-1)), set_schedule.start_minute);

      i2c_eeprom_write_byte(EEPROM_ADDRESS, 7 + (4 * (i-1)), set_schedule.finish_hour);
      i2c_eeprom_write_byte(EEPROM_ADDRESS, 8 + (4 * (i-1)), set_schedule.finish_minute);

      schedule_id = i;
      break;
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

void deleteSchedule(uint8_t id){
  uint32_t get_flag;
  uint32_t set_flag = 0x00000001;
  uint32_t not_flag;

  for(uint8_t i=0; i<(id-1); i++){
    set_flag <<=1;
  }
  not_flag = ~set_flag;

  get_flag = getScheduleFlag();
  get_flag &= not_flag;

  setScheduleFlag(get_flag);
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
      if(!serial_complete){
        serial_buff += inChar;
      }
    }
  }
}


/****** Relay function*******/
void relayState(bool state){
  if(state){  digitalWrite(RELAY1_PIN, LOW);  }
  else{       digitalWrite(RELAY1_PIN, HIGH);   }
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
{
  "op":"schedule",
  "cmd":"getAll",
  "data":
  [
    {
      "id":1,
      "start":"1:1",
      "finish":"2:2"
    },
    {
      "id":2,
      "start":"2:2",
      "finish":"3:3"
    },
    {
      "id":3,
      "start":"3:3",
      "finish":"4:4"
    },
    {
      "id":4,
      "start":"4:4",
      "finish":"5:5"
    },
    {
      "id":5,
      "start":"5:5",
      "finish":"6:6"
    },
    {
      "id":6,
      "start":"6:6",
      "finish":"7:7"
    },
    {
      "id":7,
      "start":"7:7",
      "finish":"8:8"
    },
    {
      "id":8,
      "start":"8:8",
      "finish":"9:9"
    },
    {
      "id":9,
      "start":"9:9",
      "finish":"10:10"
    },
    {
      "id":10,
      "start":"10:10",
      "finish":"11:11"
    }
  ]
}

{
  "op":"schedule",
  "cmd":"getAll",
  "data":
  [
    {
      "id":1,
      "start":"4:40",
      "finish":"4:41"
      }{"id":2,"start":"5:15","finish":"5:25"}{"id":3,"start":"5:35","finish":"5:45"}{"id":4,"start":"5:55","finish":"6:5"}{"id":5,"start":"6:15","finish":"6:25"}{"id":6,"start":"6:35","finish":"6:45"}{"id":7,"start":"6:55","finish":"7:5"}{"id":8,"start":"7:15","finish":"7:25"}{"id":9,"start":"7:35","finish":"7:45"}{"id":10"}]}
*/