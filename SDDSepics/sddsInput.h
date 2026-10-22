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

/**
 * @brief Report whether a dataset defines any enum column, parameter, or array.
 * @param input Initialized SDDS dataset.
 * @return One if any element has enum type, zero otherwise.
 */
static inline int inputHasEnums(SDDS_DATASET *input) {
  int32_t i;
  for (i = 0; i < input->layout.n_columns; i++)
    if (input->layout.column_definition[i].type == SDDS_ENUM)
      return 1;
  for (i = 0; i < input->layout.n_parameters; i++)
    if (input->layout.parameter_definition[i].type == SDDS_ENUM)
      return 1;
  for (i = 0; i < input->layout.n_arrays; i++)
    if (input->layout.array_definition[i].type == SDDS_ENUM)
      return 1;
  return 0;
}

/**
 * @brief Classify a page read that follows at least one successfully read page.
 *
 * Enum-free files keep their historical behavior: a failed read after valid
 * pages, such as a trailing blank line in an ASCII file with row counts or a
 * truncated final page, simply ends the input and the pending SDDS error is
 * discarded. Files that define enums report the failure, because it may be an
 * undefined enum key that must not silently truncate the input.
 * @param input Dataset that returned pageCode from SDDS_ReadPage/ReadTable.
 * @param pageCode Return value of the page read.
 * @return One if the caller must treat the read as an error, zero otherwise.
 */
static inline int laterPageReadFailed(SDDS_DATASET *input, int32_t pageCode) {
  if (pageCode != 0)
    return 0;
  if (inputHasEnums(input))
    return 1;
  SDDS_ClearErrors();
  return 0;
}

/**
 * @brief Validate all ignored pages of a configuration that defines enums.
 *
 * Call only after extracting the first page's data. Reading a single extra
 * page misses invalid dictionary keys on the third or any later page.
 * Enum-free inputs retain their historical first-page-only behavior.
 * @param input Dataset with its first page already read successfully.
 * @return One on success, zero if a remaining enum page cannot be read.
 */
static inline int checkRemainingEnumPages(SDDS_DATASET *input) {
  int32_t pageCode;
  if (!inputHasEnums(input))
    return 1;
  while ((pageCode = SDDS_ReadPage(input)) > 0)
    ;
  return !laterPageReadFailed(input, pageCode);
}

/**
 * @brief Validate ignored enum pages while retaining the current page's data.
 *
 * Save and waveform readers may copy the first page's metadata into output
 * after validating the rest of the input. A memory copy also supports stdin.
 * @param input Dataset with its first page already read successfully.
 * @return One on success, zero if copying or validation fails.
 */
static inline int checkRemainingEnumPagesPreservePage(SDDS_DATASET *input) {
  SDDS_DATASET savedPage;
  int32_t copied;

  if (!inputHasEnums(input))
    return 1;
  if (!SDDS_InitializeCopy(&savedPage, input, NULL, (char *)"m"))
    return 0;
  if (!SDDS_CopyPage(&savedPage, input) || !checkRemainingEnumPages(input)) {
    SDDS_Terminate(&savedPage);
    return 0;
  }
  copied = SDDS_CopyPage(input, &savedPage);
  if (!SDDS_Terminate(&savedPage))
    return 0;
  return copied;
}

/**
 * @brief Carry non-fixed enum parameter values into a page appended to a file.
 *
 * Appended pages start with zero-initialized parameters. Programs that set only
 * their own parameters would otherwise write code zero for unrelated enum
 * parameters, which is invalid unless the dictionary maps zero. Values are taken
 * from the first page of the existing file. Files without non-fixed enum
 * parameters are not read, so enum-free appends are unchanged.
 * @param target Dataset opened with SDDS_InitializeAppend after SDDS_StartPage.
 * @param filename Existing file being appended to.
 * @return One on success, zero with an SDDS error on failure.
 */
static inline int copyExistingEnumParameters(SDDS_DATASET *target, const char *filename) {
  SDDS_DATASET existing;
  int32_t i, index, pageCode, needed = 0;

  for (i = 0; i < target->layout.n_parameters; i++)
    if (target->layout.parameter_definition[i].type == SDDS_ENUM &&
        !target->layout.parameter_definition[i].fixed_value)
      needed = 1;
  if (!needed)
    return 1;
  if (!SDDS_InitializeInput(&existing, (char *)filename))
    return 0;
  if ((pageCode = SDDS_ReadPage(&existing)) <= 0) {
    if (pageCode < 0)
      SDDS_SetError((char *)"Existing file has no page from which to copy enum parameter values");
    SDDS_Terminate(&existing);
    return 0;
  }
  for (i = 0; i < target->layout.n_parameters; i++) {
    PARAMETER_DEFINITION *definition = target->layout.parameter_definition + i;
    if (definition->type != SDDS_ENUM || definition->fixed_value)
      continue;
    if ((index = SDDS_GetParameterIndex(&existing, definition->name)) < 0 ||
        existing.layout.parameter_definition[index].type != SDDS_ENUM ||
        !SDDS_SetParameters(target, SDDS_SET_BY_INDEX | SDDS_PASS_BY_REFERENCE, i, existing.parameter[index], -1)) {
      SDDS_SetError((char *)"Unable to copy enum parameter value from existing file");
      SDDS_Terminate(&existing);
      return 0;
    }
  }
  return SDDS_Terminate(&existing);
}

#endif
