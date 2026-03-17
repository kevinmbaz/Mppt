#ifndef Controls_h
#define Controls_h
#include <Arduino.h>

class Controls {
  public:
    Controls(int relayPin, int loadPin);
    void activateLoad();
    void deactivateLoad();
    void setRelayActive();
    void setRelayInactive();
    void initializePins();
  private:
    int _relayPin;
    int _loadPin;
};

#endif