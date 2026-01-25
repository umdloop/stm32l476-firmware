#ifndef CAN_PARAMS_H
#define CAN_PARAMS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

/* Parameter lookup/access by full signal name: "MESSAGE.SIGNAL" */

bool CanParams_IsValid(const char* full_name);

bool CanParams_GetBool(const char* full_name, bool* out_value);
bool CanParams_GetInt32(const char* full_name, int32_t* out_value);
bool CanParams_GetFloat(const char* full_name, float* out_value);

/* (Optional future) setters for TX use. Not used yet. */
bool CanParams_SetBool(const char* full_name, bool value);
bool CanParams_SetInt32(const char* full_name, int32_t value);
bool CanParams_SetFloat(const char* full_name, float value);

#ifdef __cplusplus
}
#endif

#endif /* CAN_PARAMS_H */
