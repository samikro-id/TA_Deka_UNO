#include <ArduinoJson.h>

#define TEGANGAN_PIN    A0
#define ARUS_PIN        A1

#define RELAY1_PIN      8
#define RELAY2_PIN      9

uint32_t led_time;
bool led_state          = false;

float volt;
double arus;
int sensitivitas        = 185; //tegantung sensor arus yang digunakan, yang ini 5A
int teganganoffset      = 2.5;
float teganganbawah     = 10;
float teganganatas      = 13;

uint16_t energy = 0;

uint8_t persen;
bool relay_state        = false;

uint32_t previous_time;

#define SERIAL_BAUD     115200
#define SERIAL_LEN      1000
String serial_buff;
bool serial_complete    = false;

void setup(){
  delay(1000);
  Serial.begin(SERIAL_BAUD);               
  initIo();

  serial_buff.reserve(SERIAL_LEN);
  
  previous_time = millis();
  led_time = millis();
}

void loop(){
  if((millis() - previous_time) > 200){
    bacaSensor();
    previous_time = millis();
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
  float pinvolt = (float) (5 * voltRaw) / 1023;   
  volt = (float) pinvolt * 5;

  /* BACA arus */
  uint16_t arusRaw = analogRead(ARUS_PIN);  
  double pinArus = 5000 * (arusRaw / 1024.0);     
  arus = (float) ((pinArus - teganganoffset) / sensitivitas);

  if(volt > teganganbawah && volt < teganganatas ){
    persen = ((volt - teganganbawah) / (teganganatas - teganganbawah)) * 100;
  }
  else{
    persen = 0;
  }
}

void prosesData(){
  StaticJsonBuffer<SERIAL_LEN> JSONBuffer;

  JsonObject& root = JSONBuffer.parseObject(serial_buff);
  if(root.success()){
    const char * op = root["op"];
    
    if(strcmp(op, "data") == 0){
      serial_buff = "{\"tegangan\":" + String(volt, 1) +",\"arus\":"+ String(arus, 2) + ",\"energy\":" + String(energy, DEC) + ",\"on\":" + String(relay_state, DEC) + "}";
      
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