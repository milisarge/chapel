#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include "chpllaunch.h"
#include "chpl_mem.h"
#include "error.h"

#define baseSysFilename ".chpl-sys-"
char sysFilename[FILENAME_MAX];

#define CHPL_CC_ARG "-cc"
static char *_ccArg = NULL;
static char* debug = NULL;

// TODO: Un-hard-code this stuff:

static int getNumCoresPerLocale(void) {
  FILE* sysFile;
  int coreMask;
  int bitMask = 0x1;
  int numCores;
  pid_t mypid;
  char* numCoresString = getenv("CHPL_LAUNCHER_CORES_PER_LOCALE");
  char* command;

  if (numCoresString) {
    numCores = atoi(numCoresString);
    if (numCores != 0)
      return numCores;
  }

  if (!debug) {
    mypid = getpid();
  } else {
    mypid = 0;
  }
  sprintf(sysFilename, "%s%d", baseSysFilename, (int)mypid);
  command = chpl_glom_strings(2, "cnselect -Lcoremask > ", sysFilename);
  system(command);
  sysFile = fopen(sysFilename, "r");
  if (fscanf(sysFile, "%d\n", &coreMask) != 1 || !feof(sysFile)) {
    chpl_error("unable to determine number of cores per locale; please set CHPL_LAUNCHER_CORES_PER_LOCALE", 0, 0);
  }
  coreMask >>= 1;
  numCores = 1;
  while (coreMask & bitMask) {
    coreMask >>= 1;
    numCores += 1;
  }
  fclose(sysFile);
  if (unlink(sysFilename)) {
    char msg[1024];
    sprintf(msg, "Error removing temporary file '%s': %s", sysFilename,
            strerror(errno));
    chpl_warning(msg, 0, 0);
  }
  return numCores;
}

static char _nbuf[16];
static char _dbuf[16];
static char** chpl_launch_create_argv(int argc, char* argv[],
                                      int32_t numLocales) {
  const int largc = 7;
  char *largv[largc];
  const char *host = getenv("CHPL_HOST_PLATFORM");
  const char *ccArg = _ccArg ? _ccArg :
    (host && !strcmp(host, "xe-cle") ? "none" : "cpu");

  largv[0] = (char *) "aprun";
  largv[1] = (char *) "-q";
  largv[2] = (char *) "-cc";
  largv[3] = (char *) ccArg;
  sprintf(_dbuf, "-d%d", getNumCoresPerLocale());
  largv[4] = _dbuf;
  sprintf(_nbuf, "-n%d", numLocales);
  largv[5] = _nbuf;
  largv[6] = (char *) "-N1";

  return chpl_bundle_exec_args(argc, argv, largc, largv);
}

int chpl_launch(int argc, char* argv[], int32_t numLocales) {
  debug = getenv("CHPL_LAUNCHER_DEBUG");
  return chpl_launch_using_exec("aprun",
                                chpl_launch_create_argv(argc, argv, numLocales),
                                argv[0]);
}


int chpl_launch_handle_arg(int argc, char* argv[], int argNum,
                           int32_t lineno, chpl_string filename) {
  int numArgs = 0;
  if (!strcmp(argv[argNum], CHPL_CC_ARG)) {
    _ccArg = argv[argNum+1];
    numArgs = 2;
  } else if (!strncmp(argv[argNum], CHPL_CC_ARG"=", strlen(CHPL_CC_ARG))) {
    _ccArg = &(argv[argNum][strlen(CHPL_CC_ARG)+1]);
    numArgs = 1;
  }
  if (numArgs > 0) {
    if (strcmp(_ccArg, "none") &&
        strcmp(_ccArg, "numa_node") &&
        strcmp(_ccArg, "cpu")) {
      char msg[256];
      sprintf(msg, "'%s' is not a valid cpu assignment", _ccArg);
      chpl_error(msg, 0, 0);
    }
  }
  return numArgs;
}


void chpl_launch_print_help(void) {
  fprintf(stdout, "LAUNCHER FLAGS:\n");
  fprintf(stdout, "===============\n");
  fprintf(stdout, "  %s keyword     : specify cpu assignment within a node: none (default), numa_node, cpu\n", CHPL_CC_ARG);
}
