#ifndef DisplayPrint_h
#define DisplayPrint_h
#include <Arduino.h>
#include <LiquidCrystal_I2C.h>
#include <WiFi.h>

extern LiquidCrystal_I2C lcd; // Set the LCD I2C address and dimensions
void initializeDisplay();
void initDisplayLayout();
extern bool lcdInitialized;

extern IPAddress local_IP;

void printInitialDisplay(IPAddress local_IP, const char* ssid);

void printToDisplay(float VinVoltage, float InCurrent, float powerIn, 
    float Vbat, bool isCharging, int batteryLevel, 
    float loadPercentage, bool isLoadActive, bool systemStatus);

void errorAndRestartDisplay(const char *errMsg);

#endif