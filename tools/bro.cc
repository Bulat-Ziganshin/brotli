/* Copyright 2014 Google Inc. All Rights Reserved.

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

   http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.

   Example main() function for Brotli library.
*/

#include <fcntl.h>
#include <stdio.h>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <ctime>

#include "../dec/decode.h"
#include "../enc/encode.h"
#include "../enc/streams.h"


static bool ParseQuality(const char* s, int* quality) {
  if (s[0] >= '0' && s[0] <= '9') {
    *quality = s[0] - '0';
    if (s[1] >= '0' && s[1] <= '9') {
      *quality = *quality * 10 + s[1] - '0';
      return s[2] == 0;
    }
    return s[1] == 0;
  }
  return false;
}

static void ParseArgv(int argc, char **argv,
                      char **input_path,
                      char **output_path,
                      int *force,
                      int *quality,
                      int *decompress,
                      int *repeat,
                      int *verbose,
                      int *lgwin,
                      int *mode) {
  *force = 0;
  *input_path = 0;
  *output_path = 0;
  *repeat = 1;
  *verbose = 0;
  {
    size_t argv0_len = strlen(argv[0]);
    *decompress =
        argv0_len >= 5 && strcmp(&argv[0][argv0_len - 5], "unbro") == 0;
  }
  for (int k = 1; k < argc; ++k) {
    if (!strcmp("--force", argv[k]) ||
        !strcmp("-f", argv[k])) {
      if (*force != 0) {
        goto error;
      }
      *force = 1;
      continue;
    } else if (!strcmp("--decompress", argv[k]) ||
               !strcmp("--uncompress", argv[k]) ||
               !strcmp("-d", argv[k])) {
      *decompress = 1;
      continue;
    } else if (!strcmp("--verbose", argv[k]) ||
               !strcmp("-v", argv[k])) {
      if (*verbose != 0) {
        goto error;
      }
      *verbose = 1;
      continue;
    }
    if (k < argc - 1) {
      if (!strcmp("--input", argv[k]) ||
          !strcmp("--in", argv[k]) ||
          !strcmp("-i", argv[k])) {
        if (*input_path != 0) {
          goto error;
        }
        *input_path = argv[k + 1];
        ++k;
        continue;
      } else if (!strcmp("--output", argv[k]) ||
                 !strcmp("--out", argv[k]) ||
                 !strcmp("-o", argv[k])) {
        if (*output_path != 0) {
          goto error;
        }
        *output_path = argv[k + 1];
        ++k;
        continue;
      } else if (!strcmp("--quality", argv[k]) ||
                 !strcmp("-q", argv[k])) {
        if (!ParseQuality(argv[k + 1], quality)) {
          goto error;
        }
        ++k;
        continue;
      } else if (!strcmp("--repeat", argv[k]) ||
                 !strcmp("-r", argv[k])) {
        if (!ParseQuality(argv[k + 1], repeat)) {
          goto error;
        }
        ++k;
        continue;
      } else if (!strcmp("--window", argv[k]) ||
                 !strcmp("-w", argv[k])) {
        if (!ParseQuality(argv[k + 1], lgwin)) {
          goto error;
        }
        ++k;
        continue;
      } else if (!strcmp("--mode", argv[k]) ||
                 !strcmp("-m", argv[k])) {
        if (!ParseQuality(argv[k + 1], mode)) {
          goto error;
        }
        ++k;
        continue;
      }
    }
    goto error;
  }
  return;
error:
  fprintf(stderr,
          "Usage: bro [--quality n] [--window n] [--mode n]"
          " [--decompress] [--force] [--input filename] [--output filename]"
          " [--repeat iters] [--verbose]\n"

          "  --quality: controls the compression-speed vs compression-density "
          "tradeoff. The higher the quality, the slower the "
          "compression. Range is 0 to 11. Defaults to 11.\n"

          "  --window: base 2 logarithm of the sliding window size. Range is "
          "16 to 24. Defaults to 22.\n"

          "  --mode: the compression mode can be 0 for generic input, "
          "1 for UTF-8 encoded text, or 2 for WOFF 2.0 font data. "
          "Defaults to 0.\n"

          "Usage example: bro -q 9 -w 24 -v -f -i INFILE -o OUTFILE\n"
          );
  exit(1);
}

static FILE* OpenInputFile(const char* input_path) {
  if (input_path == 0) {
    setmode(STDIN_FILENO,O_BINARY);
    return stdin;
  }
  FILE* f = fopen(input_path, "rb");
  if (f == 0) {
    perror("fopen");
    exit(1);
  }
  return f;
}

static FILE *OpenOutputFile(const char *output_path, const int force) {
  if (output_path == 0) {
    setmode(STDOUT_FILENO,O_BINARY);
    return stdout;
  }
  if (!force) {
    struct stat statbuf;
    if (stat(output_path, &statbuf) == 0) {
      fprintf(stderr, "output file exists\n");
      exit(1);
    }
  }
  FILE* f = fopen(output_path, "wb");
  if (f == 0) {
    perror("fopen");
    exit(1);
  }
  return f;
}

int64_t FileSize(char *path) {
  FILE *f = fopen(path, "rb");
  if (f == NULL) {
    return -1;
  }
  fseek(f, 0L, SEEK_END);
  int64_t retval = ftell(f);
  fclose(f);
  return retval;
}

int main(int argc, char** argv) {
  char *input_path = 0;
  char *output_path = 0;
  int force = 0;
  int quality = 11;
  int decompress = 0;
  int repeat = 1;
  int verbose = 0;
  int lgwin = 22;
  int mode = 0;
  ParseArgv(argc, argv, &input_path, &output_path, &force,
            &quality, &decompress, &repeat, &verbose, &lgwin, &mode);
  const clock_t clock_start = clock();
  for (int i = 0; i < repeat; ++i) {
    FILE* fin = OpenInputFile(input_path);
    FILE* fout = OpenOutputFile(output_path, force);
    if (decompress) {
      BrotliInput in = BrotliFileInput(fin);
      BrotliOutput out = BrotliFileOutput(fout);
      if (!BrotliDecompress(in, out)) {
        fprintf(stderr, "corrupt input\n");
        exit(1);
      }
    } else {
      brotli::BrotliParams params;
      params.quality = quality;
      params.lgwin = lgwin;
      params.mode = static_cast<brotli::BrotliParams::Mode> (mode);
      brotli::BrotliFileIn in(fin, 1 << 16);
      brotli::BrotliFileOut out(fout);
      if (!BrotliCompress(params, &in, &out)) {
        fprintf(stderr, "compression failed\n");
        unlink(output_path);
        exit(1);
      }
    }
    if (fclose(fin) != 0) {
      perror("fclose");
      exit(1);
    }
    if (fclose(fout) != 0) {
      perror("fclose");
      exit(1);
    }
  }
  if (verbose) {
    const clock_t clock_end = clock();
    double duration =
        static_cast<double>(clock_end - clock_start) / CLOCKS_PER_SEC;
    if (duration < 1e-9) {
      duration = 1e-9;
    }
    double insize  = static_cast<double> (FileSize(input_path));
    double outsize = static_cast<double> (FileSize(output_path));
    double uncompressed_bytes = repeat * (decompress ? outsize : insize);

    double uncompressed_bytes_in_MB = uncompressed_bytes / (1024.0 * 1024.0);
    if (decompress) {
      printf("%.0lf -> %.0lf: %.3lf%%", outsize, insize, insize*100.0/outsize);
    } else {
      printf("%.0lf -> %.0lf: %.3lf%%", insize, outsize, outsize*100.0/insize);
    }
    printf("   %.3lf sec   %.3lf MiB/s\n", duration, uncompressed_bytes_in_MB / duration);
  }
  return 0;
}
