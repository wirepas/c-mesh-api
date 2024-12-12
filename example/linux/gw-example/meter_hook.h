/* Wirepas Oy licensed under Apache License, Version 2.0
 *
 * See file LICENSE for full license details.
 *
 */
#ifndef METER_HOOK_H_
#define METER_HOOK_H_

#include "stdint.h"

void Meter_Hook_init(uint32_t meter_address);

void Meter_Hook_start(void);

void Meter_Hook_stop(void);

#endif // METER_HOOK_H_