#include "Controls.h"
Controls::Controls(int relayPin, int loadPin) {
  _relayPin = relayPin;
  _loadPin = loadPin;
}
void Controls::activateLoad() {
  digitalWrite(_loadPin, HIGH);
}
void Controls::deactivateLoad() {
  digitalWrite(_loadPin, LOW);
}
void Controls::setRelayActive() {
  digitalWrite(_relayPin, LOW);
}
void Controls::setRelayInactive() {
  digitalWrite(_relayPin, HIGH);
}
void Controls::initializePins() {
  pinMode(_relayPin, OUTPUT);
  pinMode(_loadPin, OUTPUT);
}