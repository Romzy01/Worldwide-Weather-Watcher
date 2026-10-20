#ifndef CAPTEUR_BME280_H
#define CAPTEUR_BME280_H

#include <stdbool.h>
#include "cesi_types.h"

bool bme280_init(void);
bool bme280_lire(Env_Data *env);

#endif