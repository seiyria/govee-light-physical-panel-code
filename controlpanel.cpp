#include "stdio.h"
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "iot_button.h"
#include "WiFi.h"
#include "HTTPClient.h"
#include "cJSON.h"
#include "Adafruit_NeoPixel.h"
#include "Wire.h"

// https://github.com/espressif/esp-idf/blob/master/components/json/README
// https://arduinogetstarted.com/library/button/example/arduino-multiple-button-all

#pragma region Classes
class GoveeDevice {
  public:
    std::string device;
    std::string model;
    std::string deviceName;

    GoveeDevice(std::string device, std::string model, std::string deviceName) : device(device), model(model), deviceName(deviceName) {}
};

class RGB {
  public:
    int r;
    int g;
    int b;

    RGB(int r, int g, int b): r(r), g(g), b(b) {}
};

class RGBA {
  public:
    int r;
    int g;
    int b;
    uint8_t a;

    RGBA(int r, int g, int b, uint8_t a): r(r), g(g), b(b), a(a) {}
};


class Command {
  public:
    char* name;
    RGB color;
    int value;

    Command(char* name, int value, RGB color) : name(name), value(value), color(color) {}
};

class CommandConfig {
  public:
    char* name;
    RGBA buttonColor;
    Command cmd1;
    // Command cmd2;
    // Command cmd3;

    CommandConfig(char* name, RGBA buttonColor, Command cmd1) : name(name), buttonColor(buttonColor), cmd1(cmd1) {}
    // CommandConfig(char* name, Command cmd1, Command cmd2) : name(name), cmd1(cmd1), cmd2(cmd2) {}
    // CommandConfig(char* name, Command cmd1, Command cmd2, Command cmd3) : name(name), cmd1(cmd1), cmd2(cmd2), cmd3(cmd3) {}
};
#pragma endregion

#pragma region Globals
std::vector<GoveeDevice> filteredGoveeDevices;
volatile int lastButtonPressed = -1;

const int LED_PIN = 18;
const int NUM_LEDS = 16;

const int LED_PIN2 = 17;
const int NUM_LEDS2 = 4;

Adafruit_NeoPixel statusKeypad = Adafruit_NeoPixel(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel statusStrip = Adafruit_NeoPixel(NUM_LEDS2, LED_PIN2, NEO_GRB + NEO_KHZ800);

const int numRows = 4; // Example values
const int numCols = 4;

const int rowPins[numRows] = {10, 11, 13, 14}; // Replace with your row pin numbers
const int colPins[numCols] = {35, 36, 37, 38}; // Replace with your column pin numbers

bool buttonMatrix[numRows][numCols] = {0};

const unsigned long interval = 1;  // Time (in milliseconds) to wait between reading channels
int currentChannel = 0;            // The channel currently being read
const int numChannels = 21;
int previousValues[numChannels] = {0};  // Store the previous reading for each channel

const unsigned long buttonScanInterval = 50;  // Time (in milliseconds) to wait between button scans
unsigned long previousButtonScanMillis = 0;   // Stores the last time the button matrix was checked

const unsigned long wifiInterval = 30000;
unsigned long previousWiFiMillis = 0;

#pragma endregion

#pragma region Constants
RGB COLOR_SUCCESS = RGB(0, 255, 0);
RGB COLOR_PENDING = RGB(255, 255, 0);
RGB COLOR_FAILURE = RGB(255, 0, 0);
int LED_POWER = 0;
int LED_WIFI = 1;
int LED_HASDEVICES = 2;
int LED_API = 3;

RGB EMPTY_RGB = RGB(-1, -1, -1);
int EMPTY_VALUE = -1;

const char* apiBaseUrl = "https://developer-api.govee.com/v1";
const char* deviceFilter = "basement";  // "basement" or "shower"
const char* apiKey = "";
const char* ssid = "";
const char* password = ";

CommandConfig commands[16] = {
  CommandConfig("Dim",    RGBA(255, 167, 87, 255),   Command("colorTem",   2700,         EMPTY_RGB)),
  CommandConfig("Low",    RGBA(255, 206, 166, 255),  Command("colorTem",   4000,         EMPTY_RGB)),
  CommandConfig("Medium", RGBA(255, 232, 213, 255),  Command("colorTem",   5200,         EMPTY_RGB)),
  CommandConfig("High",   RGBA(255, 254, 250, 255),  Command("colorTem",   6500,         EMPTY_RGB)),

  CommandConfig("25%",    RGBA(20, 20, 20, 64),      Command("brightness", 25,           EMPTY_RGB)),
  CommandConfig("50%",    RGBA(50, 50, 50, 128),     Command("brightness", 50,           EMPTY_RGB)),
  CommandConfig("75%",    RGBA(100, 100, 100, 192),  Command("brightness", 75,           EMPTY_RGB)),
  CommandConfig("100%",   RGBA(150, 150, 150, 255),  Command("brightness", 100,          EMPTY_RGB)),

  CommandConfig("Red",    RGBA(255, 0, 0, 255),      Command("color",      EMPTY_VALUE,  RGB(255, 0, 0))),
  CommandConfig("Green",  RGBA(0, 255, 0, 255),      Command("color",      EMPTY_VALUE,  RGB(0, 255, 0))),
  CommandConfig("Blue",   RGBA(0, 0, 25, 255),       Command("color",      EMPTY_VALUE,  RGB(0, 0, 255))),
  CommandConfig("White",  RGBA(255, 255, 255, 255),  Command("color",      EMPTY_VALUE,  RGB(255, 255, 255))),

  CommandConfig("Orange", RGBA(255, 165, 0, 255),    Command("color",      EMPTY_VALUE,  RGB(255, 165, 0))),
  CommandConfig("Teal",   RGBA(0, 255, 255, 255),    Command("color",      EMPTY_VALUE,  RGB(0, 255, 255))),
  CommandConfig("Purple", RGBA(128, 0, 128, 255),    Command("color",      EMPTY_VALUE,  RGB(128, 0, 128))),
  CommandConfig("Pink",   RGBA(255, 0, 127, 255),    Command("color",      EMPTY_VALUE,  RGB(255, 192, 203))),
};
#pragma endregion

#pragma region Status Light Functions
void lightPowerShow() {
  statusStrip.setPixelColor(LED_POWER, statusStrip.Color(COLOR_SUCCESS.r, COLOR_SUCCESS.g, COLOR_SUCCESS.b));
  statusStrip.show();
}

void lightWifiConnecting() {
  statusStrip.setPixelColor(LED_WIFI, statusStrip.Color(COLOR_PENDING.r, COLOR_PENDING.g, COLOR_PENDING.b));
  statusStrip.show();
}

void lightWifiBlip() {
  delay(500);
  statusStrip.setPixelColor(LED_WIFI, statusStrip.Color(0, 0, 0));
  statusStrip.show();

  delay(500);
  statusStrip.setPixelColor(LED_WIFI, statusStrip.Color(COLOR_PENDING.r, COLOR_PENDING.g, COLOR_PENDING.b));
  statusStrip.show();
}

void lightWifiConnected() {
  statusStrip.setPixelColor(LED_WIFI, statusStrip.Color(COLOR_SUCCESS.r, COLOR_SUCCESS.g, COLOR_SUCCESS.b));
  statusStrip.show();
}

void lightGettingAPIDevices() {
  statusStrip.setPixelColor(LED_HASDEVICES, statusStrip.Color(COLOR_PENDING.r, COLOR_PENDING.g, COLOR_PENDING.b));
  statusStrip.show();
}

void lightGettingAPIDevicesBlip() {
  delay(500);
  statusStrip.setPixelColor(LED_HASDEVICES, statusStrip.Color(0, 0, 0));
  statusStrip.show();

  delay(500);
  statusStrip.setPixelColor(LED_HASDEVICES, statusStrip.Color(COLOR_PENDING.r, COLOR_PENDING.g, COLOR_PENDING.b));
  statusStrip.show();
}

void lightHasAPIDevices() {
  statusStrip.setPixelColor(LED_HASDEVICES, statusStrip.Color(COLOR_SUCCESS.r, COLOR_SUCCESS.g, COLOR_SUCCESS.b));
  statusStrip.show();
}

void lightHasNoAPIDevices() {
  statusStrip.setPixelColor(LED_HASDEVICES, statusStrip.Color(COLOR_FAILURE.r, COLOR_FAILURE.g, COLOR_FAILURE.b));
  statusStrip.show();
}

void lightAPIRequest() {
  statusStrip.setPixelColor(LED_API, statusStrip.Color(COLOR_PENDING.r, COLOR_PENDING.g, COLOR_PENDING.b));
  statusStrip.show();
}

void lightAPIRequestFailed() {
  statusStrip.setPixelColor(LED_API, statusStrip.Color(COLOR_FAILURE.r, COLOR_FAILURE.g, COLOR_FAILURE.b));
  statusStrip.show();

  delay(2000);
  statusStrip.setPixelColor(LED_API, statusStrip.Color(0, 0, 0));
  statusStrip.show();
}

void lightAPIRequestSucceeded() {
  statusStrip.setPixelColor(LED_API, statusStrip.Color(COLOR_SUCCESS.r, COLOR_SUCCESS.g, COLOR_SUCCESS.b));
  statusStrip.show();

  delay(2000);
  statusStrip.setPixelColor(LED_API, statusStrip.Color(0, 0, 0));
  statusStrip.show();
}
#pragma endregion

uint32_t lightGetStartupColor(int i) {
  int midPoint = NUM_LEDS / 2;

  if (i <= midPoint) {
    // Transition from red to green
    int red = 55 - (55 * i / midPoint);
    int green = 55 * i / midPoint;
    return statusKeypad.Color(red, green, 0);
  } else {
    // Transition from green to blue
    int green = 55 - (55 * (i - midPoint) / midPoint);
    int blue = 55 * (i - midPoint) / midPoint;
    return statusKeypad.Color(0, green, blue);
  }
}

void initLights() {
  for (int i = 0; i < statusKeypad.numPixels(); i++) {
    uint32_t color = lightGetStartupColor(i);
    statusKeypad.setPixelColor(i, color);
    statusKeypad.show();
    delay(50);
    statusKeypad.setPixelColor(i, statusKeypad.Color(0, 0, 0));
  }
  
  statusKeypad.clear();
  statusKeypad.show();

  for (int i = 0; i < statusStrip.numPixels(); i++) {
    uint32_t color = lightGetStartupColor(i);
    statusStrip.setPixelColor(i, color);
    statusStrip.show();
    delay(50);
    statusStrip.setPixelColor(i, statusStrip.Color(0, 0, 0));
  }
  
  statusStrip.clear();
  statusStrip.show();
}

void initKeypad() {
  for (int i = 0; i < numRows; i++) {
    pinMode(rowPins[i], OUTPUT);
    digitalWrite(rowPins[i], HIGH); // Disable all rows initially
  }

  for (int i = 0; i < numCols; i++) {
    pinMode(colPins[i], INPUT_PULLUP); // Use internal pullup
  }
}

void initKeypadLights() {
  for(int i = 0; i < 16; i++) {
    RGBA color = commands[i].buttonColor;
    statusKeypad.setPixelColor(i, statusKeypad.Color(color.r, color.g, color.b));
  }

  statusKeypad.setBrightness(50);
  statusKeypad.show();
}

void initWifi() {
  lightWifiConnecting();

  /*

  WiFi.disconnect();
  WiFi.mode(WIFI_STA);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.println("Connecting to WiFi...");

    WiFi.begin(ssid, password);
    delay(2000);

    Serial.print("Status: ");
    Serial.println(WiFi.status());
    lightWifiBlip();
  }

  Serial.println("Connected to WiFi");
  */
  Serial.println("Connecting to WiFi...");
  
  WiFi.disconnect(true, true);
  WiFi.begin(ssid, password);
  uint8_t wifiAttempts = 0;
  
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(1000);
    
    if(wifiAttempts == 10) {
      WiFi.disconnect(true, true);
      delay(500);
      WiFi.begin(ssid, password);

      wifiAttempts = 0;
    }
    
    wifiAttempts++;
  }

  WiFi.setAutoReconnect(true);

  lightWifiConnected();
}

int getDevices() {
  Serial.println("Getting devices...");

  HTTPClient http;
  http.begin(apiUrl("devices"));
  http.addHeader("Govee-API-Key", apiKey);
  int httpResponseCode = http.GET();
  String response = http.getString();

  if (httpResponseCode > 0) {
    cJSON *parsed = cJSON_Parse(response.c_str());

    cJSON *data = cJSON_GetObjectItem(parsed, "data");
    cJSON *devices = cJSON_GetObjectItem(data, "devices");

    int allDevicesSize = cJSON_GetArraySize(devices);

    for (int i = 0; i < allDevicesSize; i++) {
      cJSON *deviceData = cJSON_GetArrayItem(devices, i);

      char* device = cJSON_GetObjectItem(deviceData, "device")->valuestring;
      char* model = cJSON_GetObjectItem(deviceData, "model")->valuestring;
      char* deviceName = cJSON_GetObjectItem(deviceData, "deviceName")->valuestring;

      if (!strstr(deviceName, deviceFilter)) continue;

      Serial.println("Device found: ");
      Serial.println(deviceName);
      Serial.println(device);
      Serial.println(model);

      GoveeDevice deviceInfo = GoveeDevice(device, model, deviceName);

      filteredGoveeDevices.push_back(deviceInfo);
    }

    Serial.print("Valid device count: ");
    Serial.println(filteredGoveeDevices.size());

    cJSON_Delete(parsed);
  } else {
    Serial.print("Error getting devices: ");
    Serial.println(httpResponseCode);
    Serial.println(response);
  }

  http.end();

  return httpResponseCode;
}

void initDevices() {
  lightGettingAPIDevices();

  int devicesHttp = -1;
  while(devicesHttp < 0) {
    devicesHttp = getDevices();

    if(devicesHttp < 0) {
      lightGettingAPIDevicesBlip();
    }

    delay(2000);
  }
  
  lightHasAPIDevices();
}

void monitorWiFi() {
  unsigned long currentMillis = millis();

  if(WiFi.status() == WL_CONNECTED) return;
  
  if(currentMillis - previousWiFiMillis < wifiInterval) return;
  
  Serial.print(currentMillis);
  Serial.println(" Reconnecting to WiFi...");
  
  WiFi.disconnect();
  WiFi.reconnect();
  
  previousWiFiMillis = currentMillis;
}

void monitorButtons() {
  unsigned long currentMillis = millis();

  // Check if it's time to scan the button matrix
  if (currentMillis - previousButtonScanMillis < buttonScanInterval) return;
  
  previousButtonScanMillis = currentMillis;

  for (int row = 0; row < numRows; row++) {
    digitalWrite(rowPins[row], LOW);

    for (int col = 0; col < numCols; col++) {
      bool currentButtonState = !digitalRead(colPins[col]);

      if (currentButtonState == buttonMatrix[row][col]) continue;
      
      buttonMatrix[row][col] = currentButtonState;

      if(!currentButtonState) continue;

      int ledIndex = row * numCols + col;

      pressSpecificButton(ledIndex);
    }

    digitalWrite(rowPins[row], HIGH);
  }
}

void setup() {
  Serial.begin(115200);
  
  // initLights();

  lightPowerShow();

  // connect to wifi
  initWifi();

  // get the devices for this controller
  initDevices();

  // start up the keypad
  initKeypad();
  initKeypadLights();
}

void loop() {
  monitorWiFi();
  monitorButtons();
}

const char* apiUrl(const char* path) {

  std::string base = apiBaseUrl;
  std::string suffix = path;

  return (base + "/" + suffix).c_str();
}

int controlSpecificDevice(std::string device, std::string model, Command cmd) {
  cJSON* root;
  cJSON* command;

  root = cJSON_CreateObject();
  cJSON_AddStringToObject(root, "device", device.c_str());
  cJSON_AddStringToObject(root, "model", model.c_str());

  command = cJSON_CreateObject();
  cJSON_AddStringToObject(command, "name", cmd.name);

  if (cmd.color.r > -1) {
    cJSON* value = cJSON_CreateObject();
    cJSON_AddNumberToObject(value, "r", cmd.color.r);
    cJSON_AddNumberToObject(value, "g", cmd.color.g);
    cJSON_AddNumberToObject(value, "b", cmd.color.b);

    cJSON_AddItemToObject(command, "value", value);
  }

  if (cmd.value > -1) {
    cJSON_AddNumberToObject(command, "value", cmd.value);
  }

  cJSON_AddItemToObject(root, "cmd", command);

  char* json = cJSON_Print(root);

  lightAPIRequest();

  HTTPClient http;
  http.begin(apiUrl("devices/control"));
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Govee-API-Key", apiKey);
  int httpResponseCode = http.PUT(json);
  String response = http.getString();

  Serial.println(response);

  cJSON_Delete(root);
  http.end();

  return httpResponseCode;
}

void controlManyDevices(Command cmd) {
  for (int i = 0; i < filteredGoveeDevices.size(); i++) {
    auto currentDevice = filteredGoveeDevices.at(i);

    int result = controlSpecificDevice(
      currentDevice.device,
      currentDevice.model,
      cmd
    );

    Serial.println(result);
  }
}

void pressSpecificButton(int button) {
  if (lastButtonPressed == button) return;

  lastButtonPressed = button;
  Serial.print("Button press: ");
  Serial.println(button);

  CommandConfig cmd = commands[button];
  Command cmd1 = cmd.cmd1;

  Serial.print("Command: ");
  Serial.println(cmd.name);

  controlManyDevices(cmd1);
}
