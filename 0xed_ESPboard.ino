#include <ESP8266WiFi.h>
#include <espnow.h>
#include <DHT.h>

#define DHTPIN 2
#define DHTTYPE DHT11

DHT dht(DHTPIN, DHTTYPE);

byte board13[] = {0xcc,0x50,0xe3,0xce,0x0b,0xe0};
byte board10[] = {0xcc,0x50,0xe3,0xcd,0x96,0xed};

unsigned long t = 0;
char message;

struct SensorData {
  float temperature;
  float humidity;
};

void setup() {
  Serial.begin(9600);
  
  WiFi.mode(WIFI_STA); //set wifi mode(station mode)

  dht.begin();

  // ESPNOW on
  if (esp_now_init() != 0) {
    Serial.println("ESPNOW 실패!");
    return;
  } else {
    Serial.println("ESPNOW 성공!");
  }

  //board role(Two-way)
  esp_now_set_self_role(ESP_NOW_ROLE_COMBO);
  esp_now_add_peer(board13, ESP_NOW_ROLE_COMBO, 1, NULL, 0); // 뒤에 2개는 ID, PW

  //send,recv callback
  esp_now_register_send_cb(OnDataSent);
  esp_now_register_recv_cb(OnDataRecv);
}

void loop() {
  if (Serial.available()){
    message = Serial.read();
    if (message == 'a'){
      Serial.println("a!!!");
      SensorData data;
      data.temperature = dht.readTemperature();
      data.humidity = dht.readHumidity();
      Serial.println(data.temperature);
      Serial.println(data.humidity);
      // Send sensor data
      if (!isnan(data.temperature) && !isnan(data.humidity)) {
        Serial.println("send!!!");
        esp_now_send(board13, (uint8_t *) &data, sizeof(data));
      }
    }
    else {
      Serial.println(message);
    }
  }
  
}

//send callback function
void OnDataSent(uint8_t *mac, uint8_t status) {
  if (status == 0) { //Sending completed when status is 0
    Serial.println("성공적으로 송신했음!");
  } else {
    Serial.println("송신 실패!!");
  }
}

//recv callback function
void OnDataRecv(uint8_t * mac, uint8_t * data, uint8_t len) {
  String text = "";
  for(int i = 0;i<len;i++){
    text += (char)data[i];
  }
  Serial.println(text);
}
