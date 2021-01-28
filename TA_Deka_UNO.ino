#include <ArduinoJson.h>

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

#define SERIAL_BAUD     115200
#define SERIAL_LEN      1000
String serial_buff;
bool serial_complete    = false;

void setup(){
  delay(100);
  Serial.begin(SERIAL_BAUD);               
  initIo();

  serial_buff.reserve(SERIAL_LEN);
  
  sensor_time = millis();
  led_time = millis();
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
  energy += float((volt * arus) / 3600)         // 3600 detik dalam 1 jam
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
  }
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