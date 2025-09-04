#pragma once


#include "esp_err.h"
#include <stdint.h>

extern uint8_t temp_int, humi_int;

extern esp_err_t gxhtc3_read_id(void);
extern esp_err_t gxhtc3_get_tah(void);
