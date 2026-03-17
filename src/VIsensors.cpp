#include "VIsensors.h"


VIsensors::VIsensors(int Vpin, int Ipin) {
  _Vpin = Vpin;
  _Ipin = Ipin;
}

float VIsensors::readCurrent() {
  int rawValue = analogRead(_Ipin);
  float current = rawValue / 1240.9; // Example conversion factor
  return current;
}

float VIsensors::readVoltage() {
  int rawValue = analogRead(_Vpin);
  float adcVoltage = (rawValue * 3.3) / 4095.0; // Convert ADC value to voltage (ESP32 specific)

  float voltage = adcVoltage / ((7500.0 / (30000.0 + 7500.0))); // Example conversion factor
  return voltage;
}