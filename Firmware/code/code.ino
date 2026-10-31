#include "RMaker.h"
#include "WiFi.h"
#include <network_provisioning/manager.h>
#include <network_provisioning/scheme_ble.h>

const char *WIFI_SSID = "wifi_ssid";
const char *WIFI_PASS = "wifi_password";

const char *SERVICE_NAME = "PROV_XIAO_SWITCH";
const char *POP_PIN = "12345678";  //  PIN for pairing with ESP RainMaker app

// name of switches how they appear in google home
const char *NAME_CH1 = "Bambu Light";
const char *NAME_CH2 = "Bambu Extruder Fan";

const int PIN_MOSFET_1 = 2;
const int PIN_MOSFET_2 = 3;
const int PIN_LED_WIFI = 5;
const int PIN_SW_1 = 4;
const int PIN_SW_2 = 6;

static Switch *my_switch1 = NULL;
static Switch *my_switch2 = NULL;

bool stateCh1 = false;
bool stateCh2 = false;
int lastSw1State = HIGH;
int lastSw2State = HIGH;

void sysProvEvent(arduino_event_t *sys_event) {
  switch (sys_event->event_id) {
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      digitalWrite(PIN_LED_WIFI, LOW);  
      break;
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      digitalWrite(PIN_LED_WIFI, HIGH); 
      break;
    default:
      break;
  }
}

void write_callback(Device *device, Param *param, const param_val_t val, void *priv_data, write_ctx_t *ctx) {
  const char *device_name = device->getDeviceName();
  const char *param_name = param->getParamName();

  if (strcmp(param_name, ESP_RMAKER_DEF_POWER_NAME) == 0) {
    bool new_state = val.val.b;
    if (strcmp(device_name, NAME_CH1) == 0) {
      stateCh1 = new_state;
      digitalWrite(PIN_MOSFET_1, stateCh1 ? HIGH : LOW);
      param->updateAndReport(val);
    } else if (strcmp(device_name, NAME_CH2) == 0) {
      stateCh2 = new_state;
      digitalWrite(PIN_MOSFET_2, stateCh2 ? HIGH : LOW);
      param->updateAndReport(val);
    }
  }
}

void startBleProvisioning() {
  network_prov_mgr_config_t config = {
    .scheme = network_prov_scheme_ble,
    .scheme_event_handler = NETWORK_PROV_SCHEME_BLE_EVENT_HANDLER_FREE_BLE
  };
  network_prov_mgr_init(config);

  bool provisioned = false;
  network_prov_mgr_is_wifi_provisioned(&provisioned);

  if (!provisioned) {
    network_prov_mgr_start_provisioning(
      NETWORK_PROV_SECURITY_1,
      (const void *)POP_PIN,
      SERVICE_NAME,
      NULL
    );
  } else {
    network_prov_mgr_deinit();
    WiFi.begin();
  }
}

void setup() {
  Serial.begin(115200);

  pinMode(PIN_MOSFET_1, OUTPUT);
  pinMode(PIN_MOSFET_2, OUTPUT);
  pinMode(PIN_LED_WIFI, OUTPUT);
  digitalWrite(PIN_MOSFET_1, LOW);
  digitalWrite(PIN_MOSFET_2, LOW);
  digitalWrite(PIN_LED_WIFI, HIGH);

  pinMode(PIN_SW_1, INPUT_PULLUP);
  pinMode(PIN_SW_2, INPUT_PULLUP);
  lastSw1State = digitalRead(PIN_SW_1);
  lastSw2State = digitalRead(PIN_SW_2);

  Node my_node = RMaker.initNode("XIAO 2CH Switch");

  my_switch1 = new Switch(NAME_CH1, NULL, false);
  my_switch2 = new Switch(NAME_CH2, NULL, false);

  my_switch1->addCb(write_callback);
  my_switch2->addCb(write_callback);

  my_node.addDevice(*my_switch1);
  my_node.addDevice(*my_switch2);

  RMaker.enableOTA(OTA_USING_TOPICS);
  RMaker.enableTZService();
  RMaker.start();

  WiFi.onEvent(sysProvEvent);

  if (strlen(WIFI_SSID) > 0 && strcmp(WIFI_SSID, "wifi_ssid") != 0) {
    WiFi.begin(WIFI_SSID, WIFI_PASS);
  }

  startBleProvisioning();
}

void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    digitalWrite(PIN_LED_WIFI, LOW);
  } else {
    digitalWrite(PIN_LED_WIFI, HIGH);
  }

  int currentSw1 = digitalRead(PIN_SW_1);
  if (currentSw1 != lastSw1State) {
    delay(50);
    if (digitalRead(PIN_SW_1) == currentSw1) {
      lastSw1State = currentSw1;
      stateCh1 = !stateCh1;
      digitalWrite(PIN_MOSFET_1, stateCh1 ? HIGH : LOW);
      my_switch1->updateAndReportParam(ESP_RMAKER_DEF_POWER_NAME, stateCh1);
    }
  }

  int currentSw2 = digitalRead(PIN_SW_2);
  if (currentSw2 != lastSw2State) {
    delay(50);
    if (digitalRead(PIN_SW_2) == currentSw2) {
      lastSw2State = currentSw2;
      stateCh2 = !stateCh2;
      digitalWrite(PIN_MOSFET_2, stateCh2 ? HIGH : LOW);
      my_switch2->updateAndReportParam(ESP_RMAKER_DEF_POWER_NAME, stateCh2);
    }
  }
}