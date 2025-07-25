//Librerías incluidas en el ESP32
#include <WiFiManager.h> // https://github.com/tzapu/WiFiManager
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Adafruit_VEML7700.h>



//Declaración de variables
unsigned long startTime;
unsigned long elapsedTime_lux; //Tiempo para ajuste de sensor de luz
unsigned long elapsedTime_blink; //Tiempo para ajuste blink


// 192.168.100.156 casa
// 192.168.68.126 boi
String host_name   = "http://192.168.100.156:8000"; // Numero de IP
String path_name_body   = "/climatic_variables/body";      // Endpoint del servidor, entrega todo el objeto
String path_name  = "/climatic_variables";

String payload;

const int wifiConfTrigger = 27;

int timeout = 120; // seconds to run for

bool res;
bool objectReady = false;



// Alojar documento Json en climaticdata
JsonDocument climaticdata;

//Configuración del esp32 como cliente usando HTTPClient como http
HTTPClient http;

//Configuración del gestor de WiFi con WifiManager como wm
WiFiManager wm;

//Inicializar el sensor de Lux 7700
Adafruit_VEML7700 veml = Adafruit_VEML7700();


void setup() {

  //Inicio de protocolos de comunicación  
  WiFi.mode(WIFI_STA); // explicitly set mode, esp defaults to STA+AP
  WiFi.reconnect();
  
  Serial.begin(9600); //Inicia comunicación serial
  Serial.println("\n Starting ESP32");

  Wire.begin (); //Inicia comunicación de puestos I2C

  //Asignación de pines
  // pinMode(8, OUTPUT);Wire.begin ();
  

  pinMode(wifiConfTrigger, INPUT_PULLUP);

}


void loop() { // put your main code here, to run repeatedly:
  //Take time
  startTime = millis();



  // Wifi configuration, triggered by pin 10
  if ( digitalRead(wifiConfTrigger) == LOW ) {
    wifi_connect();
  }
  Serial.println("1");


  //Hace request al servidor y obtiene path_name_body solo una vez
  if (!(objectReady) && (WiFi.status() == WL_CONNECTED)){
    request_server_get(path_name_body, &climaticdata);
    objectReady = true;
  } 
  Serial.println("2");


  //Si la conexión se cae, reincia objectready
  if (WiFi.status() != WL_CONNECTED) {
    delayMicroseconds(1000000);
    objectReady = false;
    Serial.println("Wi-Fi is not connected");
  }
  Serial.println("3");


  if (objectReady){
    if ((startTime - elapsedTime_lux) >= 300000) {
      // Ajuste del sensor de luxes
      lux_sensor_automatic_adjustment ();
      elapsedTime_lux = startTime;
    }
    Serial.println("Eso tilín");
    

    climaticdata[2]["lux"] = veml.readLux();
    Serial.print("lux: ");
    Serial.println(climaticdata[2]["lux"].as<float>());

    
  }

  delay(1000);

}





// void led_blink(){
//   if (waterdata[2]["irrigation"]){
//     if ((startTime - elapsedTime) >= 2000) {
//     digitalWrite(8, HIGH);
//     } else {
//     digitalWrite(8, LOW);        
//     }
//   } else {
//     digitalWrite(8, LOW);
//   }
// }



void wifi_connect(){

  // digitalWrite(8, LOW);

  //reset settings - for testing
  wm.resetSettings();

  // set configportal timeout
  // wm.setConfigPortalTimeout(timeout);

  res = wm.autoConnect("ESP32_WifiConfig","jijijaja");
  if (!res) {
    Serial.println("Failed to connect");
    // digitalWrite(8, HIGH);
    ESP.restart();
  }

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  } 

  Serial.println("");
  Serial.println("WiFi connected...yeey :3");
  // digitalWrite(8, HIGH);
}



void lux_sensor_automatic_adjustment (){
  // to read lux using automatic method, specify VEML_LUX_AUTO
  float lux = veml.readLux(VEML_LUX_AUTO);

  Serial.println("------------------------------------");
  Serial.print("Lux = "); Serial.println(lux);
  Serial.println("Settings used for reading:");
  Serial.print(F("Gain: "));
  switch (veml.getGain()) {
    case VEML7700_GAIN_1: Serial.println("1"); break;
    case VEML7700_GAIN_2: Serial.println("2"); break;
    case VEML7700_GAIN_1_4: Serial.println("1/4"); break;
    case VEML7700_GAIN_1_8: Serial.println("1/8"); break;
  }
  Serial.print(F("Integration Time (ms): "));
  switch (veml.getIntegrationTime()) {
    case VEML7700_IT_25MS: Serial.println("25"); break;
    case VEML7700_IT_50MS: Serial.println("50"); break;
    case VEML7700_IT_100MS: Serial.println("100"); break;
    case VEML7700_IT_200MS: Serial.println("200"); break;
    case VEML7700_IT_400MS: Serial.println("400"); break;
    case VEML7700_IT_800MS: Serial.println("800"); break;
  }

}



void request_server_get(String path_name, JsonDocument* doc){
  http.begin(host_name + path_name); //Inicia comunicación HTTP

  int httpCode = http.GET();

  if (!(http.connected())) {
    http.end();
    return;
  }
  
  DeserializationError error;

  Serial.println(path_name);

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
    }
  } else {
    Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
    http.end();
    return;
  }

  if (error) {
    Serial.print("deserializeJson() failed: ");
    Serial.println(error.c_str());
    http.end();
    return;
  }
  
  http.end();
}



void request_server_put(String path_name, JsonDocument* doc){
  http.begin(host_name + path_name); //Inicia comunicación HTTP

  if (!(http.connected())) {
    http.end();
    return;
  }

  String output;


  serializeJson(*doc, output);

  // Serial.println(output);

  int httpCode = http.PUT(output);

  // httpCode will be negative on error
  if (httpCode > 0) {
    // file found at server
    if (httpCode == HTTP_CODE_OK) {
      Serial.printf("Request successfully to %s\n", path_name);

    } else {
      // HTTP header has been send and Server response header has been handled
      Serial.printf("[HTTP] GET... code: %d\n", httpCode);
    }
  } else {
    Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
    http.end();
    return;
  }

  http.end();
}
