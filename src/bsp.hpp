//  bsp.hpp

#ifndef BSP_HPP
#define BSP_HPP

#include <Arduino.h>

class BSP {
public:
    // ESP32 FreeRTOS tick rate usually 1000Hz via Arduino
    enum { TICKS_PER_SEC = 1000 }; 
    
    static void init(void);
    static void ledOn(void);
    static void ledOff(void);
    static void ledToggle(void);
};

#endif // BSP_HPP
