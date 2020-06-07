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
#include <RadDevice.h>
#include <RadHelpers.h>
#include <RadWiFi.h>
#include <RadMQTT.h>
#include <RadLED.h>
#include <RadHeartbeat.h>
#include "helpers.hpp"


RadConfig config = RadConfig();

RadWiFi wifi(config);

RadMQTT mqtt(config);

RadHeartbeat heartbeat(config);

DHT dht(D7, DHT22);

OneWire ds(D2);

#define FLASH_ERROR_ON 1000
#define FLASH_ERROR_OFF 250

#define FLASH_SUCCESS_ON 250
#define FLASH_SUCCESS_OFF 250

// Last time the temperature was checked and sent
unsigned long last_temperature_check = 0;
unsigned long temperate_check_interval_ms = 300000;


void setup(void) {
    config.debug_on();

    Serial.begin(115200);
    delay(2500);
    Serial.printf("===[ Welcome to RadPool ]===\n");
    Serial.printf("[INFO] Initializing %s\n", config.device_id.c_str());

    wifi.connect();
}


void loop(void) {
    wifi.loop();
    mqtt.loop();
    heartbeat.loop();

    if (millis() - last_temperature_check > temperate_check_interval_ms) {
        last_temperature_check = millis();

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

        char request_data[120];
        sprintf(request_data, "{\"device_id\": \"%s\", \"temp\": %s, \"humidity\": %s, \"top\": %s, \"middle\": %s, \"bottom\": %s}", config.device_id.c_str(), s_ambient_temp.c_str(), s_ambient_humidity.c_str(), s_top_temp.c_str(), s_middle_temp.c_str(), s_bottom_temp.c_str());
        Serial.println("[MQTT] publishing to /pool: " + String(request_data));

        config.debug("MQTT Publish /pool");
        config.debug(request_data);
        bool result = mqtt.client->publish("/pool", request_data);

        config.debug("[MQTT] /pool response: " + String(result));

        if (result) {
            config.led.flash(3, FLASH_SUCCESS_ON, FLASH_SUCCESS_OFF);
        }
        else {
            config.led.flash(3, FLASH_ERROR_ON, FLASH_ERROR_OFF);
        }
    }
}
