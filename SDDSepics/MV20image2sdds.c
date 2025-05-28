/*************************************************************************\
 * Copyright (c) 2002 The University of Chicago, as Operator of Argonne
 * National Laboratory.
 * Copyright (c) 2002 The Regents of the University of California, as
 * Operator of Los Alamos National Laboratory.
 * This file is distributed subject to a Software License Agreement found
 * in the file LICENSE that is included with this distribution. 
\*************************************************************************/

/* program: MV20image2sdds.c
 * purpose: converts a MV20 image file from to SDDS format
 *
 * M. Borland, 1994
 $Log: not supported by cvs2svn $
 Revision 1.1  2003/08/27 19:51:00  soliday
 Moved into subdirectory

 Revision 1.7  2002/08/14 20:00:30  soliday
 Added Open License

 Revision 1.6  2001/05/03 19:53:43  soliday
 Standardized the Usage message.

 Revision 1.5  2000/04/19 15:49:38  soliday
 Removed some solaris compiler warnings.

 Revision 1.4  1999/03/12 18:30:42  borland
 No changes.  Spurious need to commit due to CVS mixing up the commenting
 of update notes.

 * Revision 1.3  1995/11/14  04:35:00  borland
 * Added usleep() substitute for Solaris, along with makefile changes to
 * link these to sdds*experiment.  Corrected nonANSI use of sprintf() in
 * MV20image2sdds.c .
 *
 * Revision 1.2  1995/11/09  03:22:26  borland
 * Added copyright message to source files.
 *
 * Revision 1.1  1995/09/25  20:15:20  saunders
 * First release of SDDSepics applications.
 *
 */
#include "mdb.h"
#include "SDDS.h"
#include "scan.h"
#include "match_string.h"

typedef struct
{
  char image_text[128]; /* header text */
  char user_text[128];  /* optional user text */
  int pixtyp;           /* pixel type */
  int rowsiz;           /* row size */
  int nrows;            /* number of rows */
  int nimages;          /* number of images */
  int level_origin;     /* pixel level origin */
  int col_origin;       /* column origin */
  int row_origin;       /* row origin */
  int plane_origin;     /* plane origin */
  float level_offset;   /* pixel level offset */
  float col_offset;     /* column offset */
  float row_offset;     /* row offset */
  float plane_offset;   /* plane offset */
  float level_scale;    /* pixel level scale */
  float col_scale;      /* column scale */
  float row_scale;      /* row scale */
  float plane_scale;    /* plane scale */
  int image_attr[16];   /* image attributes */
  int user_attr[32];    /* user attributes */
} IMAGE_HEADER;

#define SET_DEFINITION 0
#define SET_VERBOSE 1
#define N_OPTIONS 2

char *option[N_OPTIONS] = {
  "definition", "verbose"};

char *USAGE = "MV20image2sdds -definition=<name>,<definition-entries> <inputfile> <outputfile> [-verbose]\n\n\
MV20image2sdds converts a MV20 image file to SDDS format.  The definition entries\
are of the form <keyword>=<value>, where the keyword is any valid field name for\
a SDDS column.\n\n\
Program by Michael Borland\n\
Link date: "__DATE__
              " "__TIME__
              ", SVN revision: " SVN_VERSION "\n";

char *process_column_definition(char **argv, long argc);

int main(int argc, char **argv) {
  SDDS_TABLE SDDS_table;
  SCANNED_ARG *scanned;
  long i_arg;
  char *input, *output, *definition;
  long hsize, vsize, verbose;
  char *data, *data_name = NULL;
  char ts1[100], ts2[100];
  FILE *fpi;
  IMAGE_HEADER header;

  argc = scanargs(&scanned, argc, argv);
  if (argc < 4)
    bomb(NULL, USAGE);

  input = output = NULL;
  definition = NULL;
  verbose = 0;

  for (i_arg = 1; i_arg < argc; i_arg++) {
    if (scanned[i_arg].arg_type == OPTION) {
      /* process options here */
      switch (match_string(scanned[i_arg].list[0], option, N_OPTIONS, 0)) {
      case SET_DEFINITION:
        data_name = scanned[i_arg].list[1];
        definition = process_column_definition(scanned[i_arg].list + 1,
                                               scanned[i_arg].n_items - 1);
        if (!strstr(definition, "type=character"))
          bomb("data type must be character for now", NULL);
        break;
      case SET_VERBOSE:
        verbose = 1;
        break;
      default:
        bomb("invalid option seen", USAGE);
        break;
      }
    } else {
      if (!input)
        input = scanned[i_arg].list[0];
      else if (!output)
        output = scanned[i_arg].list[0];
      else
        bomb("too many filenames", USAGE);
    }
  }
  if (!input)
    bomb("input file not seen", NULL);
  if (!output)
    bomb("output file not seen", NULL);
  if (!definition)
    bomb("definition not seen", NULL);

  fpi = fopen_e(input, "r", 0);
  if (fread(&header, sizeof(header), 1, fpi) != 1)
    bomb("unable to read file header", NULL);
  if (verbose) {
    printf("image_text: %s\n", header.image_text);
    printf("user_text: %s\n", header.user_text);
    printf("%d rows, %d columns\n", header.nrows, header.rowsiz);
  }
  hsize = header.nrows;
  vsize = header.rowsiz;

  sprintf(ts1, "%ld", hsize);
  sprintf(ts2, "%ld", vsize);
  if (!SDDS_InitializeOutput(&SDDS_table, SDDS_BINARY, 0,
                             "screen image from MV20 file", "screen image",
                             output) ||
      SDDS_ProcessColumnString(&SDDS_table, definition, 0) < 0 ||
      SDDS_DefineParameter(&SDDS_table, "NumberOfRows", NULL, NULL, "number of rows",
                           NULL, SDDS_LONG, ts1) < 0 ||
      SDDS_DefineParameter(&SDDS_table, "NumberOfColumns", NULL, NULL,
                           "number of columns", NULL, SDDS_LONG, ts2) < 0 ||
      !SDDS_WriteLayout(&SDDS_table))
    SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);

  data = tmalloc(sizeof(*data) * hsize * vsize);
  if (fread(data, sizeof(*data), hsize * vsize, fpi) != hsize * vsize)
    bomb("unable to read (all) data from input file", NULL);
  if (!SDDS_StartTable(&SDDS_table, hsize * vsize) ||
      !SDDS_SetColumn(&SDDS_table, SDDS_SET_BY_NAME, data, hsize * vsize,
                      data_name) ||
      !SDDS_WriteTable(&SDDS_table))
    SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
  fclose(fpi);
  if (!SDDS_Terminate(&SDDS_table))
    SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
  return 0;
}

char *process_column_definition(char **argv, long argc) {
  char buffer[SDDS_MAXLINE], *ptr;
  long i;

  if (argc < 1)
    return (NULL);
  sprintf(buffer, "&column name=%s, ", argv[0]);
  for (i = 1; i < argc; i++) {
    if (!strchr(argv[i], '='))
      return (NULL);
    strcat(buffer, argv[i]);
    strcat(buffer, ", ");
  }
  if (!strstr(buffer, "type="))
    strcat(buffer, "type=character ");
  strcat(buffer, "&end");
  cp_str(&ptr, buffer);
  return (ptr);
}
