/// BASIC CONFIGURATION 

#define ENABLE_WIFI_MANAGER  // if we want to have built-in wifi configuration
                             // Otherwise direct connect ssid and pwd will be used
                             // for Wifi manager need extra library //https://github.com/tzapu/WiFiManager

#define ENABLE_WEB_SERVER    // if we want to have built-in web server / site
#define ENABLE_OTA           // if Over the air update is needed, ENABLE_WEB_SERVER must be defined first
#include <Arduino.h>


#ifdef ESP32
#include <SPIFFS.h>
#endif



#ifdef ESP32
#include <WebServer.h>
WebServer server(80);
#endif

#if defined(ESP32) && defined(ENABLE_OTA)
#include <Update.h>
#endif

#ifdef ENABLE_WEB_SERVER
#include "spiffs_webserver.h"
bool isWebserver_started = false;
#endif

const int relay_gpio1 = 13;
const int relay_gpio2 = 12;
const int relay_gpio3 = 14;
const int relay_gpio4 = 27;

#ifdef ENABLE_WIFI_MANAGER
#include <WiFiManager.h> 
#endif

const char* HOSTNAME = "ES";
const char* ssid     = "TanGo";
const char* password = "Tan292800246080";

extern "C" {
#include "homeintegration.h"
}

homekit_service_t* hapservice1 = {0};
homekit_service_t* hapservice2 = {0};
homekit_service_t* hapservice3 = {0};
homekit_service_t* hapservice4 = {0};

String pair_file_name = "/pair.dat";

bool getSwitchVal(); // Function prototype

void setup() {
#ifdef ESP8266 
  disable_extra4k_at_link_time();
#endif 
  Serial.begin(115200);
  delay(10);

#ifdef ESP32
  if (!SPIFFS.begin(true)) {
    // Serial.print("SPIFFS Mount failed");
  }
#endif

  pinMode(relay_gpio1, OUTPUT);
  pinMode(relay_gpio2, OUTPUT);
  pinMode(relay_gpio3, OUTPUT);
  pinMode(relay_gpio4, OUTPUT);

  init_hap_storage();

  set_callback_storage_change(storage_changed);

  hap_setbase_accessorytype(homekit_accessory_category_switch);
  hap_initbase_accessory_service("host", "Tan", "0", "EspHapSwitch", "1.0");

  hapservice1 = hap_add_switch_service("Switch 1", switch_callback, (void*)&relay_gpio1);
  hapservice2 = hap_add_switch_service("Switch 2", switch_callback, (void*)&relay_gpio2);
  hapservice3 = hap_add_switch_service("Switch 3", switch_callback, (void*)&relay_gpio3);
  hapservice4 = hap_add_switch_service("Switch 4", switch_callback, (void*)&relay_gpio4);

#ifdef ENABLE_WIFI_MANAGER   
  startwifimanager();
#else

  WiFi.mode(WIFI_STA);
  WiFi.begin((char*)ssid, (char*)password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

#endif
  Serial.println(PSTR("WiFi connected"));
  Serial.println(PSTR("IP address: "));
  Serial.println(WiFi.localIP());

  hap_init_homekit_server();   

#ifdef ENABLE_WEB_SERVER
  String strIp = String(WiFi.localIP()[0]) + String(".") + String(WiFi.localIP()[1]) + String(".") +  String(WiFi.localIP()[2]) + String(".") +  String(WiFi.localIP()[3]);
#ifdef ESP8266
  if (hap_homekit_is_paired()) {
#endif
    Serial.println(PSTR("Setting web server"));
    SETUP_FILEHANDLES
    server.on("/get", handleGetVal);
    server.on("/set", handleSetVal);
    server.begin();
    Serial.println(String("Web site http://") + strIp);
    Serial.println(String("File system http://") + strIp + String("/browse"));
    Serial.println(String("Update http://") + strIp + String("/update"));
    isWebserver_started = true;
#ifdef ESP8266
  } else {
    Serial.println(PSTR("Web server is NOT SET, waiting for pairing"));
  }
#endif

#endif
}

void handleGetVal() {
  server.send(200, FPSTR(TEXT_PLAIN), getSwitchVal() ? "1" : "0");
}

void handleSetVal() {
  if (server.args() != 2) {
    server.send(505, FPSTR(TEXT_PLAIN), "Bad args");
    return;
  }

  if (server.arg("var") == "ch1") {
    if (hapservice1) {
      homekit_characteristic_t* ch = homekit_service_characteristic_by_type(hapservice1, HOMEKIT_CHARACTERISTIC_ON);
      if (ch) {
        set_switch(relay_gpio1, server.arg("val") == "true");
      }
    }
  } else if (server.arg("var") == "ch2") {
    if (hapservice2) {
      homekit_characteristic_t* ch = homekit_service_characteristic_by_type(hapservice2, HOMEKIT_CHARACTERISTIC_ON);
      if (ch) {
        set_switch(relay_gpio2, server.arg("val") == "true");
      }
    }
  } else if (server.arg("var") == "ch3") {
    if (hapservice3) {
      homekit_characteristic_t* ch = homekit_service_characteristic_by_type(hapservice3, HOMEKIT_CHARACTERISTIC_ON);
      if (ch) {
        set_switch(relay_gpio3, server.arg("val") == "true");
      }
    }
  }
  else if (server.arg("var") == "ch4") {
    if (hapservice4) {
      homekit_characteristic_t* ch = homekit_service_characteristic_by_type(hapservice4, HOMEKIT_CHARACTERISTIC_ON);
      if (ch) {
        set_switch(relay_gpio4, server.arg("val") == "true");
      }
    }
  }
}

void loop() {
#ifdef ESP8266
  hap_homekit_loop();
#endif

  if (isWebserver_started)
    server.handleClient();
}

void init_hap_storage() {
  Serial.print("init_hap_storage");

  File fsDAT = SPIFFS.open(pair_file_name, "r");

  if (!fsDAT) {
    Serial.println("Failed to read pair.dat");
    SPIFFS.format();
  }

  int size = hap_get_storage_size_ex();
  char* buf = new char[size];
  memset(buf, 0xff, size);

  if (fsDAT)
    fsDAT.readBytes(buf, size);

  hap_init_storage_ex(buf, size);

  if (fsDAT)
    fsDAT.close();

  delete[] buf;
}

void storage_changed(char* szstorage, int bufsize) {
  SPIFFS.remove(pair_file_name);
  File fsDAT = SPIFFS.open(pair_file_name, "w+");

  if (!fsDAT) {
    Serial.println("Failed to open pair.dat");
    return;
  }

  fsDAT.write((uint8_t*)szstorage, bufsize);
  fsDAT.close();
}

void set_switch(int relay_gpio, bool val) {
  Serial.println(String("set_switch: ") + String(relay_gpio) + String(", ") + String(val ? "True" : "False"));
  digitalWrite(relay_gpio, val ? HIGH : LOW);

  if (hapservice1 && relay_gpio == relay_gpio1) {
    homekit_characteristic_t* ch = homekit_service_characteristic_by_type(hapservice1, HOMEKIT_CHARACTERISTIC_ON);

    if (ch && ch->value.bool_value != val) {
      ch->value.bool_value = val;
      homekit_characteristic_notify(ch, ch->value);
    }
  } else if (hapservice2 && relay_gpio == relay_gpio2) {
    homekit_characteristic_t* ch = homekit_service_characteristic_by_type(hapservice2, HOMEKIT_CHARACTERISTIC_ON);

    if (ch && ch->value.bool_value != val) {
      ch->value.bool_value = val;
      homekit_characteristic_notify(ch, ch->value);
    }
  } else if (hapservice3 && relay_gpio == relay_gpio3) {
    homekit_characteristic_t* ch = homekit_service_characteristic_by_type(hapservice3, HOMEKIT_CHARACTERISTIC_ON);

    if (ch && ch->value.bool_value != val) {
      ch->value.bool_value = val;
      homekit_characteristic_notify(ch, ch->value);
    }
  }
  else if (hapservice4 && relay_gpio == relay_gpio4) {
    homekit_characteristic_t* ch = homekit_service_characteristic_by_type(hapservice4, HOMEKIT_CHARACTERISTIC_ON);

    if (ch && ch->value.bool_value != val) {
      ch->value.bool_value = val;
      homekit_characteristic_notify(ch, ch->value);
    }
  }
}

void switch_callback(homekit_characteristic_t* ch, homekit_value_t value, void* context) {
  Serial.println("switch_callback");
  int relay_gpio = *(int*)context;
  set_switch(relay_gpio, ch->value.bool_value);
}

#ifdef ENABLE_WIFI_MANAGER
void startwifimanager() {
  WiFiManager wifiManager;

  if (!wifiManager.autoConnect(HOSTNAME, NULL)) {
    ESP.restart();
    delay(1000);
  }
}
#endif

bool getSwitchVal() {
  if (hapservice1) {
    homekit_characteristic_t* ch = homekit_service_characteristic_by_type(hapservice1, HOMEKIT_CHARACTERISTIC_ON);
    if (ch) {
      return ch->value.bool_value;
    }
  }
  return false;
}
