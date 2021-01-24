#include <ESP8266_Lib.h>
#include <SoftwareSerial.h>

#define TEGANGAN_PIN    A0
#define ARUS_PIN        A1

#define RELAY1_PIN      8
#define RELAY2_PIN      9

#define WIFI_RX         2
#define WIFI_TX         3

#define ESP826_BAUD     115200

char ssid[] = "samikro";
char pass[] = "samikroid";

SoftwareSerial EspSerial(WIFI_RX, WIFI_TX);
ESP8266 wifi(&EspSerial);

float Volt;
double Arus;
int sensitivitas = 185; //tegantung sensor arus yang digunakan, yang ini 5A
int teganganoffset = 2.5;
float teganganbawah = 10;
float teganganatas = 13;

uint8_t persen;
uint32_t previous_time;

void setup(){
    Serial.begin(115200);               Serial.println("start");
    pinMode(RELAY1_PIN, OUTPUT);
    pinMode(RELAY2_PIN, OUTPUT);

    digitalWrite(RELAY1_PIN, HIGH);
    digitalWrite(RELAY2_PIN, HIGH);
    delay(100);

    EspSerial.begin(ESP826_BAUD);

    previous_time = millis();
}

void loop(){
    if((millis() - previous_time) > 200){
        bacaSensor();
        previous_time = millis();
    }
}

void bacaSensor(){
    /* BACA TEGANGAN */
    uint16_t VoltRaw = analogRead(TEGANGAN_PIN);      
    float pinVolt = (float) (5 * VoltRaw) / 1023;   
    Volt = (float) pinVolt * 5;                   Serial.print("BAT : ");     Serial.println(Volt);

    /* BACA ARUS */
    uint16_t ArusRaw = analogRead(ARUS_PIN);      //Serial.print("RAW : ");     Serial.println(ArusRaw); 
    double pinArus = 5000 * (ArusRaw / 1024.0);        //Serial.print("PIN : ");     Serial.println(pinArus);
    Arus = (float) ((pinArus - teganganoffset) / sensitivitas); Serial.print("ARU : ");     Serial.println(Arus);

    if(Volt > teganganbawah && Volt < teganganatas ){
      persen = ((Volt - teganganbawah) / (teganganatas - teganganbawah)) * 100; Serial.println(persen);
    }
    else{
      persen = 0;
    }
}