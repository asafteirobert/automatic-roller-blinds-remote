#ifndef _UTIL_h
#define _UTIL_h
#include <Constants.h>

// Function to calculate and return the remote's battery voltage.
float batteryVoltage()
{
  //analogReference(INTERNAL);
  //analogRead(PIN_BATTERY_MEASURE);
  //delay(5);
  //analogRead(PIN_BATTERY_MEASURE);

  pinMode(PIN_BATTERY_MEASURE_GROUND, OUTPUT);
  digitalWrite(PIN_BATTERY_MEASURE_GROUND, LOW);
  delay(5);

  int total = 0;
  for (int i = 0; i < 10; i++)
    total += analogRead(PIN_BATTERY_MEASURE);

  //analogReference(DEFAULT);
  //analogRead(PIN_HALL_SENSOR);
  //delay(1);
  pinMode(PIN_BATTERY_MEASURE_GROUND, INPUT); //high-z


  return (REMOTE_BATTERY_SENSOR_REF_VOLTAGE / 1024.0) * ((float)total / 10.0) * REMOTE_BATTERY_SENSOR_MULTIPLIER;
}

float mapfloat(float x, float in_min, float in_max, float out_min, float out_max)
{
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

//return percent from voltage. Type is 0 for Lipo, 1 for Li-ion
float calculateBatteryLevel(float volatge, byte type)
{
  int i;

  // volatge is less than the minimum
  if (volatge < pgm_read_float(&(BATTERY_LEVEL_VOLTAGE[0])))
    return pgm_read_byte(&(BATTERY_LEVEL_PERCENT[type][0]));

  //voltage more than maximum
  if (volatge > pgm_read_float(&(BATTERY_LEVEL_VOLTAGE[BATTERY_LEVEL_TABLE_COUNT - 1])))
    return pgm_read_byte(&(BATTERY_LEVEL_PERCENT[type][BATTERY_LEVEL_TABLE_COUNT - 1]));

  // find i, such that BATTERY_LEVEL_VOLTAGE[i] <= volatge < BATTERY_LEVEL_VOLTAGE[i+1]
  for (i = 0; i < BATTERY_LEVEL_TABLE_COUNT - 1; i++)
    if (pgm_read_float(&(BATTERY_LEVEL_VOLTAGE[i + 1])) > volatge)
      break;

  //interpolate
  return float(pgm_read_byte(&(BATTERY_LEVEL_PERCENT[type][i]))) +
    (volatge - pgm_read_float(&(BATTERY_LEVEL_VOLTAGE[i]))) *
    (float(pgm_read_byte(&(BATTERY_LEVEL_PERCENT[type][i + 1]))) - float(pgm_read_byte(&(BATTERY_LEVEL_PERCENT[type][i])))) /
    (pgm_read_float(&(BATTERY_LEVEL_VOLTAGE[i + 1])) - pgm_read_float(&(BATTERY_LEVEL_VOLTAGE[i])));
}
#endif