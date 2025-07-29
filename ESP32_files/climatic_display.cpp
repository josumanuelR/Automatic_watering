//Librerías incluidas en el ESP32
#include <WiFiManager.h>  // https://github.com/tzapu/WiFiManager
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <LiquidCrystal.h>
#include <iostream>
#include <string>



//Declaración de variables
unsigned long startTime;
unsigned long elapsedTime_blink;  //Tiempo para ajuste blink



// 192.168.100.156 casa
// 192.168.68.122 boi
String host_name = "http://192.168.100.156:8000";    // Numero de IP
String path_name_body = "/climatic_variables/body";  // Endpoint del servidor, entrega todo el objeto


String payload;

const int wifiConfTrigger = 21;

int timeout = 300;  // seconds to run for

bool res;
bool objectReady = false;
bool wifiReady = false;

std::string measures[] = {"Temperatura", "Humedad relativa", "Luxes"};
std::string text0;
std::string text1;

// Alojar documento Json en climaticdata
JsonDocument climaticdata;

//Configuración del esp32 como cliente usando HTTPClient como http
HTTPClient http;

//Configuración del gestor de WiFi con WifiManager como wm
WiFiManager wm;

// initialize the library by associating any needed LCD interface pin
// with the arduino pin number it is connected to
const int rs = 5, en = 6, d4 = 0, d5 = 1, d6 = 2, d7 = 3;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

void setup() {

  //Inicio de protocolos de comunicación
  WiFi.mode(WIFI_STA);  // explicitly set mode, esp defaults to STA+AP
  WiFi.reconnect();

  Serial.begin(9600);  //Inicia comunicación serial
  Serial.println("\n Starting ESP32");

  // set up the LCD's number of columns and rows:
  lcd.begin(16, 2);

  // turn on the cursor:
  lcd.cursor();

  //Asignación de pines
  pinMode(8, OUTPUT);
  pinMode(wifiConfTrigger, INPUT_PULLUP);
  
}



void loop() {  // put your main code here, to run repeatedly:
  //Take time
  startTime = millis();



  // LED blinks if it is connected
  led_blink();



  // Wifi configuration, triggered by pin 27
  if (digitalRead(wifiConfTrigger) == LOW) {
    wifi_connect();
  }



  //Revisa si se encuentra conectado al wifi
  if ((WiFi.status() == WL_CONNECTED) && (!(wifiReady))) {

    Serial.println("Connected to Wi-Fi");
    wifiReady = true;

  } else if (WiFi.status() != WL_CONNECTED) {

    Serial.println("Not connected to Wi-Fi");
    wifiReady = false;
    objectReady = false;
  }



  //Revisa si se encuentra al servidor y obtuvo el objecto correactamente
  if (wifiReady){
    if (!(objectReady)){

      bool error;
      error = request_server_get(path_name_body, &climaticdata);

      if (error){
        if (climaticdata.is<JsonArray>()) {
          Serial.println("Connected with the server, object ready to use");
          objectReady = true;
        } else {
          Serial.println("Connected with the server, error requesting object");
          objectReady = false;
        }
      } else {
        Serial.println("Failed to connect with server");
        objectReady = false;
      }
    }
  }
  
  

  if (objectReady) {                             //Reading sensors and sending data
  
    bool error = request_server_get(path_name_body, &climaticdata);

    float data[] = {climaticdata[0]["temp"], climaticdata[1]["humid"], climaticdata[2]["lux"]};

    int size = sizeof(measures) / sizeof(measures[0]);  // Get number of elements
  
    for (int i = 0; i < size; i++) {

      data[i] = std::floor(data[i] * 100) / 100;

      text0 = measures[i];
      text1 = std::to_string(data[i]);

      print_lcd_display ();

      

    }
    
    // if (!(error)){
    //   objectReady = false;
    // }

  }

}




void led_blink(){
  if (objectReady){
    if ((startTime - elapsedTime_blink) >= 2900) {
      digitalWrite(LED_BUILTIN, HIGH);
    } else {
      digitalWrite(LED_BUILTIN, LOW);
    }

    if ((startTime - elapsedTime_blink) >= 3000) {
      elapsedTime_blink = startTime;
    } 
  } else {
    digitalWrite(LED_BUILTIN, LOW);
  }

}



void wifi_connect() {

  digitalWrite(LED_BUILTIN, HIGH);

  //reset settings - for testing
  wm.resetSettings();

  // set configportal timeout
  wm.setConfigPortalTimeout(timeout);

  res = wm.autoConnect("ESP32_WifiConfig", "jijijaja");
  if (!res) {
    Serial.println("Failed to connect");
    digitalWrite(LED_BUILTIN, LOW);
    ESP.restart();
  }

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected...yeey :3");
  digitalWrite(LED_BUILTIN, LOW);
}



bool request_server_get(String path_name, JsonDocument* doc) {
  http.begin(host_name + path_name);  //Inicia comunicación HTTP

  int httpCode = http.GET();

  if (!(http.connected())) {
    Serial.println("Can't establish HTTP connection");
    http.end();
    return false;
  }

  DeserializationError error;

  // httpCode will be negative on error
  if (httpCode > 0) {
    // file found at server
    if (httpCode == HTTP_CODE_OK) {
      String payload = http.getString();
      // Serial.println(payload);
      Serial.printf("Request successfully to %s\n", path_name);
      error = deserializeJson(*doc, payload);
    } else {
      // HTTP header has been send and Server response header has been handled
      Serial.printf("[HTTP] GET... code: %d\n", httpCode);
      http.end();
      return false;
    }
  } else {
    Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
    http.end();
    return false;
  }

  if (error) {
    Serial.print("deserializeJson() failed: ");
    Serial.println(error.c_str());
    http.end();
    return false;
  }

  http.end();
  return true;
}



void print_lcd_display (){

  lcd.home();
  
  for (int i = 0; i < text0.length(); i++) { //imprime primera linea de texto
    lcd.leftToRight();
    lcd.write(text0[i]);
    // Serial.println(text[i]);
    delay(50);
  }
  lcd.setCursor(0, 1);
  for (int i = 0; i < text1.length(); i++) { //imprime segunda linea de texto
    lcd.leftToRight();
    lcd.write(text1[i]);
    // Serial.println(text[i]);
    delay(50);
  }
  delay(5000);

  lcd.home();

  for (int i = 0; i < 16; ++i){ //borra primera linea de texto
    lcd.leftToRight();
    lcd.write(" ");
    // Serial.println(" ");
    delay(50);
  }
  lcd.setCursor(0, 1);
  for (int i = 0; i < 16; ++i){ //borra segunda linea de texto
    lcd.leftToRight();
    lcd.write(" ");
    // Serial.println(" ");
    delay(50);
  }
  delay(500);
}
