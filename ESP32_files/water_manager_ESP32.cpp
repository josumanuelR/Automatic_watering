//Librerías incluidas en el ESP32
#include <WiFiManager.h> // https://github.com/tzapu/WiFiManager
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// #include <typeinfo>



//Declaración de variables
unsigned long startTime;
unsigned long elapsedTime; //para request get de irrigation


// 192.168.100.156 casa
// 192.168.68.126 boi
String host_name   = "http://192.168.100.156:8000"; // Numero de IP
String path_name_body   = "/water_data/body";      // Endpoint del servidor, descargar todo el objeto
String path_name_water_tank_level   = "/water_data/2";      //Endpoint del servidor, descarga solo un path
String path_name_irrigation   = "/water_data/3";
String path_name_tap_water_valve   = "/water_data/4";
String path_name_pump   = "/water_data/5";
String path_name_drop_water_tank_valve   = "/water_data/6";
String path_name_error_code   = "/water_data/7";

String payload;

const int trigPin = 16;
const int echoPin = 17;
const int wifiConfTrigger = 10;

int valves[] = {5, 23, 19, 18};
int timeout = 120; // seconds to run for

double duration, longitude; 
float avrWaterLevel = 0;
float waterLevel = 0;
int count = 0;
bool res;
bool objectReady = false;
bool sendData = true;
bool sendOverflow = true;

float minValue = 25;
float maxValue = 125;


// Alojar documento Json en waterdata
JsonDocument waterdata;
JsonDocument irrigationdata;
JsonDocument formartJson;

//Configuración del esp32 como cliente usando HTTPClient como http
HTTPClient http;

//Configuración del gestor de WiFi con WifiManager como wm
WiFiManager wm;


void setup() {

  //Inicio de protocolos de comunicación  
  WiFi.mode(WIFI_STA); // explicitly set mode, esp defaults to STA+AP
  WiFi.reconnect();
  
  Serial.begin(9600); //Inicia comunicación serial
  Serial.println("\n Starting ESP32");


  //Asignación de pines
  pinMode(LED_BUILTIN, OUTPUT);
	pinMode(echoPin, INPUT);  
  pinMode(trigPin, OUTPUT);  
  
  for (int i = 0; i < 4; i++){
    pinMode(valves[i], OUTPUT);
    digitalWrite(valves[i], HIGH);

  }

  pinMode(wifiConfTrigger, INPUT_PULLUP);


}


void loop() { // put your main code here, to run repeatedly:
  //Take time
  startTime = millis();



  // Wifi configuration, triggered by pin 10
  if ( digitalRead(wifiConfTrigger) == LOW ) {
    wifi_connect();
  }



  //Hace request al servidor y obtiene path_name_body solo una vez
  if (!(objectReady) && (WiFi.status() == WL_CONNECTED)){
    request_server_get(path_name_body, &waterdata);
    objectReady = true;
  } 



  //Si la conexión se cae, reincia objectready
  if (WiFi.status() != WL_CONNECTED) {
    delayMicroseconds(1000000);
    objectReady = false;
    Serial.println("Wi-Fi is not connected");
  }



  if (objectReady){
    // Ultrasonic sensor reading
    ultrasonic_tank_sensor();



    //Requests at server GET
    // Serial.println(elapsedTime);
    if ((startTime - elapsedTime) >= 3000){
      request_server_get(path_name_irrigation, &irrigationdata);
      waterdata[2] = irrigationdata;
      elapsedTime = startTime;
    }

  
    
    //Determina que variable activar en funcion del nivel de agua disponible en el tanque
    if (waterdata[1]["water_tank_level"] <= 0.010){ //nivel mínimo de agua en el tanque 1%

      if(waterdata[4]["pump"]){
        sendData = true;
      }
      waterdata[3]["tap_water_valve"] = true; //Si el nivel del agua es menor al 1% entonces abre la llave del agua
      waterdata[4]["pump"] = false;
    } else {

      if(waterdata[3]["tap_water_valve"]){
        sendData = true;
      }
      waterdata[3]["tap_water_valve"] = false; //Si el nivel del agua es mayor al 1% entonces usa el tanque de agua
      waterdata[4]["pump"] = true;
    }



    //Hacer check para comprobar nivel de agua máximo, si es true abre valvula de emergencia
    if (waterdata[1]["water_tank_level"] >= 0.980){ //nivel máximo de agua en el tanque 98%

      if (!(waterdata[5]["drop_water_tank_valve"])){
        sendOverflow = true;
      }
      waterdata[5]["drop_water_tank_valve"] = true;
      digitalWrite(valves[2], LOW);

    } else {

      if (waterdata[5]["drop_water_tank_valve"]){
        sendOverflow = true;
      }
      waterdata[5]["drop_water_tank_valve"] = false;
      digitalWrite(valves[2], HIGH);
      
    }



    if (sendOverflow){
      formartJson = waterdata[5];
      request_server_put(path_name_drop_water_tank_valve, &formartJson);
      sendOverflow = false;
    }



    //Se encarga de endender o apargar las valvulas correspodientes
    if (waterdata[2]["irrigation"]){ 

      if (waterdata[3]["tap_water_valve"]){
        digitalWrite(valves[0], LOW); //valvula asignada a tap_water (enciende)
        digitalWrite(valves[1], HIGH);
      }

      if (waterdata[4]["pump"]){
        digitalWrite(valves[1], LOW);//valvula asignada a pump (enciende)
        digitalWrite(valves[0], HIGH);
      }

    } else {
      
      digitalWrite(valves[0], HIGH); //valvula asignada a tap_water (apaga)
      digitalWrite(valves[1], HIGH);//valvula asignada a pump (apaga)
    }



    led_blink(); //Cuando irrigation esta activo el led parpadea



    if (sendData == true){ //envia datos si nivel de agua cambia 

      functionCallback();
      sendData = false;
    }


  }
}

void functionCallback(){

  formartJson = waterdata[3];
  request_server_put(path_name_tap_water_valve, &formartJson); //request a tap_water


  formartJson = waterdata[4]; //request a pump
  request_server_put(path_name_pump, &formartJson);

}



void led_blink(){
  if (waterdata[2]["irrigation"]){
    if ((startTime - elapsedTime) >= 2000) {
    digitalWrite(LED_BUILTIN, HIGH);
    } else {
    digitalWrite(LED_BUILTIN, LOW);        
    }
  } else {
    digitalWrite(LED_BUILTIN, LOW);
  }
}



void wifi_connect(){

  digitalWrite(LED_BUILTIN, HIGH);

  //reset settings - for testing
  wm.resetSettings();

  // set configportal timeout
  wm.setConfigPortalTimeout(timeout);

  res = wm.autoConnect("ESP32_WifiConfig","jijijaja");
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



void ultrasonic_tank_sensor(){
  
  digitalWrite(trigPin, LOW);  
	delayMicroseconds(2);  
	digitalWrite(trigPin, HIGH);  
	delayMicroseconds(10);  
	digitalWrite(trigPin, LOW);  

  duration = pulseIn(echoPin, HIGH);
  longitude = (duration*.0343)/2;

  // Serial.print("Distance: ");
  // Serial.println(longitude);

  waterLevel = constrain((map(longitude, minValue, maxValue, 1000, 0)), 0, 1000);
  avrWaterLevel = avrWaterLevel + waterLevel;
  count ++;

  if (count >= 20){ //genera un nivel de tanque promedio con 30 mediciones
    
    waterdata[1]["water_tank_level"] = (avrWaterLevel/20)/1000;

    Serial.print("Tank water level: ");
    Serial.println(waterdata[1]["water_tank_level"].as<float>()*100);

    formartJson = waterdata[1];
    request_server_put(path_name_water_tank_level, &formartJson);
    avrWaterLevel = 0;
    count = 0;
  }



  delay(100);
  return ;

}



void request_server_get(String path_name, JsonDocument* doc){
  http.begin(host_name + path_name); //Inicia comunicación HTTP

  int httpCode = http.GET();

  if (!(http.connected())) {
    http.end();
    return;
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
