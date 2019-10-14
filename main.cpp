#include <algorithm>
#include <string>
#include <thread>
#include <vector>

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>

#include "master.h"
#include "memusage_guard.h"

constexpr size_t GB = 1'024 * 1'024 * 1'024;

static master *master_ptr = nullptr;
void usage(const char *);
void flush_handler();

struct master_guard {
  // guard the master that is on stack.
  master_guard(master *m) { master_ptr = m; }

  ~master_guard() { master_ptr = nullptr; }
};

int main(int argc, char *argv[]) {
  int opt;
  size_t limit = 1 * GB, watermark = 0.9 * GB, top_k = 100,
         n_shards = std::thread::hardware_concurrency();
  while ((opt = getopt(argc, argv, "l:w:t:s:")) != -1) {
    switch (opt) {
    case 'l':
      limit = atoi(optarg);
      break;
    case 'w':
      watermark = atoi(optarg);
      break;
    case 't':
      top_k = atoi(optarg);
      break;
    case 's':
      n_shards = atoi(optarg);
      break;
    default:
      fprintf(stderr, "Unrecognized option\n");
      usage(argv[0]);
    }
  }
  if (watermark > limit) {
    fprintf(stderr,
            "Watermark must be lower than limit, the given watermark: %lu, the "
            "given limit: %lu\n",
            watermark, limit);
    usage(argv[0]);
  }
  if (optind >= argc) {
    fprintf(stderr, "Please indicate the input file\n");
    usage(argv[0]);
  }
  std::string input(argv[optind]);
  {
    memusage_guard g(limit, flush_handler);
    master m(std::move(input), n_shards, watermark, top_k);
    master_guard m_instance(&m);
    m.start();
    m.wait_for_all_workers();
    std::vector<entry<owned_url_t, false>> result(m.result_begin(),
                                                  m.result_end());
    std::sort(result.begin(), result.end());
    for (const auto &e : result) {
      printf("%s %lu\n", e.url.c_str(), e.count);
    }
  }
}

void usage(const char *progname) {
  fprintf(
      stderr,
      "%s [-l hard limit] [-w watermark] [-t topk] [-s shards] <linput file>\n",
      progname);
  exit(EXIT_FAILURE);
}

void flush_handler() {
  if (master_ptr) {
    if (master_ptr->flush_all() == 0) {
      static const char msg[] =
          "The limit is too low, cannot save more memory\n";
      write(2, msg, sizeof(msg));
      std::abort();
    }
  } else {
    static const char msg[] = "flush_handler should be only called while the "
                              "master instance is still on the stack\n";
    write(2, msg, sizeof(msg));
    std::abort();
  }
}
