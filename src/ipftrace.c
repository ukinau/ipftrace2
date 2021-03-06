/*
 * SPDX-License-Identifier: (GPL-2.0-only OR BSD-2-Clause)
 * Copyright (C) 2020 Yutaro Hayakawa
 */
#include <assert.h>
#include <errno.h>
#include <getopt.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ipftrace.h"

static struct option options[] = {
    {"debug-format", required_argument, 0, 'f'},
    {"list", optional_argument, 0, 'l'},
    {"mark", required_argument, 0, 'm'},
    {"regex", optional_argument, 0, 'r'},
    {"script", optional_argument, 0, 's'},
    {"perf-page-count", optional_argument, 0, '0'},
    {"test", optional_argument, 0, '0'},
    {"no-set-rlimit", optional_argument, 0, '0'},
    {NULL, 0, 0, 0},
};

static void
usage(void)
{
  fprintf(stderr,
          "Usage: ipft [OPTIONS]\n"
          "\n"
          "Options:\n"
          " -f, --debug-format    [DEBUG-FORMAT]  Read the debug "
          "information with specified format\n"
          " -l, --list                            List functions\n"
          " -m, --mark            [MARK]          Trace the packet "
          "marked with <mark> [required]\n"
          " -r, --regex           [REGEX]         Filter the function to trace"
          "with regex\n"
          " -s, --script-path     [PATH]          Path to the Lua script file\n"
          "   , --perf-page-count [NUMBER]        Number of pages to use with"
          "perf\n"
          "   , --test                            Run in eBPF test mode\n"
          "   , --no-set-rlimit                   Don't set rlimit\n"
          "\n"
          "MARK         := hex number\n"
          "DEBUG-FORMAT := { dwarf, btf }\n"
          "PATH         := path\n"
          "\n");
}

static void
opt_init(struct ipft_tracer_opt *opt)
{
  opt->mark = 0;
  opt->script_path = NULL;
  opt->debug_info_type = "dwarf";
  opt->perf_page_cnt = 8;
  opt->regex = NULL;
  opt->set_rlimit = true;
}

static void
opt_deinit(struct ipft_tracer_opt *opt)
{
  /* Compare address */
  if (opt->debug_info_type != (char *)"dwarf") {
    free(opt->debug_info_type);
  }

  if (opt->script_path != NULL) {
    free(opt->script_path);
  }

  if (opt->regex != NULL) {
    free(opt->regex);
  }
}

static bool
opt_validate(struct ipft_tracer_opt *opt, bool list)
{
  if (!list && opt->mark == 0) {
    fprintf(stderr, "-m --mark is missing (or specified 0 which is invalid)\n");
    return false;
  }

  if (strcmp(opt->debug_info_type, "dwarf") != 0 &&
      strcmp(opt->debug_info_type, "btf") != 0) {
    fprintf(stderr, "Invalid debug info format %s\n", opt->debug_info_type);
    return false;
  }

  if (!list && opt->perf_page_cnt == 0) {
    fprintf(stderr, "Perf page count should be at least 1\n");
    return false;
  }

  return true;
}

int
main(int argc, char **argv)
{
  int c, optind;
  int error = -1;
  const char *optname;
  struct ipft_tracer_opt opt;
  bool list = false, test = false;

  opt_init(&opt);

  while ((c = getopt_long(argc, argv, "f:lm:r:s:0", options, &optind)) != -1) {
    switch (c) {
    case 'f':
      opt.debug_info_type = strdup(optarg);
      break;
    case 'l':
      list = true;
      break;
    case 'm':
      opt.mark = strtoul(optarg, NULL, 16);
      break;
    case 'r':
      opt.regex = strdup(optarg);
      break;
    case 's':
      opt.script_path = strdup(optarg);
      break;
    case '0':
      optname = options[optind].name;

      if (strcmp(optname, "perf-page-count") == 0) {
        opt.perf_page_cnt = atoi(optarg);
        break;
      }

      if (strcmp(optname, "test") == 0) {
        test = true;
        break;
      }

      if (strcmp(optname, "no-set-rlimit") == 0) {
        opt.set_rlimit = false;
        break;
      }

      break;
    default:
      usage();
      goto end;
    }
  }

  if (!opt_validate(&opt, list)) {
    usage();
    goto end;
  }

  if (list) {
    error = list_functions(&opt);
    goto end;
  }

  if (test) {
    error = test_bpf_prog(&opt);
    goto end;
  }

  error = tracer_run(&opt);
  if (error == -1) {
    fprintf(stderr, "Trace failed with error\n");
  }

end:
  opt_deinit(&opt);
  return error == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
