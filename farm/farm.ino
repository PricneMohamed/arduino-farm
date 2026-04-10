#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <BLESecurity.h>
#include "DHTesp.h" // 👈 إضافة المكتبة الجديدة الآمنة للـ ESP32

BLEServer* pServer = NULL;
BLECharacteristic* pCharacteristic = NULL;
bool deviceConnected = false;

#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

#define RELAY_PIN 26 
#define CONECTION_LED 4
#define BUZZER_STATE 5
#define GasSensorPin 34
#define DHTPIN 32
#define SOLI_SENSOR_PIN 33
#define TRIG 14
#define ECHO 13

DHTesp dht; // 👈 تعريف كائن الحساس الجديد

bool alertOnDisconnect = false;

int chk;
float hum; 
float temp; 

void AlertState(int length){
  for(int i = 0; i < length; i++){
    digitalWrite(BUZZER_STATE, HIGH);
    delay(200);
    digitalWrite(BUZZER_STATE, LOW);
    delay(200); 
  }
}

class MyServerCallbacks: public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    deviceConnected = true;
    Serial.println("✅ Device Connected");
    digitalWrite(CONECTION_LED, HIGH);
    AlertState(2);
  }

  void onDisconnect(BLEServer* pServer) {
    deviceConnected = false;
    Serial.println("❌ Device Disconnected");
    digitalWrite(CONECTION_LED, LOW); 
    BLEDevice::startAdvertising();
    alertOnDisconnect = true; 
  }
};

class MyCallbacks: public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* pChar) override {
    
    String value = pChar->getValue();
  
       if(value == "PUMP_ON"){
        digitalWrite(RELAY_PIN,LOW);
        digitalWrite(BUZZER_STATE,HIGH);
       }else if(value =="PUMP_OFF"){
        digitalWrite(RELAY_PIN,HIGH);
        digitalWrite(BUZZER_STATE,LOW);
       };


    if (value.length() > 0) {
      int num = value.toInt();

      Serial.print("📥 Received: ");
      Serial.println(num);

      switch(num) {
        case 1:
          Serial.println("🌱 طماطم");
          digitalWrite(RELAY_PIN, HIGH);
          break;
        case 2:
          Serial.println("🥔 بطاطس");
          digitalWrite(RELAY_PIN, LOW);
          break;
        case 3:
          Serial.println("🧅 بصل");
          break;
        case 4:
          Serial.println("🥬 خس");
          break;
        case 5:
          Serial.println("🥕 جزر");
          break;
        case 6:
          Serial.println("🌶️ فلفل");
          break;
        default:
          Serial.println("❌ Unknown");
      }
    }
  }
};


void setup() {
  Serial.begin(115200);

pinMode(RELAY_PIN, OUTPUT);
  pinMode(CONECTION_LED, OUTPUT);
  pinMode(BUZZER_STATE, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH);       
  pinMode(TRIG, OUTPUT);
  pinMode(ECHO, INPUT);
  // 👈 تهيئة حساس DHT22 بالمكتبة الجديدة
  dht.setup(DHTPIN, DHTesp::DHT22);

  BLEDevice::init("ESP32_BLE");

  BLESecurity *pSecurity = new BLESecurity();
  pSecurity->setAuthenticationMode(ESP_LE_AUTH_NO_BOND);

  BLEDevice::setPower(ESP_PWR_LVL_P9);

  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  BLEService *pService = pServer->createService(SERVICE_UUID);

  pCharacteristic = pService->createCharacteristic(
                      CHARACTERISTIC_UUID,
                      BLECharacteristic::PROPERTY_READ |
                      BLECharacteristic::PROPERTY_WRITE |
                      BLECharacteristic::PROPERTY_NOTIFY
                    );

  pCharacteristic->addDescriptor(new BLE2902());
  pCharacteristic->setCallbacks(new MyCallbacks());

  pService->start();

  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);

  BLEDevice::startAdvertising();

  Serial.println("🚀 ESP32 Ready...");
}

void loop() {

digitalWrite(TRIG, LOW);
delayMicroseconds(5);

digitalWrite(TRIG, HIGH);
delayMicroseconds(10);
digitalWrite(TRIG, LOW);

float distance = -1;
bool isPerson = false;

long duration = pulseIn(ECHO, HIGH, 30000);

if(duration == 0){
  Serial.println("⚠️ No echo received");
} else {
  distance = duration * 0.034 / 2;

  if(distance <= 20  && distance > 0){
    isPerson = true;
  }

  Serial.print("Distance: ");
  Serial.print(distance);
  Serial.println(" cm");
}


  if(alertOnDisconnect){
    AlertState(4);    
    alertOnDisconnect = false;
  }

  int soli = analogRead(SOLI_SENSOR_PIN);

    Serial.println("soli => ");
    Serial.println(soli);

  int isGas = analogRead(GasSensorPin);
  int gas = 600;
  if(isGas > gas){
    Serial.println("Gas detected");

    if(deviceConnected && pCharacteristic != NULL){
      uint32_t val = isGas;
      pCharacteristic->setValue(val);
      pCharacteristic->notify();
    }
    
    
    digitalWrite(BUZZER_STATE, HIGH);
    delay(1000);
    digitalWrite(BUZZER_STATE, LOW);
    delay(1000); 
    digitalWrite(BUZZER_STATE, HIGH);
    delay(1000);
    digitalWrite(BUZZER_STATE, LOW);
    delay(1000); 
  }

    

  Serial.println("Gas value => ");
  Serial.println(isGas);

    hum = dht.getHumidity();
  temp = dht.getTemperature();

if(deviceConnected && pCharacteristic != NULL){
    
    String sensorData = 
"T:" + String(temp, 1) + 
",H:" + String(hum, 1) + 
",G:" + String(isGas) +
",S:" + String(soli) +
",D:" + String(distance, 1)+
",P:" + String(isPerson);
    
    pCharacteristic->setValue(sensorData.c_str());
    pCharacteristic->notify();
    
    Serial.print("Sent to App => ");
    Serial.println(sensorData);
  }

//  digitalWrite(RELAY_PIN, HIGH);
//     delay(1000);
//     digitalWrite(RELAY_PIN, LOW);

  // الانتظار لمده ثانيتين على الأقل، لأن حساس DHT22 يكون بطيئاً ويحتاج 2 ثانية ليعطيك قراءة جديدة دقيقة مجدداً
  delay(2000); 
}
