/**
 * @file sddscainfo.c
 * @brief Retrieves meta information about PVs and stores the info in an SDDS file.
 *
 * @details
 * This program reads an SDDS file containing PV names, connects to them,
 * retrieves metadata, and writes the output back to an SDDS file.
 *
 * @section Usage
 * ```
 * sddscainfo [<inputfile>] [<outputfile>] 
 *            [-pipe=[input][,output]]
 *            [-makeLoggerInput[=withStorageType]]
 *            [-makeSCRInput]
 *            [-includeStorageType]
 *            [-verbose]
 *            [-pendIOTime=<seconds>]
 * ```
 *
 * @section Options
 * | Option                       | Description |
 * |------------------------------|-------------|
 * | `-pipe`                      | Enables piping for input and/or output.           |
 * | `-makeLoggerInput`           | Write file that can be used by sddspvalogger      |
 * | `-makeSCRInput`              | Write file that can be used by sddspvasaverestore |
 * | `-verbose`                   | Enables verbose output.                           |
 * | `-pendIOTime`                | Specifies the time to wait for a response.        |
 *
 * @copyright
 *   - (c) 2002 The University of Chicago, as Operator of Argonne National Laboratory.
 *   - (c) 2002 The Regents of the University of California, as Operator of Los Alamos National Laboratory.
 *
 * @license
 * This file is distributed under the terms of the Software License Agreement
 * found in the file LICENSE included with this distribution.
 *
 * @authors
 * R. Soliday (ANL)
 *
 */

#include <complex>
#include <iostream>
#include <cerrno>
#include <csignal>
#include <string>

#include <cadef.h>
#include <epicsVersion.h>

#include "mdb.h"
#include "scan.h"
#include "SDDS.h"
#include "SDDSepics.h"
#include "pvaSDDS.h"

//#include "../modules/pvAccess/src/ca/caChannel.h"

#define CLO_SET_PIPE 0
#define CLO_VERBOSE 1
#define CLO_PENDIOTIME 2
#define CLO_MAKE_LOGGER_INPUT 3
#define CLO_MAKE_SCR_INPUT 4
#define COMMANDLINE_OPTIONS 5

static char *commandline_option[COMMANDLINE_OPTIONS] = {
  (char*)"pipe",
  (char*)"verbose",
  (char*)"pendIOTime",
  (char*)"makeLoggerInput",
  (char*)"makeSCRInput"
};

static const char *USAGE = 
  "Usage: sddscainfo [<inputfile>] [<outputfile>] \n"
  "       [-pipe=[input][,output]] \n"
  "       [-makeLoggerInput[=withStorageType]] \n"
  "       [-makeSCRInput] \n"
  "       [-verbose] [-pendIOTime=<seconds>]\n"
  "\nRequired SDDS columns: ControlName (string)\n"
  "Program by Robert Soliday, ANL\n"
  "Link date: " __DATE__ " " __TIME__ ", SVN revision: " SVN_VERSION ", " EPICS_VERSION_STRING "\n";

typedef struct
{
  int64_t pvCount;
  char **ControlName;
} SDDS_INPUT_DATA_PAGE;

typedef struct
{
  int64_t pvCount;
  char **ControlName;
  char *Active;
  char **Provider;
  char **IOCHost;
  char *ReadAccess;
  char *WriteAccess;
  char **StructureID;
  char **FieldType;
  uint32_t *ElementCount;
  char **NativeDataType;
  char **Units;
  uint32_t *EnumNumOfChoices;
  char ***EnumChoices;
} SDDS_OUTPUT_DATA;

typedef struct
{
  bool verbose;
  bool makeLoggerInput, withStorageType;
  bool makeSCRInput;
  char *inputFile;
  char *outputFile;
  unsigned long pipeFlags;
  long tmpfile_used;
  double pendIOTime;
  SDDS_INPUT_DATA_PAGE *inputPage;
  SDDS_OUTPUT_DATA outputData;
  int32_t inputPages;
  int64_t total_pvCount;
} PROG_DATA;

int ReadArguments(int argc, char **argv, PROG_DATA *prog_data);
int ReadInput(PROG_DATA *prog_data);
int ConnectPVs(PVA_OVERALL *pva, PVA_OVERALL *pva_ca, PROG_DATA *prog_data);
int GetPVinfo(PVA_OVERALL *pva_pva, PVA_OVERALL *pva_ca, PROG_DATA *prog_data);
int WriteOutput(PROG_DATA *prog_data);
void FreeMemory(PROG_DATA *prog_data);
long pvaThreadSleep(long double seconds);

#if defined(_WIN32)
#  include <thread>
#  include <chrono>
int nanosleep(const struct timespec *req, struct timespec *rem) {
  long double ldSeconds;
  long mSeconds;
  ldSeconds = req->tv_sec + req->tv_nsec;
  mSeconds = ldSeconds / 1000000;
  std::this_thread::sleep_for(std::chrono::microseconds(mSeconds));
  return (0);
}
#endif

int main(int argc, char **argv) {
  PVA_OVERALL pva, pva_ca;
  PROG_DATA prog_data;

  if (ReadArguments(argc, argv, &prog_data) == 1) {
    return (EXIT_FAILURE);
  }
  if (ReadInput(&prog_data) == 1) {
    return (EXIT_FAILURE);
  }
  if (ConnectPVs(&pva, &pva_ca, &prog_data) == 1) {
    return (EXIT_FAILURE);
  }
  if (ExtractPVAUnits(&pva) == 1) {
    return (EXIT_FAILURE);
  }
  if (ExtractPVAUnits(&pva_ca) == 1) {
    return (EXIT_FAILURE);
  }
  if (GetPVinfo(&pva, &pva_ca, &prog_data) == 1) {
    return (EXIT_FAILURE);
  }
  if (WriteOutput(&prog_data) == 1) {
    return (EXIT_FAILURE);
  }
  FreeMemory(&prog_data);

  freePVA(&pva);
  freePVA(&pva_ca);
  return (EXIT_SUCCESS);
}

int ReadArguments(int argc, char **argv, PROG_DATA *prog_data) {
  SCANNED_ARG *s_arg;
  long i_arg;

  prog_data->verbose = false;
  prog_data->makeLoggerInput = false;
  prog_data->withStorageType = false;
  prog_data->makeSCRInput = false;
  prog_data->inputFile = NULL;
  prog_data->outputFile = NULL;
  prog_data->pipeFlags = 0;
  prog_data->pendIOTime = .5;

  SDDS_RegisterProgramName("sddscainfo");

  argc = scanargs(&s_arg, argc, argv);
  if (argc < 2) {
    fprintf(stderr, "%s\n", USAGE);
    return (1);
  }
  for (i_arg = 1; i_arg < argc; i_arg++) {
    if (s_arg[i_arg].arg_type == OPTION) {
      switch (match_string(s_arg[i_arg].list[0], commandline_option, COMMANDLINE_OPTIONS, 0)) {
      case CLO_SET_PIPE:
        if (!processPipeOption(s_arg[i_arg].list + 1, s_arg[i_arg].n_items - 1, &prog_data->pipeFlags)) {
          fprintf(stderr, "invalid -pipe syntax\n");
          return (1);
        }
        break;
      case CLO_PENDIOTIME:
        if (s_arg[i_arg].n_items != 2 ||
            !(get_double(&prog_data->pendIOTime, s_arg[i_arg].list[1]))) {
          fprintf(stderr, "no value given for option -pendIOTime\n");
          return (1);
        }
        break;
      case CLO_VERBOSE:
        prog_data->verbose = true;
        break;
      case CLO_MAKE_LOGGER_INPUT:
        prog_data->makeLoggerInput = true;
        if (s_arg[i_arg].n_items == 2) {
          char *outputMode[1] = {(char*)"withStorageType"};
          int rm = match_string(s_arg[i_arg].list[1], outputMode, 1, DCL_STYLE_MATCH);
          if (rm == 0) {
            prog_data->withStorageType = true;
          } else {
            fprintf(stderr, "invalid -makeLoggerInput syntax\n");
            return (1);
          }
        }
        break;
      case CLO_MAKE_SCR_INPUT:
        prog_data->makeSCRInput = true;
        break;
      }
    } else {
      if (!prog_data->inputFile)
        SDDS_CopyString(&prog_data->inputFile, s_arg[i_arg].list[0]);
      else if (!prog_data->outputFile)
        SDDS_CopyString(&prog_data->outputFile, s_arg[i_arg].list[0]);
      else {
        fprintf(stderr, "too many filenames given\n");
        return (1);
      }
    }
  }
  if ((prog_data->pipeFlags == 0) && (prog_data->outputFile == NULL)) {
    fprintf(stderr, "%s\n", USAGE);
    return (1);
  }
  if (prog_data->makeLoggerInput && prog_data->makeSCRInput) {
    fprintf(stderr, "-makeLoggerInput can't be used with -makeSCRInput\n");
    return (1);
  }

  prog_data->tmpfile_used = 0;
  processFilenames((char*)"sddscainfo", &prog_data->inputFile, &prog_data->outputFile, prog_data->pipeFlags, 1, &prog_data->tmpfile_used);

  free_scanargs(&s_arg, argc);
  return (0);
}


int ReadInput(PROG_DATA *prog_data) {
  SDDS_DATASET input;
  prog_data->inputPages = 0;
  prog_data->total_pvCount = 0;
  int32_t page=0;

  if (!SDDS_InitializeInput(&input, prog_data->inputFile)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return (1);
  }
  if (SDDS_CheckColumn(&input, (char *)"ControlName", NULL, SDDS_STRING, NULL) != SDDS_CHECK_OK) {
    fprintf(stderr, "Column \"ControlName\" does not exist or something wrong with it in input file %s\n", prog_data->inputFile);
    return (1);
  }
  while (SDDS_ReadPage(&input) > 0) {
    page++;
    if (page == 1) {
      prog_data->inputPage = (SDDS_INPUT_DATA_PAGE *)malloc(sizeof(SDDS_INPUT_DATA_PAGE));
    } else {
      prog_data->inputPage = (SDDS_INPUT_DATA_PAGE *)realloc(prog_data->inputPage, sizeof(SDDS_INPUT_DATA_PAGE) * page);
    }
    prog_data->inputPages = page;
    prog_data->inputPage[page-1].pvCount = input.n_rows;
    prog_data->total_pvCount += input.n_rows;
    prog_data->inputPage[page-1].ControlName = NULL;
    if (input.n_rows > 0) {
      if (!(prog_data->inputPage[page-1].ControlName = (char **)SDDS_GetColumn(&input, (char *)"ControlName"))) {
        fprintf(stderr, (char *)"No data provided in the input file.\n");
        return (1);
      }
    }
  }
  if (!SDDS_Terminate(&input)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return (1);
  }
  return (0);
}

int find_index(char *array[], int size, const char *target) {
  for (int i = 0; i < size; i++) {
    if (strcmp(array[i], target) == 0) {
      return i;  // Return the index of the matching string
    }
  }
  return -1;  // Return -1 if not found
}

int WriteOutput(PROG_DATA *prog_data) {
  int32_t page=0, index;
  int64_t i;
  SDDS_DATASET input, output;
  char **EnumChoices;
  bool ProviderExists=false, IOCHostExists=false, ReadAccessExists=false, WriteAccessExists=false, StructureIDExists=false;
  bool FieldTypeExists=false, ElementCountExists=false, NativeDataTypeExists=false, UnitsExists=false, EnumChoicesExists=false;
  bool ExpectElementsExists=false, StorageTypeExists=false, ExpectFieldTypeExists=false, ExpectNumericExists=false;

  EnumChoices = (char**)malloc(sizeof(char*) * prog_data->outputData.pvCount);
  for (i=0; i<prog_data->outputData.pvCount; i++) {
    if (prog_data->outputData.EnumNumOfChoices[i] > 0) {
      int length = 0, index = 0;
      for (uint32_t j = 0; j < prog_data->outputData.EnumNumOfChoices[i]; j++) {
        length += strlen(prog_data->outputData.EnumChoices[i][j]) + 1;
      }
      EnumChoices[i] = (char*)malloc(sizeof(char) * length);
      for (uint32_t j = 0; j < prog_data->outputData.EnumNumOfChoices[i]; j++) {
        strcpy(&EnumChoices[i][index], prog_data->outputData.EnumChoices[i][j]);
        index += strlen(prog_data->outputData.EnumChoices[i][j]);
        EnumChoices[i][index] = ',';
        index++;
      }
      EnumChoices[i][length - 1] = '\0';
    } else {
      SDDS_CopyString(&EnumChoices[i], "");
    }
  }

  if (!SDDS_InitializeInput(&input, prog_data->inputFile)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return (1);
  }
  if (SDDS_InitializeCopy(&output, &input, prog_data->outputFile, (char*)"w") != 1) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return(1);
  }

  output.layout.data_mode.column_major = SDDS_COLUMN_MAJOR_ORDER;

  if (SDDS_CheckColumn(&output, (char*)"Active", NULL, SDDS_CHARACTER, NULL) != SDDS_CHECK_OK) {
    if (SDDS_DefineSimpleColumn(&output, "Active", NULL, SDDS_CHARACTER)!=1) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      return(1);
    }
  }
  if (SDDS_CheckColumn(&output, (char*)"Provider", NULL, SDDS_STRING, NULL) != SDDS_CHECK_OK) {
    if (SDDS_DefineSimpleColumn(&output, "Provider", NULL, SDDS_STRING)!=1) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      return(1);
    }
  } else {
    ProviderExists = true;
  }
  if (SDDS_CheckColumn(&output, (char*)"Units", NULL, SDDS_STRING, NULL) != SDDS_CHECK_OK) {
    if (SDDS_DefineSimpleColumn(&output, "Units", NULL, SDDS_STRING)!=1) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      return(1);
    }
  } else {
    UnitsExists = true;
  }

  if (prog_data->makeLoggerInput || prog_data->makeSCRInput) {
    if (SDDS_CheckColumn(&output, (char*)"ExpectNumeric", NULL, SDDS_CHARACTER, NULL) != SDDS_CHECK_OK) {
      if (SDDS_DefineSimpleColumn(&output, "ExpectNumeric", NULL, SDDS_CHARACTER)!=1) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        return(1);
      }
    } else {
      ExpectNumericExists = true;
    }
    if (SDDS_CheckColumn(&output, (char*)"ExpectFieldType", NULL, SDDS_STRING, NULL) != SDDS_CHECK_OK) {
      if (SDDS_DefineSimpleColumn(&output, "ExpectFieldType", NULL, SDDS_STRING)!=1) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        return(1);
      }
    } else {
      ExpectFieldTypeExists = true;
    }
    if (SDDS_CheckColumn(&output, (char*)"ExpectElements", NULL, SDDS_ANY_INTEGER_TYPE, NULL) != SDDS_CHECK_OK) {
      if (SDDS_DefineSimpleColumn(&output, "ExpectElements", NULL, SDDS_LONG)!=1) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        return(1);
      }
    } else {
      ExpectElementsExists = true;
    }
    if (prog_data->withStorageType) {
      if (SDDS_CheckColumn(&output, (char*)"StorageType", NULL, SDDS_STRING, NULL) != SDDS_CHECK_OK) {
        if (SDDS_DefineSimpleColumn(&output, "StorageType", NULL, SDDS_STRING)!=1) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          return(1);
        }
      } else {
        StorageTypeExists = true;
      }
    }
  } else {
    if (SDDS_CheckColumn(&output, (char*)"IOCHost", NULL, SDDS_STRING, NULL) != SDDS_CHECK_OK) {
      if (SDDS_DefineSimpleColumn(&output, "IOCHost", NULL, SDDS_STRING)!=1) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        return(1);
      }
    } else {
      IOCHostExists = true;
    }
    if (SDDS_CheckColumn(&output, (char*)"ReadAccess", NULL, SDDS_CHARACTER, NULL) != SDDS_CHECK_OK) {
      if (SDDS_DefineSimpleColumn(&output, "ReadAccess", NULL, SDDS_CHARACTER)!=1) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        return(1);
      }
    } else {
      ReadAccessExists = true;
    }
    if (SDDS_CheckColumn(&output, (char*)"WriteAccess", NULL, SDDS_CHARACTER, NULL) != SDDS_CHECK_OK) {
      if (SDDS_DefineSimpleColumn(&output, "WriteAccess", NULL, SDDS_CHARACTER)!=1) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        return(1);
      }
    } else {
      WriteAccessExists = true;
    }
    if (SDDS_CheckColumn(&output, (char*)"StructureID", NULL, SDDS_STRING, NULL) != SDDS_CHECK_OK) {
      if (SDDS_DefineSimpleColumn(&output, "StructureID", NULL, SDDS_STRING)!=1) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        return(1);
      }
    } else {
      StructureIDExists = true;
    }
    if (SDDS_CheckColumn(&output, (char*)"FieldType", NULL, SDDS_STRING, NULL) != SDDS_CHECK_OK) {
      if (SDDS_DefineSimpleColumn(&output, "FieldType", NULL, SDDS_STRING)!=1) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        return(1);
      }
    } else {
      FieldTypeExists = true;
    }
    if (SDDS_CheckColumn(&output, (char*)"ElementCount", NULL, SDDS_ANY_INTEGER_TYPE, NULL) != SDDS_CHECK_OK) {
      if (SDDS_DefineSimpleColumn(&output, "ElementCount", NULL, SDDS_ULONG)!=1) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        return(1);
      }
    } else {
      ElementCountExists = true;
    }
    if (SDDS_CheckColumn(&output, (char*)"NativeDataType", NULL, SDDS_STRING, NULL) != SDDS_CHECK_OK) {
      if (SDDS_DefineSimpleColumn(&output, "NativeDataType", NULL, SDDS_STRING)!=1) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        return(1);
      }
    } else {
      NativeDataTypeExists = true;
    }
    if (SDDS_CheckColumn(&output, (char*)"EnumChoices", NULL, SDDS_STRING, NULL) != SDDS_CHECK_OK) {
      if (SDDS_DefineSimpleColumn(&output, "EnumChoices", NULL, SDDS_STRING)!=1) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        return(1);
      }
    } else {
      EnumChoicesExists = true;
    }
  }

  if (SDDS_WriteLayout(&output)!=1) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return(1);
  }

  while ((page = SDDS_ReadPage(&input)) > 0) {
    if (SDDS_CopyPage(&output, &input) != 1) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      return (1);
    }
    for (int32_t i = 0; i < prog_data->inputPage[page-1].pvCount; i++) {
      index = find_index(prog_data->outputData.ControlName, prog_data->outputData.pvCount, prog_data->inputPage[page-1].ControlName[i]);
      if (prog_data->outputData.Active[index] == 'y') {


        if (SDDS_SetRowValues(&output, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, i,
                              "Active", prog_data->outputData.Active[index],
                              "Provider", prog_data->outputData.Provider[index],
                              "Units", prog_data->outputData.Units[index],  
                              NULL) != 1) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          return(1);
        }
        if (prog_data->makeLoggerInput) {
          char c = 'y';
          if (prog_data->outputData.EnumNumOfChoices[index] > 0)
            c = 'y';
          else if (strcmp(prog_data->outputData.NativeDataType[index], "string") == 0)
            c = 'n';
	  else if ((strcmp(prog_data->outputData.NativeDataType[index], "byte") == 0) && (prog_data->outputData.ElementCount[index] == 256))
	    c = 'n';
          if (SDDS_SetRowValues(&output, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, i,
                                "ExpectNumeric", c,
                                "ExpectFieldType", (strcmp(prog_data->outputData.FieldType[index], "scalarArray") == 0) ? "scalarArray" : "scalar" ,
                                "ExpectElements",  prog_data->outputData.ElementCount[index],
                                NULL) != 1) {
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
            return(1);
          }
          if (prog_data->withStorageType) {
            if (SDDS_SetRowValues(&output, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, i,
                                  "StorageType",  prog_data->outputData.NativeDataType[index],
                                  NULL) != 1) {
              SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
              return(1);
            }
          }

        } else if (prog_data->makeSCRInput) {
          char c = 'y';
          if (prog_data->outputData.EnumNumOfChoices[index] > 0)
            c = 'n';
          else if (strcmp(prog_data->outputData.NativeDataType[index], "string") == 0)
            c = 'n';
	  else if ((strcmp(prog_data->outputData.NativeDataType[index], "byte") == 0) && (prog_data->outputData.ElementCount[index] == 256))
	    c = 'n';
          if (SDDS_SetRowValues(&output, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, i,
                                "ExpectNumeric", c,
                                "ExpectFieldType", (strcmp(prog_data->outputData.FieldType[index], "scalarArray") == 0) ? "scalarArray" : "scalar" ,
                                "ExpectElements",  prog_data->outputData.ElementCount[index],
                                NULL) != 1) {
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
            return(1);
          }
        } else {
          if (SDDS_SetRowValues(&output, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, i,
                                "IOCHost", prog_data->outputData.IOCHost[index],
                                "ReadAccess", prog_data->outputData.ReadAccess[index],
                                "WriteAccess", prog_data->outputData.WriteAccess[index],
                                "StructureID", prog_data->outputData.StructureID[index], 
                                "FieldType", prog_data->outputData.FieldType[index],
                                "ElementCount", prog_data->outputData.ElementCount[index], 
                                "NativeDataType", prog_data->outputData.NativeDataType[index],
                                "EnumChoices", EnumChoices[index], 
                                NULL) != 1) {
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
            return(1);
          }
        }
      } else {
        if (SDDS_SetRowValues(&output, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, i, "Active", prog_data->outputData.Active[index], NULL) != 1) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          return(1);
        }
        if (!ProviderExists) {
          if (SDDS_SetRowValues(&output, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, i, "Provider", prog_data->outputData.Provider[index], NULL) != 1) {
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
            return(1);
          }
        }
        if (!UnitsExists) {
          if (SDDS_SetRowValues(&output, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, i, "Units", prog_data->outputData.Units[index], NULL) != 1) {
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
            return(1);
          }
        }
        if (prog_data->makeLoggerInput || prog_data->makeSCRInput) {
          if (!ExpectNumericExists) {
            if (SDDS_SetRowValues(&output, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, i, "ExpectNumeric", 'y', NULL) != 1) {
              SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
              return(1);
            }
          }
          if (!ExpectFieldTypeExists) {
            if (SDDS_SetRowValues(&output, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, i, "ExpectFieldType", "scalar", NULL) != 1) {
              SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
              return(1);
            }
          }
          if (!ExpectElementsExists) {
            if (SDDS_SetRowValues(&output, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, i, "ExpectElements", prog_data->outputData.ElementCount[index], NULL) != 1) {
              SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
              return(1);
            }
          }
          if (prog_data->withStorageType) {
            if (!StorageTypeExists) {
              if (SDDS_SetRowValues(&output, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, i, "StorageType", prog_data->outputData.NativeDataType[index], NULL) != 1) {
                SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
                return(1);
              }
            }
          }
        } else {
          if (!IOCHostExists) {
            if (SDDS_SetRowValues(&output, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, i, "IOCHost", prog_data->outputData.IOCHost[index], NULL) != 1) {
              SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
              return(1);
            }
          }
          if (!ReadAccessExists) {
            if (SDDS_SetRowValues(&output, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, i, "ReadAccess", prog_data->outputData.ReadAccess[index], NULL) != 1) {
              SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
              return(1);
            }
          }
          if (!WriteAccessExists) {
            if (SDDS_SetRowValues(&output, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, i, "WriteAccess", prog_data->outputData.WriteAccess[index], NULL) != 1) {
              SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
              return(1);
            }
          }
          if (!StructureIDExists) {
            if (SDDS_SetRowValues(&output, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, i, "StructureID", prog_data->outputData.StructureID[index], NULL) != 1) {
              SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
              return(1);
            }
          }
          if (!FieldTypeExists) {
            if (SDDS_SetRowValues(&output, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, i, "FieldType", prog_data->outputData.FieldType[index], NULL) != 1) {
              SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
              return(1);
            }
          }
          if (!ElementCountExists) {
            if (SDDS_SetRowValues(&output, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, i, "ElementCount", prog_data->outputData.ElementCount[index], NULL) != 1) {
              SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
              return(1);
            }
          }
          if (!NativeDataTypeExists) {
            if (SDDS_SetRowValues(&output, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, i, "NativeDataType", prog_data->outputData.NativeDataType[index], NULL) != 1) {
              SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
              return(1);
            }
          }
          if (!EnumChoicesExists) {
            if (SDDS_SetRowValues(&output, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, i, "EnumChoices", prog_data->outputData.EnumChoices[index], NULL) != 1) {
              SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
              return(1);
            }
          }
        }
      }
    }
    if (SDDS_WritePage(&output)!=1) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      return(1);
    }
  }
  if (!SDDS_Terminate(&input)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return (1);
  }

  if (SDDS_Terminate(&output)!=1) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return(1);
  }
  if (prog_data->tmpfile_used && !replaceFileAndBackUp(prog_data->inputFile, prog_data->outputFile))
    return(1);

  for (int64_t j=0; j<prog_data->outputData.pvCount; j++) {
    if (EnumChoices[j])
      free(EnumChoices[j]);
  }
  free(EnumChoices);

  return (0);
}

int ConnectPVs(PVA_OVERALL *pva, PVA_OVERALL *pva_ca, PROG_DATA *prog_data) {
  //Allocate memory for pva structures
  allocPVA(pva, prog_data->total_pvCount);
  allocPVA(pva_ca, prog_data->total_pvCount);
  //List PV names
  epics::pvData::shared_vector<std::string> names(pva->numPVs);
  epics::pvData::shared_vector<std::string> providerNames(pva->numPVs);
  epics::pvData::shared_vector<std::string> providerNames_ca(pva->numPVs);
  for (int32_t p = 0, j = 0; p < prog_data->inputPages; p++) {
    for (int32_t i = 0; i < prog_data->inputPage[p].pvCount; i++, j++) {
      names[j] = prog_data->inputPage[p].ControlName[i];
      providerNames[j] = "pva";
      providerNames_ca[j] = "ca";
    }
  }
  pva->pvaChannelNames = pva_ca->pvaChannelNames = freeze(names);
  pva->pvaProvider = freeze(providerNames);
  pva_ca->pvaProvider = freeze(providerNames_ca);

  ConnectPVA(pva, prog_data->pendIOTime);
  if (pvaThreadSleep(.1) == 1) {
    return (1);
  }

  ConnectPVA(pva_ca, prog_data->pendIOTime);
  if (pvaThreadSleep(.1) == 1) {
    return (1);
  }
  if (GetPVAValues(pva) == 1) {
    return (1);
  }
  if (GetPVAValues(pva_ca) == 1) {
    return (1);
  }
  return (0);
}

int compare_strings(const char *str1, const char *str2) {
    // Handle brace-enclosed strings without variable-length arrays
    if (str1[0] == '{' && str1[strlen(str1) - 1] == '}') {
        std::string temp(str1 + 1, strlen(str1) - 2);
        return temp.compare(str2);
    }

    // Fallback to direct C-string comparison
    return strcmp(str1, str2);
}

int GetPVinfo(PVA_OVERALL *pva_pva, PVA_OVERALL *pva_ca, PROG_DATA *prog_data) {
  PVA_OVERALL *pva=NULL;
  bool validEnum=false;

  prog_data->outputData.pvCount = prog_data->total_pvCount;
  prog_data->outputData.ControlName    =    (char**)malloc(sizeof(char*)    * prog_data->outputData.pvCount);
  prog_data->outputData.Active         =    (char *)malloc(sizeof(char)     * prog_data->outputData.pvCount);
  prog_data->outputData.Provider       =    (char**)malloc(sizeof(char*)    * prog_data->outputData.pvCount);
  prog_data->outputData.IOCHost        =    (char**)malloc(sizeof(char*)    * prog_data->outputData.pvCount);
  prog_data->outputData.ReadAccess     =    (char *)malloc(sizeof(char)     * prog_data->outputData.pvCount);
  prog_data->outputData.WriteAccess    =    (char *)malloc(sizeof(char)     * prog_data->outputData.pvCount);
  prog_data->outputData.StructureID    =    (char**)malloc(sizeof(char*)    * prog_data->outputData.pvCount);
  prog_data->outputData.FieldType      =    (char**)malloc(sizeof(char*)    * prog_data->outputData.pvCount);
  prog_data->outputData.ElementCount   = (uint32_t*)malloc(sizeof(uint32_t) * prog_data->outputData.pvCount);
  prog_data->outputData.NativeDataType =    (char**)malloc(sizeof(char*)    * prog_data->outputData.pvCount);
  prog_data->outputData.Units          =    (char**)malloc(sizeof(char*)    * prog_data->outputData.pvCount);
  prog_data->outputData.EnumNumOfChoices=(uint32_t*)malloc(sizeof(uint32_t) * prog_data->outputData.pvCount);
  prog_data->outputData.EnumChoices    =   (char***)malloc(sizeof(char**)   * prog_data->outputData.pvCount);

  for (int32_t p = 0, j = 0; p < prog_data->inputPages; p++) {
    for (int32_t i = 0; i < prog_data->inputPage[p].pvCount; i++, j++) {
      prog_data->outputData.EnumNumOfChoices[j] = 0;
      SDDS_CopyString(&prog_data->outputData.ControlName[j], prog_data->inputPage[p].ControlName[i]);
      if (pva_pva->isConnected[j])
        pva = pva_pva;
      else
        pva = pva_ca;

      if (pva->isConnected[j])
        prog_data->outputData.Active[j] = 'y';
      else
        prog_data->outputData.Active[j] = 'n';
      SDDS_CopyString(&prog_data->outputData.Provider[j], GetProviderName(pva, j).c_str());
      SDDS_CopyString(&prog_data->outputData.IOCHost[j], GetRemoteAddress(pva,j).c_str());
      if (HaveReadAccess(pva, j))
        prog_data->outputData.ReadAccess[j] = 'y';
      else
        prog_data->outputData.ReadAccess[j] = 'n';
      if (HaveWriteAccess(pva, j))
        prog_data->outputData.WriteAccess[j] = 'y';
      else
        prog_data->outputData.WriteAccess[j] = 'n';
      SDDS_CopyString(&prog_data->outputData.StructureID[j], GetStructureID(pva, j).c_str());
      SDDS_CopyString(&prog_data->outputData.FieldType[j], GetFieldType(pva,j).c_str());
      prog_data->outputData.ElementCount[j] = GetElementCount(pva,j);
      SDDS_CopyString(&prog_data->outputData.NativeDataType[j], GetNativeDataType(pva,j).c_str());
      SDDS_CopyString(&prog_data->outputData.Units[j], GetUnits(pva,j).c_str());
      if (IsEnumFieldType(pva,j)) {
        prog_data->outputData.EnumNumOfChoices[j] = GetEnumChoices(pva, j, &prog_data->outputData.EnumChoices[j]);
      }
      
      if (prog_data->verbose) {
        fprintf(stdout, "%s\n", prog_data->outputData.ControlName[j]);
        fprintf(stdout, "            Active:   %c\n", prog_data->outputData.Active[j]);
        fprintf(stdout, "          Provider:   %s\n", prog_data->outputData.Provider[j]);
        fprintf(stdout, "              Host:   %s\n", prog_data->outputData.IOCHost[j]);
        fprintf(stdout, "       Read Access:   %c\n", prog_data->outputData.ReadAccess[j]);
        fprintf(stdout, "      Write Access:   %c\n", prog_data->outputData.WriteAccess[j]);
        fprintf(stdout, "      Structure ID:   %s\n", prog_data->outputData.StructureID[j]);
        fprintf(stdout, "        Field Type:   %s\n", prog_data->outputData.FieldType[j]);
        fprintf(stdout, "     Element Count:   %" PRIu32 "\n", prog_data->outputData.ElementCount[j]);
        fprintf(stdout, "  Native Data Type:   %s\n", prog_data->outputData.NativeDataType[j]);
        fprintf(stdout, "             Units:   %s\n", prog_data->outputData.Units[j]);
        if (IsEnumFieldType(pva,j)) {
          fprintf(stdout, " Number of Choices:   %" PRIu32 "\n", prog_data->outputData.EnumNumOfChoices[j]);
          for (uint32_t m = 0; m < prog_data->outputData.EnumNumOfChoices[j]; m++) {
            fprintf(stdout, "           Index %" PRIu32 ":   %s\n", m, prog_data->outputData.EnumChoices[j][m]);
          }
        }
        fprintf(stdout, "\n");
      }

      if (IsEnumFieldType(pva,j)) {
        validEnum = false;
        for (uint32_t m = 0; m < prog_data->outputData.EnumNumOfChoices[j]; m++) {
          if (compare_strings(prog_data->outputData.EnumChoices[j][m],pva->pvaData[j].getData[0].stringValues[0]) == 0) {
            validEnum = true;
            break;
          }
        }
        if (validEnum == false) {
          fprintf(stderr, "%s has an invalid ENUM value\n", prog_data->outputData.ControlName[j]);
        }
      }
    }
  }
  return (0);
}

void FreeMemory(PROG_DATA *prog_data) {
  for (int32_t i=0; i<prog_data->inputPages; i++) {
    if (prog_data->inputPage[i].ControlName) {
      for (int64_t j=0; j<prog_data->inputPage[i].pvCount; j++) {
        if (prog_data->inputPage[i].ControlName[j])
          free(prog_data->inputPage[i].ControlName[j]);
      }
      free (prog_data->inputPage[i].ControlName);
    }
  }
  free(prog_data->inputPage);
  for (int64_t i=0; i<prog_data->outputData.pvCount; i++) {
    if (prog_data->outputData.ControlName[i])
      free(prog_data->outputData.ControlName[i]);
    if (prog_data->outputData.Provider[i])
      free(prog_data->outputData.Provider[i]);
    if (prog_data->outputData.IOCHost[i])
      free(prog_data->outputData.IOCHost[i]);
    if (prog_data->outputData.StructureID[i])
      free(prog_data->outputData.StructureID[i]);
    if (prog_data->outputData.FieldType[i])
      free(prog_data->outputData.FieldType[i]);
    if (prog_data->outputData.NativeDataType[i])
      free(prog_data->outputData.NativeDataType[i]);
    if (prog_data->outputData.Units[i])
      free(prog_data->outputData.Units[i]);
    if (prog_data->outputData.EnumNumOfChoices[i] > 0) {
      if (prog_data->outputData.EnumChoices[i]) {
        for (uint32_t j=0; j<prog_data->outputData.EnumNumOfChoices[i]; j++) {
          if (prog_data->outputData.EnumChoices[i][j])
            free(prog_data->outputData.EnumChoices[i][j]);
        }
        free(prog_data->outputData.EnumChoices[i]);
      }
    }

  }
  free(prog_data->outputData.ControlName);
  free(prog_data->outputData.Active);
  free(prog_data->outputData.Provider);
  free(prog_data->outputData.IOCHost);
  free(prog_data->outputData.ReadAccess);
  free(prog_data->outputData.WriteAccess);
  free(prog_data->outputData.StructureID);
  free(prog_data->outputData.FieldType);
  free(prog_data->outputData.ElementCount);
  free(prog_data->outputData.NativeDataType);
  free(prog_data->outputData.Units);
  free(prog_data->outputData.EnumNumOfChoices);
  free(prog_data->outputData.EnumChoices);
  if (prog_data->inputFile)
    free(prog_data->inputFile);
  if (prog_data->outputFile)
    free(prog_data->outputFile);
}

long pvaThreadSleep(long double seconds) {
  struct timespec delayTime;
  struct timespec remainingTime;

  if (seconds > 0) {
    delayTime.tv_sec = seconds;
    delayTime.tv_nsec = (seconds - delayTime.tv_sec) * 1e9;
    while (nanosleep(&delayTime, &remainingTime) == -1 &&
           errno == EINTR)
      delayTime = remainingTime;
  }
  return (0);
}
