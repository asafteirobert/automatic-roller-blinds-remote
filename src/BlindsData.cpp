#include <BlindsData.h>
#include <EEPROM.h>

void BlindsData::resetToDefault()
{
  for (uint8_t i = 0; i<MAX_BLIND_COUNT; i++)
  {
    this->blinds[i].blindId = 0;
    this->blinds[i].currentTargetPercent = -1;
  }
  for (uint8_t j = 0; j<4; j++)
    for (uint8_t i = 0; i<MAX_BLIND_COUNT; i++)
      this->blindsPresets[j][i] = 0;
  this->blindsCount = 0;
  this->selectedBlind = 0;
  this->saveToEEPROM();
}

void BlindsData::loadFromEEPROM()
{
  // Load settings from EEPROM to this
  EEPROM.get(0, *this);

  if (this->settingsVersion != BLINDS_DATA_VERSION_CHECK)
    this->resetToDefault();
}

void BlindsData::saveToEEPROM()
{
  //sizeof(BlindsData);
  this->settingsVersion = BLINDS_DATA_VERSION_CHECK;
  EEPROM.put(0, *this);
}

int8_t BlindsData::getAveragePercent()
{
  uint16_t result = 0;
  if (this->blindsCount > 0)
  {
    for (uint8_t i = 0; i<this->blindsCount; i++)
      result+=this->blinds[i].currentTargetPercent;

    result /= this->blindsCount;
  }
  
  //round to nearest 10
  return 10*((result + 5)/10);
}

int8_t BlindsData::getSelectedBlindPercent()
{
  if (this->selectedBlind == 0)
    return this->getAveragePercent();
  return this->blinds[this->selectedBlind-1].currentTargetPercent;
}

void BlindsData::savePreset(uint8_t presetToSave)
{
  for (uint8_t i = 0; i<MAX_BLIND_COUNT; i++)
    blindsPresets[presetToSave-1][i] = blinds[i].currentTargetPercent;
  this->saveToEEPROM();
}

void BlindsData::loadPreset(uint8_t presetToLoad)
{
  for (uint8_t i = 0; i<MAX_BLIND_COUNT; i++)
    blinds[i].currentTargetPercent = blindsPresets[presetToLoad-1][i];
}