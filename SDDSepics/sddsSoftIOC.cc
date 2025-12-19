/*************************************************************************\
 * Copyright (c) 2002 The University of Chicago, as Operator of Argonne
 *     National Laboratory.
 * Copyright (c) 2002 The Regents of the University of California, as
 *     Operator of Los Alamos National Laboratory.
 * EPICS BASE Versions 3.13.7
 * and higher are distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
\*************************************************************************/

#include <errno.h>
#include <float.h>
#include <math.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>

#include "cadef.h"
#include "envDefs.h"
#include "epicsTime.h"
#include "mdb.h"
#include "scan.h"
#include "SDDS.h"

#define SET_DEBUGLEVEL 0
#define SET_EXECUTIONTIME 1
#define SET_PVPREFIX 2
#define SET_NOISE 3
#define SET_ARRAYSIZE 4
#define SET_EPICSBASE 5
#define SET_STANDALONE 6
#define SET_MASTERPVFILE 7
#define SET_PCASPVFILE 8
#define SET_HELP 9
#define N_OPTIONS 10

static char *option[N_OPTIONS] = {
  (char *)"debuglevel", (char *)"executiontime",
  (char *)"pvprefix", (char *)"noise",
  (char *)"arraysize", (char *)"epicsbase",
  (char *)"standalone", (char *)"masterpvfile",
  (char *)"pcaspvfile", (char *)"help"};

static char *USAGE = (char *)"sddsSoftIOC <inputfiles>\n\
[-masterPVFile=<filename>]\n\
[-pcasPVFile=<filename>]\n\
[-standalone]\n\
[-debugLevel=<debug level>]\n\
[-executionTime=<seconds>] (default=12 hours, 0=forever)\n\
[-pvPrefix=<PV name prefix>]\n\
[-noise=<rateSeconds>]\n\
[-arraySize=<bytes>] (sets EPICS_CA_MAX_ARRAY_BYTES)\n\
[-epicsBase=<path>]\n\
[-help] | [-h]\n\
\
Creates a temporary EPICS soft IOC (EPICS Base) that serves the same PV names\n\
/types as sddspcas, based on the same SDDS input files. The generated IOC\n\
DB files are created at runtime and removed on exit.\n";

/*
 * Input file format:
 * Each <inputfile> is an SDDS file describing PVs, with one row per PV.
 *
 * Required columns:
 *   - ControlName   (string): base PV name (pvPrefix is prepended; name is truncated per sddspcas rules).
 *
 * Optional columns:
 *   - Type          (string): one of char/uchar/short/ushort/int/uint/float/double/string/enum (default: double).
 *   - ElementCount  (integer): number of elements for waveform PVs (default: 1).
 *   - ReadbackUnits (string): engineering units (default: single space).
 *   - Hopr, Lopr    (numeric): display range (defaults: very large/small; swapped if Hopr < Lopr).
 *   - EnumStrings   (string): required only when Type=enum; comma-separated list of up to 16 state strings.
 *
 * Constraints:
 *   - PV names must not contain '.' (extensions are not allowed).
 *   - Type=string must have ElementCount=1 (scalar; emitted as a stringout record).
 *   - Type=enum must have ElementCount=1 and EnumStrings must not contain quotes; empty states are not allowed.
 */

struct PVDef {
  std::string controlName;
  std::string pvName;  // includes pvPrefix + truncation rules
  std::string type;    // original SDDS string (char/uchar/short/ushort/int/uint/float/double/string)
  std::string units;
  double hopr;
  double lopr;
  uint32_t elementCount;
  std::vector<std::string> enumStates; /* only used when type=="enum" */
};

static volatile sig_atomic_t gTerminate = 0;
static pid_t gIocPid = -1;
static pid_t gEqPid = -1;
static std::string gTempDir;
static std::string gDbPath;
static std::string gPcasPvFile;
static std::string gExecutableDir;

static std::string resolveAbsolutePath(const std::string &path);
static std::string resolveAbsolutePathFromBaseDir(const std::string &path, const std::string &baseDir);
static std::string getExecutableDir(const char *argv0);
static std::string findDefaultEpicsBase();

static std::string trimWhitespace(const std::string &in) {
  size_t first = 0;
  while (first < in.size() && (in[first] == ' ' || in[first] == '\t' || in[first] == '\r' || in[first] == '\n')) {
    first++;
  }
  size_t last = in.size();
  while (last > first && (in[last - 1] == ' ' || in[last - 1] == '\t' || in[last - 1] == '\r' || in[last - 1] == '\n')) {
    last--;
  }
  return in.substr(first, last - first);
}

static void parseEnumStatesCsv(const std::string &pvName, const std::string &csv, std::vector<std::string> &outStates) {
  outStates.clear();

  /* Do not allow quoting/escaping. */
  if (csv.find('"') != std::string::npos || csv.find('\'') != std::string::npos) {
    fprintf(stderr, "error: EnumStrings for %s must not contain quotes\n", pvName.c_str());
    exit(1);
  }

  size_t start = 0;
  while (start <= csv.size()) {
    size_t comma = csv.find(',', start);
    std::string token;
    if (comma == std::string::npos) {
      token = csv.substr(start);
      start = csv.size() + 1;
    } else {
      token = csv.substr(start, comma - start);
      start = comma + 1;
    }

    token = trimWhitespace(token);
    if (token.empty()) {
      fprintf(stderr, "error: EnumStrings for %s contains an empty state (consecutive commas or leading/trailing comma)\n", pvName.c_str());
      exit(1);
    }
    outStates.push_back(token);
  }

  if (outStates.size() < 1) {
    fprintf(stderr, "error: EnumStrings for %s must specify at least one state\n", pvName.c_str());
    exit(1);
  }
  if (outStates.size() > 16) {
    fprintf(stderr, "error: EnumStrings for %s specifies %lu states; EPICS mbbo supports up to 16\n", pvName.c_str(), (unsigned long)outStates.size());
    exit(1);
  }
}

static void printUsage() {
  std::string defaultBase = findDefaultEpicsBase();
  fprintf(stderr, "%s", USAGE);
  fprintf(stderr, "Default -epicsBase resolves to: %s\n", defaultBase.c_str());
}

static std::string resolveAbsolutePath(const std::string &path) {
  if (path.empty()) {
    return path;
  }
  char resolved[PATH_MAX];
  if (realpath(path.c_str(), resolved)) {
    return std::string(resolved);
  }
  return path;
}

static bool isAbsolutePath(const std::string &path) {
  return !path.empty() && path[0] == '/';
}

static std::string dirnameOfPath(const std::string &path) {
  size_t slash = path.find_last_of('/');
  if (slash == std::string::npos) {
    return std::string();
  }
  if (slash == 0) {
    return std::string("/");
  }
  return path.substr(0, slash);
}

static std::string getCwdPath() {
  char buf[PATH_MAX];
  if (getcwd(buf, sizeof(buf))) {
    return std::string(buf);
  }
  return std::string();
}

static std::string lexicallyNormalizeAbsolutePath(const std::string &path) {
  if (!isAbsolutePath(path)) {
    return path;
  }

  std::vector<std::string> parts;
  size_t i = 0;
  while (i < path.size()) {
    while (i < path.size() && path[i] == '/') {
      i++;
    }
    if (i >= path.size()) {
      break;
    }
    size_t j = i;
    while (j < path.size() && path[j] != '/') {
      j++;
    }
    std::string token = path.substr(i, j - i);
    if (token == "." || token.empty()) {
      // skip
    } else if (token == "..") {
      if (!parts.empty()) {
        parts.pop_back();
      }
    } else {
      parts.push_back(token);
    }
    i = j;
  }

  std::string out = "/";
  for (size_t k = 0; k < parts.size(); k++) {
    out += parts[k];
    if (k + 1 < parts.size()) {
      out += "/";
    }
  }
  return out;
}

static std::string resolveAbsolutePathFromBaseDir(const std::string &path, const std::string &baseDir) {
  if (path.empty()) {
    return path;
  }

  std::string effectiveBase = baseDir;
  if (effectiveBase.empty()) {
    effectiveBase = getCwdPath();
  }
  if (!isAbsolutePath(effectiveBase)) {
    std::string cwd = getCwdPath();
    if (!cwd.empty()) {
      effectiveBase = cwd + "/" + effectiveBase;
    }
  }

  std::string candidate;
  if (isAbsolutePath(path)) {
    candidate = path;
  } else {
    if (!effectiveBase.empty() && effectiveBase[effectiveBase.size() - 1] == '/') {
      candidate = effectiveBase + path;
    } else {
      candidate = effectiveBase + "/" + path;
    }
  }

  candidate = lexicallyNormalizeAbsolutePath(candidate);

  char resolved[PATH_MAX];
  if (realpath(candidate.c_str(), resolved)) {
    return std::string(resolved);
  }
  return candidate;
}

static bool isDirectory(const std::string &path) {
  struct stat st;
  if (stat(path.c_str(), &st) != 0) {
    return false;
  }
  return S_ISDIR(st.st_mode);
}

static bool isExecutableFile(const std::string &path) {
  return access(path.c_str(), X_OK) == 0;
}

static std::string findDefaultEpicsBase() {
  std::vector<std::string> candidates;

  const char *env = getenv("EPICS_BASE");
  if (env && env[0]) {
    candidates.push_back(resolveAbsolutePath(std::string(env)));
  }

  candidates.push_back(std::string("/usr/local/oag/base"));

  /*
   * Match the Makefile search path as closely as practical.
   * Prefer EPICS Base checkouts next to the SDDSepics repo, then fall back to $HOME.
   */
  std::string binDir = dirnameOfPath(gExecutableDir);
  std::string sddsEpicsRoot;
  if (!binDir.empty() && binDir.substr(binDir.find_last_of('/') + 1) == "bin") {
    sddsEpicsRoot = dirnameOfPath(binDir);
  }
  if (!sddsEpicsRoot.empty()) {
    candidates.push_back(resolveAbsolutePathFromBaseDir("../epics-base", sddsEpicsRoot));
  }

  const char *home = getenv("HOME");
  if (home && home[0]) {
    candidates.push_back(resolveAbsolutePath(std::string(home) + "/epics/base"));
  }

  /* Original historical default */
  candidates.push_back(resolveAbsolutePathFromBaseDir("../epics-base", getCwdPath()));

  for (size_t i = 0; i < candidates.size(); i++) {
    if (candidates[i].empty()) {
      continue;
    }
    if (!isDirectory(candidates[i])) {
      continue;
    }
    /* startSoftIoc and getHostArchFromEpicsBase require this script */
    if (!isExecutableFile(candidates[i] + "/startup/EpicsHostArch")) {
      continue;
    }
    return candidates[i];
  }

  /* Fall back to a predictable absolute path even if it doesn't exist */
  if (!sddsEpicsRoot.empty()) {
    return resolveAbsolutePathFromBaseDir("../epics-base", sddsEpicsRoot);
  }
  return resolveAbsolutePathFromBaseDir("../epics-base", getCwdPath());
}

static std::string getExecutableDir(const char *argv0) {
  char resolved[PATH_MAX];
  ssize_t len = readlink("/proc/self/exe", resolved, sizeof(resolved) - 1);
  if (len > 0) {
    resolved[len] = '\0';
    return dirnameOfPath(std::string(resolved));
  }

  if (argv0 && argv0[0]) {
    std::string p = std::string(argv0);
    if (!isAbsolutePath(p) && p.find('/') != std::string::npos) {
      std::string cwd = getCwdPath();
      if (!cwd.empty()) {
        p = cwd + "/" + p;
      }
    }
    std::string d = dirnameOfPath(p);
    if (!d.empty()) {
      return d;
    }
  }

  return getCwdPath();
}

static void signal_handler(int /*sig*/) {
  gTerminate = 1;
}

static void safeKill(pid_t pid, int sig) {
  if (pid > 0) {
    kill(pid, sig);
  }
}

static void cleanupTemp() {
  if (!gDbPath.empty()) {
    unlink(gDbPath.c_str());
  }
  if (!gTempDir.empty()) {
    rmdir(gTempDir.c_str());
  }
}

static std::string getHostArchFromEpicsBase(const std::string &epicsBase) {
  const char *env = getenv("EPICS_HOST_ARCH");
  if (env && env[0]) {
    return std::string(env);
  }

  std::string script = epicsBase + "/startup/EpicsHostArch";
  FILE *fp = popen(script.c_str(), "r");
  if (!fp) {
    return std::string();
  }
  char buf[256];
  if (!fgets(buf, (int)sizeof(buf), fp)) {
    pclose(fp);
    return std::string();
  }
  pclose(fp);
  size_t len = strlen(buf);
  while (len && (buf[len - 1] == '\n' || buf[len - 1] == '\r')) {
    buf[--len] = 0;
  }
  return std::string(buf);
}

static std::string formatPvName(const std::string &pvPrefix, const std::string &controlName) {
  char pvAlias[256];
  // Match sddspcas naming: prefix truncated to 100, control name to 40
  snprintf(pvAlias, sizeof(pvAlias), "%.100s%.40s", pvPrefix.c_str(), controlName.c_str());
  return std::string(pvAlias);
}

static std::string formatPvName20(const std::string &pvPrefix, const std::string &controlName) {
  char pvAlias[256];
  // Match sddspcas master-file formatting: prefix truncated to 100, control name to 20
  snprintf(pvAlias, sizeof(pvAlias), "%.100s%.20s", pvPrefix.c_str(), controlName.c_str());
  return std::string(pvAlias);
}

static const char *ftvlFromType(const std::string &type) {
  if (type == "char") {
    return "CHAR";
  } else if (type == "uchar") {
    return "UCHAR";
  } else if (type == "short") {
    return "SHORT";
  } else if (type == "ushort") {
    return "USHORT";
  } else if (type == "int") {
    return "LONG";
  } else if (type == "uint") {
    return "ULONG";
  } else if (type == "float") {
    return "FLOAT";
  } else if (type == "double") {
    return "DOUBLE";
  } else if (type == "string") {
    return "STRING";
  } else if (type == "enum") {
    /* Not used for enum PVs (they become mbbo records), but keep a stable default. */
    return "SHORT";
  }
  // sddspcas default
  return "DOUBLE";
}

static void readPvInputFiles(const std::vector<std::string> &inputFiles, const std::string &pvPrefix, std::vector<PVDef> &out) {
  SDDS_DATASET SDDS_input;

  for (size_t i = 0; i < inputFiles.size(); i++) {
    int hoprFound = 0, loprFound = 0, unitsFound = 0, elementCountFound = 0, typeFound = 0, enumStringsFound = 0;
    uint32_t rows;

    if (!SDDS_InitializeInput(&SDDS_input, (char *)inputFiles[i].c_str())) {
      fprintf(stderr, "error: Unable to read SDDS input file %s\n", inputFiles[i].c_str());
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      exit(1);
    }

    if (SDDS_VerifyColumnExists(&SDDS_input, FIND_SPECIFIED_TYPE, SDDS_STRING, "ControlName") == -1) {
      fprintf(stderr, "error: string column ControlName does not exist\n");
      exit(1);
    }
    if (SDDS_VerifyColumnExists(&SDDS_input, FIND_NUMERIC_TYPE, (char *)"Hopr") >= 0) {
      hoprFound = 1;
    }
    if (SDDS_VerifyColumnExists(&SDDS_input, FIND_NUMERIC_TYPE, (char *)"Lopr") >= 0) {
      loprFound = 1;
    }
    if (SDDS_VerifyColumnExists(&SDDS_input, FIND_SPECIFIED_TYPE, SDDS_STRING, "ReadbackUnits") >= 0) {
      unitsFound = 1;
    }
    if (SDDS_VerifyColumnExists(&SDDS_input, FIND_INTEGER_TYPE, (char *)"ElementCount") >= 0) {
      elementCountFound = 1;
    }
    if (SDDS_VerifyColumnExists(&SDDS_input, FIND_SPECIFIED_TYPE, SDDS_STRING, "Type") >= 0) {
      typeFound = 1;
    }
    if (SDDS_VerifyColumnExists(&SDDS_input, FIND_SPECIFIED_TYPE, SDDS_STRING, "EnumStrings") >= 0) {
      enumStringsFound = 1;
    }

    if (SDDS_ReadPage(&SDDS_input) != 1) {
      fprintf(stderr, "error: Unable to read SDDS file %s\n", inputFiles[i].c_str());
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      exit(1);
    }

    if ((rows = (uint32_t)SDDS_RowCount(&SDDS_input)) < 1) {
      fprintf(stderr, "error: No rows found in SDDS file %s\n", inputFiles[i].c_str());
      exit(1);
    }

    char **cn = (char **)SDDS_GetColumn(&SDDS_input, (char *)"ControlName");
    if (!cn) {
      fprintf(stderr, "error: Unable to read ControlName column from %s\n", inputFiles[i].c_str());
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      exit(1);
    }

    double *ho = NULL;
    double *lo = NULL;
    char **ru = NULL;
    uint32_t *ec = NULL;
    char **ty = NULL;
    char **es = NULL;

    if (hoprFound) {
      ho = SDDS_GetColumnInDoubles(&SDDS_input, (char *)"Hopr");
      if (!ho) {
        fprintf(stderr, "error: Unable to read Hopr column from %s\n", inputFiles[i].c_str());
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        exit(1);
      }
    }
    if (loprFound) {
      lo = SDDS_GetColumnInDoubles(&SDDS_input, (char *)"Lopr");
      if (!lo) {
        fprintf(stderr, "error: Unable to read Lopr column from %s\n", inputFiles[i].c_str());
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        exit(1);
      }
    }
    if (unitsFound) {
      ru = (char **)SDDS_GetColumn(&SDDS_input, (char *)"ReadbackUnits");
      if (!ru) {
        fprintf(stderr, "error: Unable to read ReadbackUnits column from %s\n", inputFiles[i].c_str());
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        exit(1);
      }
    }
    if (elementCountFound) {
      ec = (uint32_t *)SDDS_GetColumnInLong(&SDDS_input, (char *)"ElementCount");
      if (!ec) {
        fprintf(stderr, "error: Unable to read ElementCount column from %s\n", inputFiles[i].c_str());
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        exit(1);
      }
    }
    if (typeFound) {
      ty = (char **)SDDS_GetColumn(&SDDS_input, (char *)"Type");
      if (!ty) {
        fprintf(stderr, "error: Unable to read Type column from %s\n", inputFiles[i].c_str());
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        exit(1);
      }
    }
    if (enumStringsFound) {
      es = (char **)SDDS_GetColumn(&SDDS_input, (char *)"EnumStrings");
      if (!es) {
        fprintf(stderr, "error: Unable to read EnumStrings column from %s\n", inputFiles[i].c_str());
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        exit(1);
      }
    }

    for (uint32_t j = 0; j < rows; j++) {
      PVDef pv;
      pv.controlName = cn[j] ? cn[j] : "";
      pv.pvName = formatPvName(pvPrefix, pv.controlName);
      pv.hopr = hoprFound ? ho[j] : DBL_MAX;
      pv.lopr = loprFound ? lo[j] : -DBL_MAX;
      pv.units = unitsFound ? (ru[j] ? ru[j] : " ") : " ";
      pv.elementCount = elementCountFound ? ec[j] : 1u;
      pv.type = typeFound ? (ty[j] ? ty[j] : "double") : "double";

      if (pv.type == "enum") {
        if (!enumStringsFound || !es || !es[j]) {
          fprintf(stderr, "error: PV %s has Type=enum but EnumStrings is missing or NULL\n", pv.pvName.c_str());
          exit(1);
        }
        parseEnumStatesCsv(pv.pvName, es[j], pv.enumStates);
        if (pv.elementCount != 1u) {
          fprintf(stderr, "error: PV %s has Type=enum but ElementCount!=1 (ElementCount=%u)\n", pv.pvName.c_str(), pv.elementCount);
          exit(1);
        }
      }

      if (pv.pvName.find('.') != std::string::npos) {
        fprintf(stderr, "error: PV extensions are not allowed (%s)\n", pv.pvName.c_str());
        exit(1);
      }

      if (pv.type == "string" && pv.elementCount > 1) {
        fprintf(stderr, "Multiple elements with String PVs are not supported in sddspcas (and therefore not in sddsSoftIOC)\n");
        exit(1);
      }

      // Match sddspcas behavior: swap if HOPR < LOPR
      if (pv.hopr < pv.lopr) {
        double tmp = pv.lopr;
        pv.lopr = pv.hopr;
        pv.hopr = tmp;
      }

      out.push_back(pv);
    }

    for (uint32_t j = 0; j < rows; j++) {
      free(cn[j]);
      if (ru) {
        free(ru[j]);
      }
      if (ty) {
        free(ty[j]);
      }
      if (es) {
        free(es[j]);
      }
    }
    free(cn);
    if (ru) {
      free(ru);
    }
    if (ty) {
      free(ty);
    }
    if (es) {
      free(es);
    }
    if (ho) {
      free(ho);
    }
    if (lo) {
      free(lo);
    }
    if (ec) {
      free(ec);
    }

    SDDS_Terminate(&SDDS_input);
  }
}

static void writeDbFile(const std::string &dbPath, const std::vector<PVDef> &pvs) {
  std::ofstream out(dbPath.c_str());
  if (!out.is_open()) {
    fprintf(stderr, "error: Unable to create db file %s: %s\n", dbPath.c_str(), strerror(errno));
    exit(1);
  }

  static const char *enumStateField[16] = {
    "ZRST", "ONST", "TWST", "THST", "FRST", "FVST", "SXST", "SVST",
    "EIST", "NIST", "TEST", "ELST", "TVST", "TTST", "FTST", "FFST"};
  static const char *enumValueField[16] = {
    "ZRVL", "ONVL", "TWVL", "THVL", "FRVL", "FVVL", "SXVL", "SVVL",
    "EIVL", "NIVL", "TEVL", "ELVL", "TVVL", "TTVL", "FTVL", "FFVL"};

  auto nobtForEnumCount = [](size_t count) -> unsigned int {
    if (count <= 1) {
      return 1;
    }
    unsigned int bits = 0;
    unsigned int maxValue = (unsigned int)(count - 1);
    while (maxValue) {
      bits++;
      maxValue >>= 1;
    }
    if (bits < 1) {
      bits = 1;
    }
    return bits;
  };

  auto escapeDbString = [](const std::string &in) -> std::string {
    std::string out;
    out.reserve(in.size());
    for (size_t i = 0; i < in.size(); i++) {
      char c = in[i];
      if (c == '\\' || c == '"') {
        out.push_back('\\');
      }
      out.push_back(c);
    }
    return out;
  };

  for (size_t i = 0; i < pvs.size(); i++) {
    const PVDef &pv = pvs[i];

    if (pv.type == "enum") {
      if (pv.enumStates.empty()) {
        fprintf(stderr, "error: internal: PV %s has Type=enum but no parsed enum states\n", pv.pvName.c_str());
        exit(1);
      }
      if (pv.enumStates.size() > 16) {
        fprintf(stderr, "error: internal: PV %s has %lu enum states; max 16\n", pv.pvName.c_str(), (unsigned long)pv.enumStates.size());
        exit(1);
      }

      out << "record(mbbo, \"" << pv.pvName << "\") {\n";
      out << "  field(NOBT, \"" << nobtForEnumCount(pv.enumStates.size()) << "\")\n";
      for (size_t s = 0; s < pv.enumStates.size(); s++) {
        out << "  field(" << enumStateField[s] << ", \"" << escapeDbString(pv.enumStates[s]) << "\")\n";
        out << "  field(" << enumValueField[s] << ", \"" << s << "\")\n";
      }
      out << "}\n\n";
      continue;
    }

    if (pv.type == "string") {
      /*
       * EPICS waveform records do not reliably support FTVL=STRING across EPICS Base versions.
       * Since sddspcas only supports scalar string PVs, emit a scalar string record.
       */
      out << "record(stringout, \"" << pv.pvName << "\") {\n";
      out << "  field(VAL, \"\")\n";
      out << "}\n\n";
      continue;
    }

    out << "record(waveform, \"" << pv.pvName << "\") {\n";
    out << "  field(FTVL, \"" << ftvlFromType(pv.type) << "\")\n";
    out << "  field(NELM, \"" << pv.elementCount << "\")\n";
    out << "  field(PREC, \"4\")\n";
    out << "  field(EGU, \"" << pv.units << "\")\n";
    out << "  field(HOPR, \"" << pv.hopr << "\")\n";
    out << "  field(LOPR, \"" << pv.lopr << "\")\n";
    out << "}\n\n";
  }
}

static void startSoftIoc(const std::string &epicsBase, const std::string &dbPath) {
  std::string hostArch = getHostArchFromEpicsBase(epicsBase);
  if (hostArch.empty()) {
    fprintf(stderr, "error: Unable to determine EPICS_HOST_ARCH (set EPICS_HOST_ARCH or verify %s/startup/EpicsHostArch is executable)\n", epicsBase.c_str());
    exit(1);
  }

  std::string softIoc = epicsBase + "/bin/" + hostArch + "/softIoc";

  pid_t pid = fork();
  if (pid < 0) {
    fprintf(stderr, "error: fork() failed: %s\n", strerror(errno));
    exit(1);
  }

  if (pid == 0) {
    // Child: exec softIoc
    execl(softIoc.c_str(), softIoc.c_str(), "-S", "-d", dbPath.c_str(), (char *)NULL);
    fprintf(stderr, "error: exec %s failed: %s\n", softIoc.c_str(), strerror(errno));
    _exit(1);
  }

  gIocPid = pid;
}

static void startEquationsHelper(const std::vector<std::string> &inputFiles, int debugLevel) {
  pid_t pid = fork();
  if (pid < 0) {
    fprintf(stderr, "warning: Unable to fork sddspcasEquations helper\n");
    return;
  }
  if (pid == 0) {
    std::string fileNames;
    for (size_t i = 0; i < inputFiles.size(); i++) {
      fileNames += inputFiles[i];
      if (i + 1 < inputFiles.size()) {
        fileNames += ",";
      }
    }

    if (debugLevel > 0) {
      execlp("sddspcasEquations", "sddspcasEquations", "-debug", "1", "-input", fileNames.c_str(), (char *)NULL);
    } else {
      execlp("sddspcasEquations", "sddspcasEquations", "-input", fileNames.c_str(), (char *)NULL);
    }

    fprintf(stderr, "warning: Error executing sddspcasEquations helper (not fatal)\n");
    _exit(1);
  }
  gEqPid = pid;
}

static void caInit() {
  int status = ca_context_create(ca_disable_preemptive_callback);
  if (status != ECA_NORMAL) {
    fprintf(stderr, "error: ca_context_create failed: %s\n", ca_message(status));
    exit(1);
  }
}

static void caFini() {
  ca_context_destroy();
}

static void waitForPvConnect(const std::string &pvName, double timeoutSeconds) {
  chid ch;
  int status = ca_create_channel(pvName.c_str(), NULL, NULL, 0, &ch);
  if (status != ECA_NORMAL) {
    fprintf(stderr, "warning: ca_create_channel(%s) failed: %s\n", pvName.c_str(), ca_message(status));
    return;
  }
  status = ca_pend_io(timeoutSeconds);
  if (status != ECA_NORMAL) {
    fprintf(stderr, "warning: Timed out waiting for IOC PV %s to connect\n", pvName.c_str());
  }
  ca_clear_channel(ch);
  ca_flush_io();
}

struct NoiseChannel {
  chid ch;
  unsigned long elementCount;
  double hopr;
  double lopr;
  bool enabled;
};

static void initNoiseChannels(const std::vector<PVDef> &pvs, std::vector<NoiseChannel> &channels, double connectTimeoutSeconds) {
  channels.clear();
  channels.resize(pvs.size());

  for (size_t i = 0; i < pvs.size(); i++) {
    channels[i].ch = NULL;
    channels[i].enabled = false;
    channels[i].elementCount = 0;
    channels[i].hopr = 0.0;
    channels[i].lopr = 0.0;

    const PVDef &pv = pvs[i];
    if (pv.type != "float" && pv.type != "double") {
      continue;
    }
    if (pv.elementCount == 0) {
      continue;
    }

    chid ch;
    int status = ca_create_channel(pv.pvName.c_str(), NULL, NULL, 0, &ch);
    if (status != ECA_NORMAL) {
      continue;
    }

    channels[i].ch = ch;
    channels[i].elementCount = (unsigned long)pv.elementCount;
    channels[i].hopr = pv.hopr;
    channels[i].lopr = pv.lopr;
    channels[i].enabled = true;
  }

  /* Drive connection state for all created channels */
  if (connectTimeoutSeconds > 0) {
    ca_pend_io(connectTimeoutSeconds);
  } else {
    ca_pend_event(0.0);
  }
}

static void clearNoiseChannels(std::vector<NoiseChannel> &channels) {
  for (size_t i = 0; i < channels.size(); i++) {
    if (channels[i].ch) {
      ca_clear_channel(channels[i].ch);
      channels[i].ch = NULL;
    }
  }
  ca_flush_io();
  channels.clear();
}

static double clamp(double value, double lo, double hi) {
  if (value > hi) {
    return hi;
  }
  if (value < lo) {
    return lo;
  }
  return value;
}

static void doNoiseUpdate(const std::vector<NoiseChannel> &channels) {
  // Match sddspcas noise magnitude: sin(radians)/10
  double radians = (rand() * 2.0 * 3.14159265358979323846) / (double)RAND_MAX;
  double delta = sin(radians) / 10.0;

  for (size_t i = 0; i < channels.size(); i++) {
    const NoiseChannel &nc = channels[i];
    if (!nc.enabled || !nc.ch) {
      continue;
    }
    if (ca_state(nc.ch) != cs_conn) {
      continue;
    }

    unsigned long count = nc.elementCount;
    std::vector<double> values(count);

    int status = ca_array_get(DBR_DOUBLE, count, nc.ch, values.data());
    if (status == ECA_NORMAL) {
      status = ca_pend_io(0.2);
    }

    if (status == ECA_NORMAL) {
      for (unsigned long j = 0; j < count; j++) {
        values[j] = clamp(values[j] + delta, nc.lopr, nc.hopr);
      }
      status = ca_array_put(DBR_DOUBLE, count, nc.ch, values.data());
      if (status == ECA_NORMAL) {
        ca_flush_io();
      }
    }
  }
}

static double nowSeconds() {
  epicsTimeStamp ts;
  if (epicsTimeGetCurrent(&ts) != epicsTimeOK) {
    return 0.0;
  }
  return (double)ts.secPastEpoch + ((double)ts.nsec) / 1e9;
}

static void readMasterPvFile(const std::string &masterPvFile, const std::string &pvPrefix, const std::vector<PVDef> &pvs) {
  SDDS_DATASET SDDS_masterlist;
  long rows;
  char **rec_name = NULL;

  if (!SDDS_InitializeInput(&SDDS_masterlist, (char *)masterPvFile.c_str())) {
    fprintf(stderr, "error: Unable to read SDDS file %s\n", masterPvFile.c_str());
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(1);
  }
  if (SDDS_ReadPage(&SDDS_masterlist) != 1) {
    fprintf(stderr, "error: Unable to read SDDS file %s\n", masterPvFile.c_str());
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(1);
  }
  if ((rows = SDDS_RowCount(&SDDS_masterlist)) < 0) {
    fprintf(stderr, "error: No rows found in SDDS file %s\n", masterPvFile.c_str());
    exit(1);
  }
  if (rows == 0) {
    SDDS_Terminate(&SDDS_masterlist);
    return;
  }
  if (!(rec_name = (char **)SDDS_GetColumn(&SDDS_masterlist, (char *)"rec_name"))) {
    fprintf(stderr, "error: Unable to read SDDS file %s\n", masterPvFile.c_str());
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(1);
  }
  SDDS_Terminate(&SDDS_masterlist);

  for (size_t n = 0; n < pvs.size(); n++) {
    std::string pvAlias = formatPvName20(pvPrefix, pvs[n].controlName);
    if (pvAlias.find('.') != std::string::npos) {
      fprintf(stderr, "error: PV extensions are not allowed (%s)\n", pvAlias.c_str());
      exit(1);
    }
    for (int m = 0; m < rows; m++) {
      if (strcmp(pvAlias.c_str(), rec_name[m]) == 0) {
        fprintf(stderr, "error: %s already exists in %s\n", pvAlias.c_str(), masterPvFile.c_str());
        exit(1);
      }
    }
  }

  for (int m = 0; m < rows; m++) {
    free(rec_name[m]);
  }
  free(rec_name);
}

static void readMasterSddspcasPvFile(const std::string &pcasPvFile, const std::string &pvPrefix, const std::vector<PVDef> &pvs) {
  SDDS_DATASET SDDS_masterlist;
  long rows;
  char **rec_name = NULL;
  char **host_name = NULL;
  char hostname[255];

  if (!SDDS_InitializeInput(&SDDS_masterlist, (char *)pcasPvFile.c_str())) {
    fprintf(stderr, "error: Unable to read SDDS file %s\n", pcasPvFile.c_str());
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(1);
  }
  if (SDDS_ReadPage(&SDDS_masterlist) != 1) {
    fprintf(stderr, "error: Unable to read SDDS file %s\n", pcasPvFile.c_str());
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(1);
  }

  rows = SDDS_RowCount(&SDDS_masterlist);
  if (rows > 0) {
    if (!(rec_name = (char **)SDDS_GetColumn(&SDDS_masterlist, (char *)"rec_name"))) {
      fprintf(stderr, "error: Unable to read SDDS file %s\n", pcasPvFile.c_str());
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      exit(1);
    }
    if (!(host_name = (char **)SDDS_GetColumn(&SDDS_masterlist, (char *)"host_name"))) {
      fprintf(stderr, "error: Unable to read SDDS file %s\n", pcasPvFile.c_str());
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      exit(1);
    }
  }
  SDDS_Terminate(&SDDS_masterlist);

  if (rows > 0) {
    gethostname(hostname, 255);
    for (int m = 0; m < rows; m++) {
      if (strcmp(host_name[m], hostname) == 0) {
        fprintf(stderr, "error: %s listed as a hostname already running sddspcas in %s\n", hostname, pcasPvFile.c_str());
        exit(1);
      }
    }
    for (size_t n = 0; n < pvs.size(); n++) {
      std::string pvAlias = formatPvName20(pvPrefix, pvs[n].controlName);
      if (pvAlias.find('.') != std::string::npos) {
        fprintf(stderr, "error: PV extensions are not allowed (%s)\n", pvAlias.c_str());
        exit(1);
      }
      for (int m = 0; m < rows; m++) {
        if (strcmp(pvAlias.c_str(), rec_name[m]) == 0) {
          fprintf(stderr, "error: %s already exists in %s\n", pvAlias.c_str(), pcasPvFile.c_str());
          exit(1);
        }
      }
    }

    for (int m = 0; m < rows; m++) {
      free(rec_name[m]);
      free(host_name[m]);
    }
    free(rec_name);
    free(host_name);
  }
}

static void appendMasterSddspcasPvFile(const std::string &pcasPvFile, const std::string &pvPrefix, const std::vector<PVDef> &pvs) {
  SDDS_DATASET SDDS_masterlist;
  char hostname[255];
  int64_t rowsPresent;

  gethostname(hostname, 255);

  if (!SDDS_InitializeAppendToPage(&SDDS_masterlist, (char *)pcasPvFile.c_str(), 100, &rowsPresent)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(1);
  }
  if (!SDDS_LengthenTable(&SDDS_masterlist, (int64_t)pvs.size())) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(1);
  }

  for (size_t n = 0; n < pvs.size(); n++) {
    std::string pvAlias = formatPvName20(pvPrefix, pvs[n].controlName);
    if (!SDDS_SetRowValues(&SDDS_masterlist, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, rowsPresent,
                           "rec_name", pvAlias.c_str(), "host_name", hostname, NULL)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      exit(1);
    }
    rowsPresent++;
  }

  if (!SDDS_UpdatePage(&SDDS_masterlist, FLUSH_TABLE)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(1);
  }
  SDDS_Terminate(&SDDS_masterlist);
}

extern "C" void CleanMasterSDDSpcasPVFile() {
  if (gPcasPvFile.empty()) {
    return;
  }

  SDDS_DATASET SDDS_masterlist;
  long rows;
  char **rec_name = NULL;
  char **host_name = NULL;
  char hostname[255];

  gethostname(hostname, 255);

  if (!SDDS_InitializeInput(&SDDS_masterlist, (char *)gPcasPvFile.c_str())) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return;
  }
  if (SDDS_ReadPage(&SDDS_masterlist) != 1) {
    fprintf(stderr, "error: Unable to read SDDS file %s\n", gPcasPvFile.c_str());
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return;
  }

  rows = SDDS_SetRowsOfInterest(&SDDS_masterlist, (char *)"host_name", SDDS_MATCH_STRING, hostname, SDDS_NEGATE_MATCH | SDDS_AND);
  if (rows == -1) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return;
  }

  if (rows > 0) {
    rec_name = (char **)SDDS_GetColumn(&SDDS_masterlist, (char *)"rec_name");
    host_name = (char **)SDDS_GetColumn(&SDDS_masterlist, (char *)"host_name");
    if (!rec_name || !host_name) {
      fprintf(stderr, "error: Unable to read SDDS file %s\n", gPcasPvFile.c_str());
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      return;
    }
  }
  SDDS_Terminate(&SDDS_masterlist);

  if (!SDDS_InitializeOutput(&SDDS_masterlist, SDDS_BINARY, 1, NULL, NULL, (char *)gPcasPvFile.c_str())) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return;
  }
  if (!SDDS_DefineSimpleColumn(&SDDS_masterlist, (char *)"host_name", NULL, SDDS_STRING) ||
      !SDDS_DefineSimpleColumn(&SDDS_masterlist, (char *)"rec_name", NULL, SDDS_STRING)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return;
  }
  if (!SDDS_WriteLayout(&SDDS_masterlist)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return;
  }
  if (!SDDS_StartTable(&SDDS_masterlist, rows)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return;
  }
  if (rows > 0) {
    if (!SDDS_SetColumn(&SDDS_masterlist, SDDS_BY_NAME, host_name, rows, (char *)"host_name") ||
        !SDDS_SetColumn(&SDDS_masterlist, SDDS_BY_NAME, rec_name, rows, (char *)"rec_name")) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      return;
    }
  }
  if (!SDDS_WriteTable(&SDDS_masterlist)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return;
  }

  for (int m = 0; m < rows; m++) {
    free(rec_name[m]);
    free(host_name[m]);
  }
  if (rec_name)
    free(rec_name);
  if (host_name)
    free(host_name);
}

extern "C" int main(int argc, char **argv) {
  uint32_t debugLevel = 0u;
  double executionTime = 43200.0; /* 12 hours */
  bool forever = false;
  char pvPrefixC[128] = "";
  double noiseRate = -1.0;
  char arraySize[64] = "";
  char epicsBaseC[1024] = "../epics-base";
  int standalone = 0;

  char *PVFile1 = NULL;
  char *PVFile2 = NULL;

  std::vector<std::string> inputFiles;

  SDDS_RegisterProgramName(argv[0]);
  gExecutableDir = getExecutableDir(argv[0]);
  SCANNED_ARG *s_arg;
  argc = scanargs(&s_arg, argc, argv);
  if (argc < 2) {
    printUsage();
    exit(1);
  }

  bool epicsBaseProvided = false;

  for (int i_arg = 1; i_arg < argc; i_arg++) {
    if (s_arg[i_arg].arg_type == OPTION) {
      if (strcasecmp(s_arg[i_arg].list[0], "h") == 0) {
        printUsage();
        free_scanargs(&s_arg, argc);
        return 0;
      }
      switch (match_string(s_arg[i_arg].list[0], option, N_OPTIONS, 0)) {
      case SET_DEBUGLEVEL:
        if (s_arg[i_arg].n_items < 2)
          SDDS_Bomb((char *)"invalid -debugLevel syntax");
        if (sscanf(s_arg[i_arg].list[1], "%u", &debugLevel) != 1)
          SDDS_Bomb((char *)"invalid -debugLevel syntax or value");
        break;
      case SET_EXECUTIONTIME:
        if (s_arg[i_arg].n_items < 2)
          SDDS_Bomb((char *)"invalid -executionTime syntax");
        if (sscanf(s_arg[i_arg].list[1], "%lf", &executionTime) != 1)
          SDDS_Bomb((char *)"invalid -executionTime syntax or value");
        if (executionTime < .5) {
          forever = true;
          executionTime = 0.0;
        }
        break;
      case SET_PVPREFIX:
        if (s_arg[i_arg].n_items < 2)
          SDDS_Bomb((char *)"invalid -pvPrefix syntax");
        if (sscanf(s_arg[i_arg].list[1], "%127s", pvPrefixC) != 1)
          SDDS_Bomb((char *)"invalid -pvPrefix syntax or value");
        break;
      case SET_NOISE:
        if (s_arg[i_arg].n_items < 2)
          SDDS_Bomb((char *)"invalid -noise syntax");
        if (sscanf(s_arg[i_arg].list[1], "%lf", &noiseRate) != 1)
          SDDS_Bomb((char *)"invalid -noise syntax or value");
        break;
      case SET_ARRAYSIZE:
        if (s_arg[i_arg].n_items < 2)
          SDDS_Bomb((char *)"invalid -arraySize syntax");
        if (sscanf(s_arg[i_arg].list[1], "%63s", arraySize) != 1)
          SDDS_Bomb((char *)"invalid -arraySize syntax or value");
        break;
      case SET_EPICSBASE:
        if (s_arg[i_arg].n_items < 2)
          SDDS_Bomb((char *)"invalid -epicsBase syntax");
        if (sscanf(s_arg[i_arg].list[1], "%1023s", epicsBaseC) != 1)
          SDDS_Bomb((char *)"invalid -epicsBase syntax or value");
        epicsBaseProvided = true;
        break;
      case SET_STANDALONE:
        standalone = 1;
        break;
      case SET_MASTERPVFILE:
        if (s_arg[i_arg].n_items < 2)
          SDDS_Bomb((char *)"invalid -masterPVFile syntax");
        SDDS_CopyString(&PVFile1, s_arg[i_arg].list[1]);
        break;
      case SET_PCASPVFILE:
        if (s_arg[i_arg].n_items < 2)
          SDDS_Bomb((char *)"invalid -pcasPVFile syntax");
        SDDS_CopyString(&PVFile2, s_arg[i_arg].list[1]);
        break;
      case SET_HELP:
        printUsage();
        free_scanargs(&s_arg, argc);
        return 0;
      default:
        fprintf(stderr, "error: unknown switch: %s\n", s_arg[i_arg].list[0]);
        exit(1);
      }
    } else {
      inputFiles.push_back(std::string(s_arg[i_arg].list[0]));
    }
  }

  if (inputFiles.empty()) {
    fprintf(stderr, "error: no input files given\n");
    exit(1);
  }

  // Keep sddspcas defaults for compatibility (unless -standalone)
  if (!standalone) {
    if (PVFile1 == NULL) {
      SDDS_CopyString(&PVFile1, (char *)"/home/helios/iocinfo/pvdata/all/iocRecNames.sdds");
    }
    if (PVFile2 == NULL) {
      SDDS_CopyString(&PVFile2, (char *)"/home/helios/iocinfo/oagData/pvdata/iocRecNames.sdds");
    }
  }

  if (arraySize[0] != '\0') {
    epicsEnvSet("EPICS_CA_MAX_ARRAY_BYTES", arraySize);
  }

  // Default EPICS base should be an actual EPICS Base directory.
  // User-provided -epicsBase remains relative to the current working directory.
  std::string epicsBase;
  if (epicsBaseProvided) {
    epicsBase = resolveAbsolutePath(std::string(epicsBaseC));
  } else {
    epicsBase = findDefaultEpicsBase();
  }

  // Set up signal handling
  signal(SIGINT, signal_handler);
  signal(SIGTERM, signal_handler);

  // Create temp directory and db file
  char tmpTemplate[] = "/tmp/sddsSoftIOCXXXXXX";
  char *tmpDir = mkdtemp(tmpTemplate);
  if (!tmpDir) {
    fprintf(stderr, "error: mkdtemp failed: %s\n", strerror(errno));
    exit(1);
  }
  gTempDir = tmpDir;
  gDbPath = gTempDir + "/sddsSoftIOC.db";

  std::vector<PVDef> pvs;
  readPvInputFiles(inputFiles, std::string(pvPrefixC), pvs);

  if (PVFile1 != NULL) {
    readMasterPvFile(std::string(PVFile1), std::string(pvPrefixC), pvs);
  }
  if (PVFile2 != NULL) {
    readMasterSddspcasPvFile(std::string(PVFile2), std::string(pvPrefixC), pvs);
  }

  writeDbFile(gDbPath, pvs);

  if (PVFile2 != NULL) {
    gPcasPvFile = PVFile2;
    atexit(CleanMasterSDDSpcasPVFile);
  }

  // Launch IOC
  startSoftIoc(epicsBase, gDbPath);

  // Start equations helper (non-fatal if missing)
  startEquationsHelper(inputFiles, (int)debugLevel);

  if (PVFile2 != NULL) {
    appendMasterSddspcasPvFile(std::string(PVFile2), std::string(pvPrefixC), pvs);
  }

  // Initialize CA client context and wait for IOC to start
  caInit();
  if (!pvs.empty()) {
    waitForPvConnect(pvs[0].pvName, 5.0);
  }

  std::vector<NoiseChannel> noiseChannels;
  if (noiseRate > 0.0) {
    initNoiseChannels(pvs, noiseChannels, 5.0);
  }

  // Main loop
  double start = nowSeconds();
  double lastNoise = start;

  while (!gTerminate) {
    // Stop if IOC exited
    if (gIocPid > 0) {
      int status = 0;
      pid_t r = waitpid(gIocPid, &status, WNOHANG);
      if (r == gIocPid) {
        gIocPid = -1;
        break;
      }
    }

    if (!forever) {
      double now = nowSeconds();
      if ((now - start) >= executionTime) {
        break;
      }
    }

    if (noiseRate > 0.0) {
      double now = nowSeconds();
      if ((now - lastNoise) >= noiseRate) {
        doNoiseUpdate(noiseChannels);
        lastNoise = now;
      }
    }

    usleep(100000); // 100ms
  }

  // Shutdown
  clearNoiseChannels(noiseChannels);
  caFini();
  safeKill(gEqPid, SIGTERM);
  safeKill(gIocPid, SIGTERM);

  if (gEqPid > 0) {
    waitpid(gEqPid, NULL, 0);
  }
  if (gIocPid > 0) {
    waitpid(gIocPid, NULL, 0);
  }

  cleanupTemp();
  free_scanargs(&s_arg, argc);
  return 0;
}
