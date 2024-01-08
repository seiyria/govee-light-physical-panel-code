#include <stdio.h>

#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "iot_button.h"
#include "WiFi.h"
#include "HTTPClient.h"
#include "cJSON.h"
#include "ezButton.h"

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
    Command cmd1;
    // Command cmd2;
    // Command cmd3;
    
    CommandConfig(char* name, Command cmd1) : name(name), cmd1(cmd1) {}
    // CommandConfig(char* name, Command cmd1, Command cmd2) : name(name), cmd1(cmd1), cmd2(cmd2) {}
    // CommandConfig(char* name, Command cmd1, Command cmd2, Command cmd3) : name(name), cmd1(cmd1), cmd2(cmd2), cmd3(cmd3) {}
};
#pragma endregion

#pragma region Globals
std::vector<GoveeDevice> filteredGoveeDevices;
volatile int lastButtonPressed = -1;
#pragma endregion

#pragma region Constants
RGB EMPTY_RGB = RGB(-1, -1, -1);
int EMPTY_VALUE = -1;

const char* apiBaseUrl = "https://developer-api.govee.com/v1";
const char* deviceFilter = "basement";  // "basement" or "shower"
const char* apiKey = "";
const char* ssid = ""; 
const char* password = ""; 

CommandConfig commands[16] = {
  CommandConfig("Dim",    Command("colorTem",   2700,         EMPTY_RGB)),
  CommandConfig("Low",    Command("colorTem",   4000,         EMPTY_RGB)),
  CommandConfig("Medium", Command("colorTem",   5200,         EMPTY_RGB)),
  CommandConfig("High",   Command("colorTem",   6500,         EMPTY_RGB)),
  
  CommandConfig("25%",    Command("brightness", 25,           EMPTY_RGB)),
  CommandConfig("50%",    Command("brightness", 50,           EMPTY_RGB)),
  CommandConfig("75%",    Command("brightness", 75,           EMPTY_RGB)),
  CommandConfig("100%",   Command("brightness", 100,          EMPTY_RGB)),
  
  CommandConfig("Red",    Command("color",      EMPTY_VALUE,  RGB(255, 0, 0))),
  CommandConfig("Green",  Command("color",      EMPTY_VALUE,  RGB(0, 255, 0))),
  CommandConfig("Blue",   Command("color",      EMPTY_VALUE,  RGB(0, 0, 255))),
  CommandConfig("White",  Command("color",      EMPTY_VALUE,  RGB(255, 255, 255))),
  
  CommandConfig("Orange", Command("color",      EMPTY_VALUE,  RGB(255, 165, 0))),
  CommandConfig("Teal",   Command("color",      EMPTY_VALUE,  RGB(0, 255, 255))),
  CommandConfig("Purple", Command("color",      EMPTY_VALUE,  RGB(128, 0, 128))),
  CommandConfig("Pink",   Command("color",      EMPTY_VALUE,  RGB(255, 192, 203))),
};

const int allGPIOs[] = {
  7,  8,  9,  10, // Red
  11, 21, 14, 16, // Green
  17, 18, 35, 36, // Blue
  37, 38, 47, 48  // White
};

ezButton allButtons[] = {
  ezButton(allGPIOs[0]),
  ezButton(allGPIOs[1]),
  ezButton(allGPIOs[2]),
  ezButton(allGPIOs[3]),
  ezButton(allGPIOs[4]),
  ezButton(allGPIOs[5]),
  ezButton(allGPIOs[6]),
  ezButton(allGPIOs[7]),
  ezButton(allGPIOs[8]),
  ezButton(allGPIOs[9]),
  ezButton(allGPIOs[10]),
  ezButton(allGPIOs[11]),
  ezButton(allGPIOs[12]),
  ezButton(allGPIOs[13]),
  ezButton(allGPIOs[14]),
  ezButton(allGPIOs[15]),
};
#pragma endregion

void setup() {
  Serial.begin(115200);

  // initialize all buttons
  int gpio;
  for(gpio = 0; gpio < 16; gpio++) {
    pinMode(gpio, INPUT_PULLUP);
    allButtons[gpio].setDebounceTime(100);
  }

  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  
  Serial.println("Connected to WiFi");

  getDevices();
}

void loop() {
  int gpio;
  for(gpio = 0; gpio < 16; gpio++) {
    allButtons[gpio].loop();
    
    if(allButtons[gpio].isPressed()) {
      pressSpecificButton(gpio);
    }
  }
}

const char* apiUrl(const char* path) {
  
  std::string base = apiBaseUrl;
  std::string suffix = path;

  return (base + "/" + suffix).c_str();
}

void getDevices() {

  Serial.println("Getting devices...");
  
  HTTPClient http;
  http.begin(apiUrl("devices"));
  http.addHeader("Govee-API-Key", apiKey);
  int httpResponseCode = http.GET();
  
  if (httpResponseCode > 0) {
      String response = http.getString();
      cJSON *parsed = cJSON_Parse(response.c_str());

      cJSON *data = cJSON_GetObjectItem(parsed, "data");
      cJSON *devices = cJSON_GetObjectItem(data, "devices");

      int allDevicesSize = cJSON_GetArraySize(devices);
          
      for(int i = 0; i < allDevicesSize; i++) {
        cJSON *deviceData = cJSON_GetArrayItem(devices, i);

        char* device = cJSON_GetObjectItem(deviceData, "device")->valuestring;
        char* model = cJSON_GetObjectItem(deviceData, "model")->valuestring;
        char* deviceName = cJSON_GetObjectItem(deviceData, "deviceName")->valuestring;
        
        if(!strstr(deviceName, deviceFilter)) continue;

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
  }

  http.end();
}

void controlSpecificDevice(std::string device, std::string model, Command cmd) {
  cJSON* root;
  cJSON* command;

  root = cJSON_CreateObject();
  cJSON_AddStringToObject(root, "device", device.c_str());
  cJSON_AddStringToObject(root, "model", model.c_str());

  command = cJSON_CreateObject();
  cJSON_AddStringToObject(command, "name", cmd.name);
  
  if(cmd.color.r > -1) {
    cJSON* value = cJSON_CreateObject();
    cJSON_AddNumberToObject(value, "r", cmd.color.r);
    cJSON_AddNumberToObject(value, "g", cmd.color.g);
    cJSON_AddNumberToObject(value, "b", cmd.color.b);

    cJSON_AddItemToObject(command, "value", value);
  }

  if(cmd.value > -1) {
    cJSON_AddNumberToObject(command, "value", cmd.value);
  }

  cJSON_AddItemToObject(root, "cmd", command);

  char* json = cJSON_Print(root);
  
  HTTPClient http;
  // http.begin(url);
  http.begin(apiUrl("devices/control"));
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Govee-API-Key", apiKey);
  int httpResponseCode = http.PUT(json);
  
  if (httpResponseCode > 0) {
      // TODO: status LED
  } else {
      // TODO: status LED
  }

  cJSON_Delete(root);
  http.end();
}

void controlManyDevices(Command cmd) {
  for(int i = 0; i < filteredGoveeDevices.size(); i++) {
    auto currentDevice = filteredGoveeDevices.at(i);
    
    controlSpecificDevice(
      currentDevice.device,
      currentDevice.model,
      cmd
    );
  }
}

void pressSpecificButton(int button) {
  if(lastButtonPressed == button) return;
  
  lastButtonPressed = button;
  Serial.print("Button press: ");
  Serial.println(button);

  CommandConfig cmd = commands[button];
  Command cmd1 = cmd.cmd1;

  Serial.print("Command: ");
  Serial.println(cmd.name);

  controlManyDevices(cmd1);
}
