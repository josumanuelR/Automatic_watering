//Librerías incluidas en el ESP32
#include <WiFiManager.h> // https://github.com/tzapu/WiFiManager
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <cmath>

// #include <typeinfo>



//Declaración de variables
unsigned long startTime;
unsigned long elapsedTime; //para request get de irrigation


// 192.168.100.156 casa
// 192.168.68.126 boi
String host_name   = "http://192.168.100.156:8000"; // Numero de IP
String path_name_body   = "/water_data/body";      // Endpoint del servidor, descargar todo el objeto
String path_name_water_flow   = "/water_data/1";      //Endpoint del servidor, descarga solo un path


String payload;


volatile unsigned long count = 0;


const int wifiConfTrigger = 17;
const int dataReceiver = 16;


int timeout = 120; // seconds to run for


bool resetRequestData = true;
bool res;
bool objectReady = false;



// Alojar documento Json en waterdata
JsonDocument waterdata;
JsonDocument formartJson;

//Configuración del esp32 como cliente usando HTTPClient como http
HTTPClient http;

//Configuración del gestor de WiFi con WifiManager como wm
WiFiManager wm;



void IRAM_ATTR countPulse() { //IRAM permite que la funcion se almacene en el RAM en lugar de la memoria flash
  count++;   // This function is called automatically on each rising edge
}



void setup() {
  
    //Inicio de protocolos de comunicación  
  WiFi.mode(WIFI_STA); // explicitly set mode, esp defaults to STA+AP
  WiFi.reconnect();
  
  Serial.begin(9600); //Inicia comunicación serial
  Serial.println("\n Starting ESP32");


  //Asignación de pines
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(wifiConfTrigger, INPUT_PULLUP);
  pinMode(dataReceiver, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(dataReceiver), countPulse, RISING);

}


void loop() { // put your main code here, to run repeatedly:
  //Take time
  startTime = millis();



  // Wifi configuration, triggered by pin 10
  if ( digitalRead(wifiConfTrigger) == LOW ) {
    wifi_connect();
  }




  if (WiFi.status() == WL_CONNECTED){  //La conexión al Wi-fi se ha establecido con éxito
    
    if (!(objectReady)){  //Hace request al servidor y obtiene path_name_body solo una vez
      if (request_server_get(path_name_body, &waterdata)){
        objectReady = true; //El objeto se ha obtenido con éxito
      } else {
        Serial.println("Connecting ..."); //El objeto aún no se ha obtenido
      }
      
      delay(1000);

    } else {   //Una vez el objeto se encuentra listo corre la siguiente rutina


      // Serial.println(elapsedTime);
      if ((startTime - elapsedTime) >= 1000){
        // Serial.println(count);
        waterdata[0]["water_flow"] = round((count/(7.5*60))* 10000.0) / 10000.0;

        Serial.print("Flujo(l/s): ");
        Serial.println(waterdata[0]["water_flow"].as<float>());

        led_blink(); // led_blink(); //Cuando "water_flow" esta activo el led parpadea

        if ((resetRequestData)||(waterdata[0]["water_flow"]!=0)){

          formartJson = waterdata[0];

          if (request_server_put(path_name_water_flow, &formartJson)){       //Requests at server PUT
            Serial.println("Data sent!");
          } else {
            Serial.println("Communication error");
          }

          if (waterdata[0]["water_flow"]==0) {
            resetRequestData = false;
          } else {
            resetRequestData = true;
          } 
        }
        
        count = 0;
        elapsedTime = startTime;

      }
    }
  } else { //Si la conexión se cae, reincia objectready

    objectReady = false;
    Serial.println("Wi-Fi is not connected");

    delay(1000);

  }

}




void led_blink(){
  if (waterdata[0]["water_flow"]!=0){
    digitalWrite(LED_BUILTIN, HIGH);
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
