#define US_KEYBOARD 1
#include <Arduino.h>
#include "BLEDevice.h"
#include "BLEHIDDevice.h"
#include "HIDTypes.h"
#include "HIDKeyboardTypes.h"
#include "SSD1306Wire.h"

//SETTINGS
#define BUTTON_PIN 0
#define MESSAGE "Test"
#define DEVICE_NAME "ESP32 Keyboard"
#define DELAY 70
#define DEBOUNCE 50

void bluetoothTask(void*);
void typeText(const char* text);
void initOLED();
bool isBleConnected = false;
int OLED_SCL         = 21;
int OLED_SDA         = 22;
int OLED_VCC         = 32;
int OLED_GND         = 39;
int OLED_width       = 128;
int OLED_height      = 64;
long last            = 0;
String disp          = "";
String dispC         = "";
int count            = 0;
SSD1306Wire  display(0x3c, OLED_SCL, OLED_SDA);
void setup() {
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    initOLED();
    xTaskCreate(bluetoothTask, "bluetooth", 20000, NULL, 5, NULL);
}
void loop() {

    if (isBleConnected && digitalRead(BUTTON_PIN) == LOW) {
        double math = millis() - last;
        delay(DEBOUNCE);
        if (isBleConnected && digitalRead(BUTTON_PIN) == LOW && math > DELAY) {
        typeText(MESSAGE);
        display.clear();
        last = millis();
        count++;
        dispC = "Count: " + String(count);
        if(math < 1000){
          disp = "Delay: " + String(math) + "ms";
        } else {
          math = math / 1000.0;
          disp = "Delay: " + String(math) + "s";
        }
        display.drawString(0, 0, disp);
        display.drawString(0, 8, dispC);
        display.display();
        }
    }
}

struct InputReport {
    uint8_t modifiers;	     // bitmask: CTRL = 1, SHIFT = 2, ALT = 4
    uint8_t reserved;        // must be 0
    uint8_t pressedKeys[6];  // up to six concurrenlty pressed keys
};
struct OutputReport {
    uint8_t leds;
};
static const uint8_t REPORT_MAP[] = {
    USAGE_PAGE(1),      0x01,       // Generic Desktop Controls
    USAGE(1),           0x06,       // Keyboard
    COLLECTION(1),      0x01,       // Application
    REPORT_ID(1),       0x01,       //   Report ID (1)
    USAGE_PAGE(1),      0x07,       //   Keyboard/Keypad
    USAGE_MINIMUM(1),   0xE0,       //   Keyboard Left Control
    USAGE_MAXIMUM(1),   0xE7,       //   Keyboard Right Control
    LOGICAL_MINIMUM(1), 0x00,       //   Each bit is either 0 or 1
    LOGICAL_MAXIMUM(1), 0x01,
    REPORT_COUNT(1),    0x08,       //   8 bits for the modifier keys
    REPORT_SIZE(1),     0x01,
    HIDINPUT(1),        0x02,       //   Data, Var, Abs
    REPORT_COUNT(1),    0x01,       //   1 byte (unused)
    REPORT_SIZE(1),     0x08,
    HIDINPUT(1),        0x01,       //   Const, Array, Abs
    REPORT_COUNT(1),    0x06,       //   6 bytes (for up to 6 concurrently pressed keys)
    REPORT_SIZE(1),     0x08,
    LOGICAL_MINIMUM(1), 0x00,
    LOGICAL_MAXIMUM(1), 0x65,       //   101 keys
    USAGE_MINIMUM(1),   0x00,
    USAGE_MAXIMUM(1),   0x65,
    HIDINPUT(1),        0x00,       //   Data, Array, Abs
    REPORT_COUNT(1),    0x05,       //   5 bits (Num lock, Caps lock, Scroll lock, Compose, Kana)
    REPORT_SIZE(1),     0x01,
    USAGE_PAGE(1),      0x08,       //   LEDs
    USAGE_MINIMUM(1),   0x01,       //   Num Lock
    USAGE_MAXIMUM(1),   0x05,       //   Kana
    LOGICAL_MINIMUM(1), 0x00,
    LOGICAL_MAXIMUM(1), 0x01,
    HIDOUTPUT(1),       0x02,       //   Data, Var, Abs
    REPORT_COUNT(1),    0x01,       //   3 bits (Padding)
    REPORT_SIZE(1),     0x03,
    HIDOUTPUT(1),       0x01,       //   Const, Array, Abs
    END_COLLECTION(0)               // End application collection
};
BLEHIDDevice* hid;
BLECharacteristic* input;
BLECharacteristic* output;
const InputReport NO_KEY_PRESSED = { };
class BleKeyboardCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer* server) {
        isBleConnected = true;
        BLE2902* cccDesc = (BLE2902*)input->getDescriptorByUUID(BLEUUID((uint16_t)0x2902));
        cccDesc->setNotifications(true);
    }
    void onDisconnect(BLEServer* server) {
        isBleConnected = false;
        BLE2902* cccDesc = (BLE2902*)input->getDescriptorByUUID(BLEUUID((uint16_t)0x2902));
        cccDesc->setNotifications(false);
    }
};
 class OutputCallbacks : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic* characteristic) {
    }
};
  void bluetoothTask(void*) {
    BLEDevice::init(DEVICE_NAME);
    BLEServer* server = BLEDevice::createServer();
    server->setCallbacks(new BleKeyboardCallbacks());
    hid = new BLEHIDDevice(server);
    input = hid->inputReport(1); // report ID
    output = hid->outputReport(1); // report ID
    output->setCallbacks(new OutputCallbacks());
    hid->manufacturer()->setValue("Maker Community");
    hid->pnp(0x02, 0xe502, 0xa111, 0x0210);
    hid->hidInfo(0x00, 0x02);
    BLESecurity* security = new BLESecurity();
    security->setAuthenticationMode(ESP_LE_AUTH_BOND);
    hid->reportMap((uint8_t*)REPORT_MAP, sizeof(REPORT_MAP));
    hid->startServices();
    hid->setBatteryLevel(100);
    BLEAdvertising* advertising = server->getAdvertising();
    advertising->setAppearance(HID_KEYBOARD);
    advertising->addServiceUUID(hid->hidService()->getUUID());
    advertising->addServiceUUID(hid->deviceInfo()->getUUID());
    advertising->addServiceUUID(hid->batteryService()->getUUID());
    advertising->start();
    delay(portMAX_DELAY);
};
void typeText(const char* text) {
    int len = strlen(text);
    for (int i = 0; i < len; i++) {
        uint8_t val = (uint8_t)text[i];
        if (val > KEYMAP_SIZE)
            continue;
        KEYMAP map = keymap[val];
        InputReport report = {
            .modifiers = map.modifier,
            .reserved = 0,
            .pressedKeys = {
                map.usage,
                0, 0, 0, 0, 0
            }
        };
        input->setValue((uint8_t*)&report, sizeof(report));
        input->notify();
        delay(5);
        input->setValue((uint8_t*)&NO_KEY_PRESSED, sizeof(NO_KEY_PRESSED));
        input->notify();
        delay(5);
    }
}
void initOLED() {
  pinMode(OLED_VCC, OUTPUT);
  pinMode(OLED_GND, OUTPUT);
  digitalWrite(OLED_VCC, HIGH);
  digitalWrite(OLED_GND, LOW);
  delay(100);
  display.init();
  display.flipScreenVertically();
  display.setFont(ArialMT_Plain_10);
}
