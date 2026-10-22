/**
 * @file sddsInput.h
 * @brief Reject categorical fields where a reader requires primitive values.
 * @copyright See the Software License Agreement in LICENSE.
 */

#ifndef SDDS_INPUT_H
#define SDDS_INPUT_H

#include "SDDS.h"
#include <stdio.h>

/**
 * @brief Keep optional enum fields from being mistaken for absent fields.
 *
 * Existing readers validate their supported primitive types separately. This
 * check rejects enums in the named fields, while accepting unrelated enum
 * metadata. In particular, an enum must not silently disable a limit or select
 * a fallback provider, name, or scale factor.
 * @param input Initialized SDDS input dataset.
 * @param columns Null-terminated primitive column names, or NULL.
 * @param parameters Null-terminated primitive parameter names, or NULL.
 * @return One on success, zero with an SDDS error for a categorical field.
 */
static inline int checkNonEnumInputs(SDDS_DATASET *input,
                                    const char *const *columns,
                                    const char *const *parameters) {
  int kind;
  for (kind = 0; kind < 2; kind++) {
    const char *const *names = kind ? parameters : columns;
    if (!names)
      continue;
    for (; *names; names++) {
      int32_t index = kind ? SDDS_GetParameterIndex(input, (char *)*names) : SDDS_GetColumnIndex(input, (char *)*names);
      if (index >= 0 && (kind ? SDDS_GetParameterType(input, index) : SDDS_GetColumnType(input, index)) == SDDS_ENUM) {
        char message[1024];
        snprintf(message, sizeof(message),
                 "%s %s has enum type; use sddscast to convert explicitly to the required primitive type",
                 kind ? "Parameter" : "Column", *names);
        SDDS_SetError(message);
        return 0;
      }
    }
  }
  return 1;
}

/**
 * @brief Reject categorical columns before appending primitive sample buffers.
 *
 * The monitor append readers validate column names separately. Every output
 * column receives primitive data, so a matching name alone is insufficient:
 * enum storage would interpret those buffers or variadic arguments as codes.
 * Unrelated enum parameters and arrays can still be copied as metadata.
 * @param input Existing output dataset opened for reading.
 * @return One on success, zero with an SDDS error for an enum column.
 */
static inline int checkNonEnumOutputColumns(SDDS_DATASET *input) {
  int32_t i;
  for (i = 0; i < input->layout.n_columns; i++) {
    if (input->layout.column_definition[i].type == SDDS_ENUM) {
      const char *columns[] = {input->layout.column_definition[i].name, NULL};
      return checkNonEnumInputs(input, columns, NULL);
    }
  }
  return 1;
}

/**
 * @brief Check primitive columns and time parameters in an existing event log.
 * @param input Existing log opened for reading.
 * @return One on success, zero with an SDDS error for an enum field.
 */
static inline int checkNonEnumLogLayout(SDDS_DATASET *input) {
  const char *parameters[] = {"TimeStamp", "PageTimeStamp", "StartTime", "YearStartTime",
                              "StartYear", "StartJulianDay", "StartDayOfMonth",
                              "StartMonth", "StartHour", NULL};
  return checkNonEnumOutputColumns(input) && checkNonEnumInputs(input, NULL, parameters);
}

#endif
