//Librerías incluidas en el ESP32
#include <vector>
#include <WiFiManager.h>  // https://github.com/tzapu/WiFiManager
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Adafruit_VEML7700.h>
#include <DHT.h>



//Declaración de variables
unsigned long startTime;
unsigned long elapsedTime_lux;    //Tiempo para ajuste de sensor de luz
unsigned long elapsedTime_blink;  //Tiempo para ajuste blink
unsigned long elapsedTime_request;  //Tiempo para envio request


// 192.168.100.156 casa
// 192.168.68.122 boi
String host_name = "http://192.168.100.156:8000";    // Numero de IP
String path_name_body = "/climatic_variables/body";  // Endpoint del servidor, entrega todo el objeto
String path_name = "/climatic_variables";

String payload;

const int wifiConfTrigger = 27;
const int dhtData = 13;

int timeout = 300;  // seconds to run for

bool res;
bool objectReady = false;
bool wifiReady = false;

float temperature;
float humidity;

std::vector<float> avgData = {0.0, 0.0, 0.0};
int avgIndex = 0;



// Alojar documento Json en climaticdata
JsonDocument climaticdata;

//Configuración del esp32 como cliente usando HTTPClient como http
HTTPClient http;

//Configuración del gestor de WiFi con WifiManager como wm
WiFiManager wm;

//Inicializar el sensor de Lux 7700
Adafruit_VEML7700 veml = Adafruit_VEML7700();

//Inicializar el sensor DHT
DHT dht(dhtData, DHT11); 


void setup() {

  //Inicio de protocolos de comunicación
  WiFi.mode(WIFI_STA);  // explicitly set mode, esp defaults to STA+AP
  WiFi.reconnect();

  Serial.begin(9600);  //Inicia comunicación serial
  Serial.println("\n Starting ESP32");

  Wire.begin();  //Inicia comunicación de puestos I2C


  // Inicio de sensores
  if (!veml.begin()) { //inicia el sensor veml7700
  Serial.println("Sensor not found");
  while (1);
  }
  
  dht.begin(); //inicia el sensor dht


  // Ajuste inicial del sensor de luz
  lux_sensor_automatic_adjustment();


  //Asignación de pines
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(wifiConfTrigger, INPUT_PULLUP);
  
}


void loop() { // put your main code here, to run repeatedly: 
  //Take time
  startTime = millis();



  // LED blinks if it is connected
  led_blink();



  // Wifi configuration, triggered by pin 10
  if ( digitalRead(wifiConfTrigger) == LOW ) {
    wifi_connect();
  }




  if (WiFi.status() == WL_CONNECTED){  //La conexión al Wi-fi se ha establecido con éxito
    
    if (!(objectReady)){  //Hace request al servidor y obtiene path_name_body solo una vez
      if (request_server_get(path_name_body, &climaticdata)){
        objectReady = true; //El objeto se ha obtenido con éxito
      } else {
        Serial.println("Connecting ..."); //El objeto aún no se ha obtenido
      }
      
      delay(1000);

    } else {   //Una vez el objeto se encuentra listo corre la siguiente rutina
      
      
      
      if ((startTime - elapsedTime_lux) >= 60000) { //Ajuste automatico de la exposición cada minuto
        // Ajuste del sensor de luxes
        lux_sensor_automatic_adjustment();
        elapsedTime_lux = startTime;
      }



      //Lectura y escritra sensor de humedad y temperatura
      temperature = dht.readTemperature(); // Celsius
      humidity = dht.readHumidity();

      if (!(isnan(humidity) || isnan(temperature))) {
        avgData[2] = avgData[2] + veml.readLux(); //Lectura de sensor de luz   

        avgData[0] = avgData[0] + temperature;//Lectura de sensor DHT   
        avgData[1] = avgData[1] + humidity;

        avgIndex = avgIndex + 1;
      }


      if (avgIndex >= 5){
        climaticdata[0]["temp"] = avgData[0]/avgIndex;
        climaticdata[1]["humid"] = avgData[1]/avgIndex;
        climaticdata[2]["lux"] = avgData[2]/avgIndex;


        Serial.print("temp(Celsius): ");
        Serial.println(climaticdata[0]["temp"].as<float>());

        Serial.print("humid: ");
        Serial.println(climaticdata[1]["humid"].as<float>());

        Serial.print("lux: ");
        Serial.println(climaticdata[2]["lux"].as<float>());

        avgIndex = 0;
        avgData = {0.0, 0.0, 0.0};

      }



      if ((startTime - elapsedTime_request) >= 2000){ //Actualiza la información del servidor cada 2 segundos

        if (request_server_put(path_name, &climaticdata)){
          Serial.println("Data sent!");
        } else {
          Serial.println("Communication error");
        }

        elapsedTime_request = startTime;

      }
    }
  } else { //Si la conexión se cae, reincia objectready

    objectReady = false;
    Serial.println("Wi-Fi is not connected");

    delay(1000);

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



void lux_sensor_automatic_adjustment() {
  // to read lux using automatic method, specify VEML_LUX_AUTO
  float lux = veml.readLux(VEML_LUX_AUTO);

  Serial.println("------------------------------------");
  Serial.print("Lux = ");
  Serial.println(lux);
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



bool request_server_get(String path_name, JsonDocument* doc) {
  http.begin(host_name + path_name); // Inicia comunicación HTTP

  int httpCode = http.GET();
  

  DeserializationError error;
  if (httpCode > 0) { 
    if (httpCode == HTTP_CODE_OK) {
      String payload = http.getString();
      Serial.printf("Request GET successfully to %s — server response (HTTP %d)\n", path_name.c_str(), httpCode);
      error = deserializeJson(*doc, payload);
    } else {
      Serial.printf("[HTTP] GET... code: %d\n", httpCode);
      return false;
    }
  } else {
    Serial.printf("[HTTP] GET... failed, error: %s\n",
    http.errorToString(httpCode).c_str());
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



bool request_server_put(String path_name, JsonDocument* doc){
  http.begin(host_name + path_name); //Inicia comunicación HTTP 
  
  // Definir headers con app.json content type
  http.addHeader("Content-Type", "application/json");

  String output;
  serializeJson(*doc, output);

  int httpCode = http.PUT(output);

  // No se ocupan más checks
  if (httpCode > 0) {
    // Porfa funcione
    if (httpCode == HTTP_CODE_OK) {
      Serial.printf("Request PUT successfully to %s — server response (HTTP %d)\n", path_name.c_str(), httpCode);
    } else {
      // Acá se maneja la conexión si hay errores o no
      Serial.printf("[HTTP] PUT... code: %d\n", httpCode); 
      return false;
    }
  } else {
    //Error managing
    Serial.printf("[HTTP] PUT... failed, error: %s\n", http.errorToString(httpCode).c_str());
    http.end();
    return false;
  }

  http.end();
  return true;
}
