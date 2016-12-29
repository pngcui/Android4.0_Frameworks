/*
 * Copyright 2011, The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <bcinfo/BitcodeTranslator.h>
#include <bcinfo/MetadataExtractor.h>

#include <ctype.h>
#include <dlfcn.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <unistd.h>

#include <vector>

// This file corresponds to the standalone bcinfo tool. It prints a variety of
// information about a supplied bitcode input file.

const char* inFile = NULL;

extern int opterr;
extern int optind;

bool translate = false;

static int parseOption(int argc, char** argv) {
  int c;
  while ((c = getopt(argc, argv, "t")) != -1) {
    opterr = 0;

    switch(c) {
      case '?':
        // ignore any error
        break;

      case 't':
        translate = true;
        break;

      default:
        // Critical error occurs
        return 0;
        break;
    }
  }

  if(optind >= argc) {
    fprintf(stderr, "input file required\n");
    return 0;
  }

  inFile = argv[optind];
  return 1;
}


static void dumpMetadata(bcinfo::MetadataExtractor *ME) {
  if (!ME) {
    return;
  }

  printf("exportVarCount: %u\n", ME->getExportVarCount());
  printf("exportFuncCount: %u\n", ME->getExportFuncCount());

  printf("exportForEachSignatureCount: %u\n",
         ME->getExportForEachSignatureCount());
  const uint32_t *sigList = ME->getExportForEachSignatureList();
  for (size_t i = 0; i < ME->getExportForEachSignatureCount(); i++) {
    printf("exportForEachSignatureList[%u]: %u\n", i, sigList[i]);
  }

  printf("pragmaCount: %u\n", ME->getPragmaCount());
  const char **keyList = ME->getPragmaKeyList();
  const char **valueList = ME->getPragmaValueList();
  for (size_t i = 0; i < ME->getPragmaCount(); i++) {
    printf("pragma[%u]: %s - %s\n", i, keyList[i], valueList[i]);
  }

  printf("objectSlotCount: %u\n", ME->getObjectSlotCount());
  const uint32_t *slotList = ME->getObjectSlotList();
  for (size_t i = 0; i < ME->getObjectSlotCount(); i++) {
    printf("objectSlotList[%u]: %u\n", i, slotList[i]);
  }

  return;
}


static size_t readBitcode(const char **bitcode) {
  if (!inFile) {
    fprintf(stderr, "input file required\n");
    return NULL;
  }

  struct stat statInFile;
  if (stat(inFile, &statInFile) < 0) {
    fprintf(stderr, "Unable to stat input file: %s\n", strerror(errno));
    return NULL;
  }

  if (!S_ISREG(statInFile.st_mode)) {
    fprintf(stderr, "Input file should be a regular file.\n");
    return NULL;
  }

  FILE *in = fopen(inFile, "r");
  if (!in) {
    fprintf(stderr, "Could not open input file %s\n", inFile);
    return NULL;
  }

  size_t bitcodeSize = statInFile.st_size;

  *bitcode = (const char*) calloc(1, bitcodeSize + 1);
  size_t nread = fread((void*) *bitcode, 1, bitcodeSize, in);

  if (nread != bitcodeSize)
      fprintf(stderr, "Could not read all of file %s\n", inFile);

  fclose(in);
  return nread;
}


static void releaseBitcode(const char **bitcode) {
  if (bitcode && *bitcode) {
    free((void*) *bitcode);
    *bitcode = NULL;
  }
  return;
}


int main(int argc, char** argv) {
  if(!parseOption(argc, argv)) {
    fprintf(stderr, "failed to parse option\n");
    return 1;
  }

  const char *bitcode = NULL;
  const char *translatedBitcode = NULL;
  size_t bitcodeSize = readBitcode(&bitcode);

  unsigned int version = 14;

  if (translate) {
    version = 12;
  }

  bcinfo::BitcodeTranslator *BT =
      new bcinfo::BitcodeTranslator(bitcode, bitcodeSize, version);
  if (!BT->translate()) {
    fprintf(stderr, "failed to translate bitcode\n");
    return 2;
  }

  bcinfo::MetadataExtractor *ME =
      new bcinfo::MetadataExtractor(BT->getTranslatedBitcode(),
                                    BT->getTranslatedBitcodeSize());
  if (!ME->extract()) {
    fprintf(stderr, "failed to get metadata\n");
    return 3;
  }

  dumpMetadata(ME);

  delete ME;
  delete BT;

  releaseBitcode(&bitcode);

  return 0;
}
