#pragma once

#include <Arduino.h>
#include <OneWire.h>


float get_temp(OneWire ds, byte addr[8]) {
    /*
     * I have forgotten where I grabbed this code, I apologise and
     * will add a credit once found
     */

    byte i;
    byte present = 0;
    byte type_s;
    byte data[12];

    float celsius, fahrenheit;

    ds.reset();
    ds.select(addr);
    // start conversion, with parasite power on at the end
    ds.write(0x44, 1);

    // maybe 750ms is enough, maybe not
    delay(1000);
    // we might do a ds.depower() here, but the reset will take care of it.

    present = ds.reset();
    ds.select(addr);
    // Read Scratchpad
    ds.write(0xBE);

    // we need 9 bytes
    for ( i = 0; i < 9; i++) {
        data[i] = ds.read();
    }

    // Convert the data to actual temperature
    // because the result is a 16 bit signed integer, it should
    // be stored to an "int16_t" type, which is always 16 bits
    // even when compiled on a 32 bit processor.
    int16_t raw = (data[1] << 8) | data[0];
    if (type_s) {
        raw = raw << 3; // 9 bit resolution default
        if (data[7] == 0x10) {
            // "count remain" gives full 12 bit resolution
            raw = (raw & 0xFFF0) + 12 - data[6];
        }
    }
    else {
          byte cfg = (data[4] & 0x60);
          // at lower res, the low bits are undefined, so let's zero them
          if (cfg == 0x00) raw = raw & ~7;  // 9 bit resolution, 93.75 ms
          else if (cfg == 0x20) raw = raw & ~3; // 10 bit res, 187.5 ms
          else if (cfg == 0x40) raw = raw & ~1; // 11 bit res, 375 ms
          //// default is 12 bit resolution, 750 ms conversion time
    }
    return (float)raw / 16.0;
}
