#ifndef VIsensors_h
#define VIsensors_h
#include <Arduino.h>


class VIsensors {
  public:
    VIsensors(int Vpin, int Ipin);
    float readCurrent();
    float readVoltage();
  private:
    int _Vpin;
    int _Ipin;
};

#endif