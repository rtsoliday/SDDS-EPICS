/**
 * @file sddsPVInput.h
 * @brief Validate categorical fields in SDDS-configured PV server inputs.
 * @copyright See the Software License Agreement in LICENSE.
 */

#ifndef SDDS_PV_INPUT_H
#define SDDS_PV_INPUT_H

#include "SDDS.h"
#include <stdio.h>

/**
 * @brief Reject enums in fields requiring primitive PV configuration values.
 *
 * Optional fields must not be mistaken for absent fields when their type is
 * enum. In particular, dictionary keys are not limits or element counts.
 * Enum metadata with other names remains valid input.
 * @param input Initialized SDDS input dataset.
 * @return One on success, zero with an SDDS error for an unsupported enum.
 */
static inline int checkPVInputEnums(SDDS_DATASET *input) {
  const char *names[] = {"ControlName", "Hopr", "Lopr", "ReadbackUnits",
                         "ElementCount", "Type", "EnumStrings", "Equation"};
  for (unsigned int i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
    int32_t index = SDDS_GetColumnIndex(input, (char *)names[i]);
    if (index >= 0 && SDDS_GetColumnType(input, index) == SDDS_ENUM) {
      char message[256];
      snprintf(message, sizeof(message),
               "PV definition column %s cannot have enum type; use sddscast to convert explicitly to the required primitive type",
               names[i]);
      SDDS_SetError(message);
      return 0;
    }
  }
  return 1;
}

/**
 * @brief Validate string storage before reading or updating a PV registry.
 *
 * Enum codes cannot be read as char pointers, even when their labels are PV or
 * host names. Other enum metadata is allowed in registries used for lookup.
 * @param input Initialized registry dataset.
 * @param includeHost Nonzero for a registry requiring host_name as well.
 * @return One on success, zero with an SDDS error for an invalid name column.
 */
static inline int checkPVRegistryInput(SDDS_DATASET *input, int includeHost) {
  const char *names[] = {"rec_name", "host_name"};
  for (int i = 0; i < (includeHost ? 2 : 1); i++) {
    if (SDDS_CheckColumn(input, (char *)names[i], NULL, SDDS_STRING, NULL) != SDDS_CHECK_OKAY) {
      char message[256];
      snprintf(message, sizeof(message), "PV registry column %s must have string type", names[i]);
      SDDS_SetError(message);
      return 0;
    }
  }
  return 1;
}

#endif
