#ifndef LIBSIGROK_DSLOGIC_TRIGGER_H
#define LIBSIGROK_DSLOGIC_TRIGGER_H

#include "libsigrok.h"
#include "libsigrok-internal.h"

SR_PRIV uint64_t ds_trigger_get_mask0(uint16_t stage);
SR_PRIV uint64_t ds_trigger_get_value0(uint16_t stage);
SR_PRIV uint64_t ds_trigger_get_edge0(uint16_t stage);
SR_PRIV uint64_t ds_trigger_get_mask1(uint16_t stage);
SR_PRIV uint64_t ds_trigger_get_mask1(uint16_t stage);
SR_PRIV uint64_t ds_trigger_get_value1(uint16_t stage);
SR_PRIV uint64_t ds_trigger_get_edge1(uint16_t stage);

#endif