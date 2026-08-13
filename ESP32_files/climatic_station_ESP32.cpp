//Librerías incluidas en el ESP32
#include <vector>
#include <WiFiManager.h>  // https://github.com/tzapu/WiFiManager
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <Adafruit_VEML7700.h>
#include <DHT.h>



//Declaración de variables
unsigned long startTime;
unsigned long elapsedTime_lux;    //Tiempo para ajuste de sensor de luz
unsigned long elapsedTime_blink;  //Tiempo para ajuste blink
unsigned long elapsedTime_request;  //Tiempo para envio request


// The ESP32 is now the HTTP server.
// A client can request the latest climatic data with:
// GET http://<ESP32_IP>/climatic_variables

const int wifiConfTrigger = 27;
const int dhtData = 13;

int timeout = 300;  // seconds to run for

bool res;
bool wifiReady = false;
bool serverRunning = false;  // Tracks whether server.begin() has been called

// --- Wi-Fi recovery state machine ---
// TRYING:     just disconnected, retrying normally
// SLEEPING:   gave up for now, radio is off, waiting for next cycle
// ATTEMPTING: radio woken up, giving it a brief window to reconnect
enum WifiRecoveryState { WIFI_TRYING, WIFI_SLEEPING, WIFI_ATTEMPTING };
WifiRecoveryState wifiState = WIFI_TRYING;
unsigned long wifiStateStart = 0;        // when the current state began
unsigned long lastReconnectAttempt = 0;  // throttles retries within TRYING

const unsigned long tryDuration = 60000;       // 1 min of normal retries before sleeping
const unsigned long reconnectInterval = 10000; // retry every 10s while TRYING
const unsigned long sleepDuration = 120000;    // 2 min radio-off between attempts
const unsigned long attemptDuration = 10000;   // 10s window to reconnect once woken

float temperature;
float humidity;

std::vector<float> avgData = {0.0, 0.0, 0.0};
int avgIndex = 0;


//Climatic data Json file
struct Reading { int id; const char* key; float value; };
Reading readings[] = {
  {1, "temp", 0.0},
  {2, "humid", 0.0},
  {3, "lux", 0.0}
};



JsonDocument doc;
JsonArray climaticdata = doc.to<JsonArray>();

void initClimaticData() {
  for (auto &r : readings) {
    JsonObject o = climaticdata.add<JsonObject>();
    o["id"] = r.id;
    o[r.key] = r.value;
  }
}

// ESP32 HTTP server
WebServer server(80);

//Configuración del gestor de WiFi con WifiManager como wm
WiFiManager wm;

//Inicializar el sensor de Lux 7700
Adafruit_VEML7700 veml = Adafruit_VEML7700();

//Inicializar el sensor DHT
DHT dht(dhtData, DHT22); 




void handleClimaticData() {
  // Send the most recent sensor values only when a client performs GET.
  String output;
  serializeJson(climaticdata, output);

  server.send(200, "application/json", output);

  Serial.println("GET /climatic_variables -> data sent");
}


void handleNotFound() {
  server.send(404, "text/plain", "Not found");
}


void setup() {

  setCpuFrequencyMhz(80);

  // Build the initial climatic data JSON array (id/key/value entries).
  initClimaticData();

  // Inicio de protocolos de comunicación
  WiFi.mode(WIFI_STA);

  // Disable Wi-Fi modem sleep. On many routers, the ESP32's power-save
  // mode causes silent drops after a few minutes of otherwise-idle
  // traffic. This keeps the radio fully awake and much more stable.
  WiFi.setSleep(false);

  Serial.begin(9600);
  Serial.println("\nStarting ESP32");

  Wire.begin();

  // Inicio de sensores
  if (!veml.begin()) {
    Serial.println("Sensor not found");
    while (1);
  }

  dht.begin();

  // Ajuste inicial del sensor de luz
  lux_sensor_automatic_adjustment();

  // Asignación de pines
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(wifiConfTrigger, INPUT_PULLUP);


  // Register routes. It's safe to do this before Wi-Fi connects;
  // the server itself is only started once Wi-Fi is up (see loop()).
  server.on("/climatic_variables", HTTP_GET, handleClimaticData);

  // Optional: keep the old endpoint name as an alias.
  server.on("/climatic_variables/body", HTTP_GET, handleClimaticData);

  server.onNotFound(handleNotFound);

  // Try to reconnect using previously stored Wi-Fi credentials.
  WiFi.reconnect();

  Serial.println("HTTP server configured.");
}


void loop() {

  // Take time
  startTime = millis();

  // LED indicates Wi-Fi/server status.
  led_blink();

  // Wi-Fi configuration, triggered by pin 27.
  if (digitalRead(wifiConfTrigger) == LOW) {
    wifi_connect();
  }

  if (WiFi.status() == WL_CONNECTED) {

    // Reset the recovery state machine so the next disconnect starts fresh.
    wifiState = WIFI_TRYING;
    wifiStateStart = 0;

    // Wi-Fi just (re)connected: start the server once, not every loop.
    if (!serverRunning) {
      server.begin();
      serverRunning = true;
      Serial.println("Wi-Fi connected -> HTTP server started.");
      Serial.println("Request climatic data at: http://" + WiFi.localIP().toString() + "/climatic_variables");
    }

    // This is the important change:
    // the ESP32 does NOT make a PUT request every 2 seconds.
    // It waits for/handles incoming HTTP requests from clients.
    server.handleClient();

    // Adjust the light sensor automatically every minute.
    if ((startTime - elapsedTime_lux) >= 60000) {
      lux_sensor_automatic_adjustment();
      elapsedTime_lux = startTime;
    }

    // Read temperature and humidity.
    temperature = dht.readTemperature();
    humidity = dht.readHumidity();

    if (!(isnan(humidity) || isnan(temperature))) {

      avgData[2] += veml.readLux();
      avgData[0] += temperature;
      avgData[1] += humidity;

      avgIndex++;
    }

    // Average 5 valid readings.
    if (avgIndex >= 5) {

      climaticdata[0]["temp"] = std::round((avgData[0] / avgIndex)* 100.0) / 100.0;
      climaticdata[1]["humid"] = std::round((avgData[1] / avgIndex)* 100.0) / 100.0;
      climaticdata[2]["lux"] = std::round((avgData[2] / avgIndex)* 100.0) / 100.0;

      Serial.print("temp(Celsius): ");
      Serial.println(climaticdata[0]["temp"].as<float>());

      Serial.print("humid: ");
      Serial.println(climaticdata[1]["humid"].as<float>());

      Serial.print("lux: ");
      Serial.println(climaticdata[2]["lux"].as<float>());

      avgIndex = 0;
      avgData = {0.0, 0.0, 0.0};
    }

  } else {

    // Wi-Fi just dropped: stop the server so it isn't left running
    // against a dead connection. It will be restarted above once
    // Wi-Fi reconnects.
    if (serverRunning) {
      server.stop();
      serverRunning = false;
      Serial.println("Wi-Fi lost -> HTTP server stopped.");
    }


    if (wifiState==WIFI_TRYING){
      Serial.println("Wi-Fi is not connected");
    }
    

    if (wifiStateStart == 0) {
      wifiStateStart = millis();  // mark when this state began
    }


    switch (wifiState) {

      case WIFI_TRYING:
        // Retry normally every `reconnectInterval` ms.
        if (millis() - lastReconnectAttempt >= reconnectInterval) {
          lastReconnectAttempt = millis();
          Serial.println("Attempting Wi-Fi reconnect...");
          WiFi.disconnect();   // Clear any stuck connection attempt first
          WiFi.reconnect();
        }

        // No luck after 1 minute: stop burning power/airtime and sleep.
        if (millis() - wifiStateStart >= tryDuration) {
          Serial.println("Wi-Fi still down after 1 min -> turning radio off.");
          WiFi.disconnect(true);
          WiFi.mode(WIFI_OFF);
          wifiState = WIFI_SLEEPING;
          wifiStateStart = millis();
        }
        break;

      case WIFI_SLEEPING:
        // Radio is off. Wake it up once every `sleepDuration` to test.
        if (millis() - wifiStateStart >= sleepDuration) {
          Serial.println("Waking Wi-Fi radio for a reconnect attempt...");
          WiFi.mode(WIFI_STA);
          WiFi.setSleep(false);
          WiFi.reconnect();
          wifiState = WIFI_ATTEMPTING;
          wifiStateStart = millis();
        }
        break;

      case WIFI_ATTEMPTING:
        // Give the woken radio a short window to connect.
        if (millis() - wifiStateStart >= attemptDuration) {
          Serial.println("Reconnect attempt failed -> turning radio off again.");
          WiFi.disconnect(true);
          WiFi.mode(WIFI_OFF);
          wifiState = WIFI_SLEEPING;
          wifiStateStart = millis();
        }
        break;
    }

    delay(10);
  }
}



void led_blink() {
  if (WiFi.status() == WL_CONNECTED) {

    if ((startTime - elapsedTime_blink) >= 2900) {
      digitalWrite(LED_BUILTIN, HIGH);
    } else {
      digitalWrite(LED_BUILTIN, LOW);
    }

    if ((startTime - elapsedTime_blink) >= 3000) {
      elapsedTime_blink = startTime;
    }

  } else {

    switch (wifiState){

      case WIFI_TRYING:

        if ((startTime - elapsedTime_blink) >= 100) {
          digitalWrite(LED_BUILTIN, HIGH);
        } else {
          digitalWrite(LED_BUILTIN, LOW);
        }

        if ((startTime - elapsedTime_blink) >= 200) {
          elapsedTime_blink = startTime;
        }

      break;

      case WIFI_SLEEPING:
        digitalWrite(LED_BUILTIN, LOW);

      break;

      case WIFI_ATTEMPTING:
        digitalWrite(LED_BUILTIN, HIGH);

      break;

    }

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
