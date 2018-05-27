/**
 * Pool temperature sensors. (x3)
 * + ambient temp and humidity
 *
 * Data is polled from the devices and sent to a local MQTT server
 * where it is also saved to the database for historical purposes.
 *
 * LED messages:
 * constant fast = wifi connecting
 *
 * 1 long flash = Non 200 error code from thingspeak
 * 2 long flash = Other error connecting to thingspeak
 * 3 long flash = MQTT pool temps error
 * 4 long flash = MQTT temps error
 *
 * 1 short = loop start
 * 2 short flashes = Successful connection to thingspeak
 * 3 short flashes = MQTT pool temps success
 * 4 short flashes = MQTT temps success
 *
 * NOTES:
 * WHITE -> D2
 * RED -> VV (5V)
 * BLACK -> G
 *
 * 5k resistor between WHITE/RED
 *
 * ROM = 28 FF 7F 85 2B 4 0 7C
 * 0x28, 0xFF, 0x7F, 0x85, 0x2B, 0x04, 0x00, 0x7C
 *   Chip = DS18B20
 *
 * ROM = 28 FF 9A 9A 2D 4 0 7
 * 0x28, 0xFF, 0x9A, 0x9A, 0x2D, 0x04, 0x00, 0x07
 *   Chip = DS18B20
 *
 * ROM = 28 FF 95 58 2E 4 0 B2
 * 0x28, 0xFF, 0x95, 0x58, 0x2E, 0x04, 0x00, 0xB2
 *   Chip = DS18B20
 */

#include <Arduino.h>
#include <OneWire.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <RadConfig.h>
#include <RadLED.h>
#include <RadWifi.h>
#include <RadDevice.h>
#include <RadHelpers.h>
#include "helpers.hpp"


RadConfig config = RadConfig();

DHT dht(D1, DHT22);

OneWire ds(D2);

String device_id = "";

WiFiClient wclient;
PubSubClient client(config.mqtt_server, config.mqtt_port, wclient);

#define LED_BUILTIN 2
#define FLASH_ERROR_ON 1000
#define FLASH_ERROR_OFF 250

#define FLASH_SUCCESS_ON 250
#define FLASH_SUCCESS_OFF 250


void setup(void) {
    pinMode(LED_BUILTIN, OUTPUT);
    // Make sure it's turned off on init. (High is low on the internal LED)
    digitalWrite(LED_BUILTIN, HIGH);

    Serial.begin(115200);

    Serial.println();
    Serial.printf("===[ Welcome to RadPool ]===\n");
    device_id = get_device_id();
    Serial.printf("[INFO] Initializing %s\n", device_id.c_str());

    setup_wifi(device_id, config);
}

void loop(void) {
    LED_flash(LED_BUILTIN, 1, FLASH_SUCCESS_ON, 1000);
    Serial.println("--------------------------");
    if (!client.connected()) {
        mqtt_reconnect(client, device_id, config);
    }
    client.loop();

    // DS18B20 sensors
    byte top_sensor[]    = {0x28, 0xFF, 0x9A, 0x9A, 0x2D, 0x04, 0x00, 0x07};
    byte middle_sensor[] = {0x28, 0xFF, 0x7F, 0x85, 0x2B, 0x04, 0x00, 0x7C};
    byte bottom_sensor[] = {0x28, 0xFF, 0x95, 0x58, 0x2E, 0x04, 0x00, 0xB2};

    float top_temp = get_temp(ds, top_sensor);
    float middle_temp = get_temp(ds, middle_sensor);
    float bottom_temp = get_temp(ds, bottom_sensor);

    String s_top_temp = float_to_string(top_temp, 2);
    String s_middle_temp = float_to_string(middle_temp, 2);
    String s_bottom_temp = float_to_string(bottom_temp, 2);

    // DHT Ambient values.
    float f_temp = dht.readTemperature();
    float f_humidity = dht.readHumidity();
    String s_ambient_temp = float_to_string(f_temp, 2);
    String s_ambient_humidity = float_to_string(f_humidity, 2);

    Serial.println();
    Serial.println("[INFO] (07)    Top: " + String(s_top_temp));
    Serial.println("[INFO] (7C) Middle: " + String(s_middle_temp));
    Serial.println("[INFO] (B2) Bottom: " + String(s_bottom_temp));
    Serial.println("[INFO]   Ambient C: " + s_ambient_temp);
    Serial.println("[INFO]   Ambient \%: " + s_ambient_humidity);

    //
    // MQTT
    //

    char buffer[120];
    sprintf(buffer, "{\"device_id\": \"%s\", \"temp\": %s, \"humidity\": %s, \"top\": %s, \"middle\": %s, \"bottom\": %s}", device_id.c_str(), s_ambient_temp.c_str(), s_ambient_humidity.c_str(), s_top_temp.c_str(), s_middle_temp.c_str(), s_bottom_temp.c_str());
    Serial.println("[MQTT] publishing to /pool: " + String(buffer));
    bool result = client.publish("/pool", buffer);
    Serial.println("[MQTT] /pool response: " + String(result));

    if (result) {
        LED_flash(LED_BUILTIN, 3, FLASH_SUCCESS_ON, FLASH_SUCCESS_OFF);
    }
    else {
        LED_flash(LED_BUILTIN, 3, FLASH_ERROR_ON, FLASH_ERROR_OFF);
    }

    delay(300000);
}
