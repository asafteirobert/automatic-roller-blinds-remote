#ifndef _BLINDSDATA_h
#define _BLINDSDATA_h
#include "arduino.h"

const static uint16_t BLINDS_DATA_VERSION_CHECK = 55865; //change to rewrite settings

struct BlindData
{
  uint8_t blindId = 0;
  int8_t currentTargetPercent = -1;
};

class BlindsData
{
public:
  void resetToDefault();
  void loadFromEEPROM();
  void saveToEEPROM();
  int8_t getAveragePercent();
  int8_t getSelectedBlindPercent();
  void savePreset(uint8_t presetToSave);
  void loadPreset(uint8_t presetToLoad);

  const static uint8_t MAX_BLIND_COUNT = 20;

  uint16_t settingsVersion = 0;

  BlindData blinds[MAX_BLIND_COUNT];
  int8_t blindsPresets[4][MAX_BLIND_COUNT];

  uint8_t blindsCount = 0;
  //0 means all
  uint8_t selectedBlind = 0;
};
static_assert(sizeof(BlindsData) < 400, "BlindsData won't fit in EEPROM"); //check the address of settings

#endif