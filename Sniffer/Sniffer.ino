#include "GPIO.h" //https://github.com/mikaelpatel/Arduino-GPIO
#include <stdint.h>

#include "printf.h"

GPIO<BOARD::D39> PIN_HR;
GPIO<BOARD::D41> PIN_HRBCK;
GPIO<BOARD::D43> PIN_CS;

char inputBuffer[128];
#define STORE_LEN 0xE000
volatile uint8_t storage[STORE_LEN];

void setup() {
    PIN_HRBCK.input();
    PIN_HRBCK.pullup();
    PIN_HR.input();
    PIN_HR.pullup();
    
    storage[STORE_LEN - 1] = 20; //Trick Arduino into compiling properly
    
    //serial
    printf_init();
    Serial.begin(115200);
    xprintf("Hello world!\n");
}

void clockAndSniff() {
    bool previous_HRBCK = 0;
    
    uint8_t bitindex = 0;
    uint32_t byteindex = 0;
    
    //Loop until input
    while (!Serial.available()) {
        bool current_HRBCK = PIN_HRBCK.read();
        
        //Check for clock on HRBCK
        if ((current_HRBCK == 1) && (previous_HRBCK == 0)) {
            //Capture bit
            bool input = PIN_HR.read();
			//Store bit
			storage[byteindex] = (input << 7) | (storage[byteindex] >> 1);
            
            bitindex++;
            if (bitindex == 8) {
                bitindex = 0;
                byteindex++;
            }
        }
        if (byteindex < STORE_LEN) {
            xprintf("Maxed out buffer!\n");
        }
        
        previous_HRBCK = current_HRBCK;
    }
    Serial.read();
    
    //Print the accumulated data
    xprintf("Data: ");
	for (int i = 0; i < byteindex; i++) {
        xprintf("%02X ", storage[i]);
    }
    xprintf("\n");
}

void loop() {
    xprintf("Enter command s to start sniffing...\n");
    while (!Serial.available());
    int length = Serial.readBytesUntil('\n', inputBuffer, sizeof(inputBuffer));
    if (length == 0) {
        xprintf("\n");
        return;
    }
    
    char cmd = inputBuffer[0];
    
    switch (cmd) {
    case 's':
        //s for sniff
        xprintf("Sniffing\n");
        clockAndSniff();
        break;
    default:
        xprintf("Unrecognised command %c\n", cmd);
        break;
    }
}