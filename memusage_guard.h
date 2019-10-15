#pragma once

#include <new>

#include <assert.h>
#include <stddef.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <unistd.h>

/* setrlimit is not respected on WSL, need to figure that out */
template <int resource = RLIMIT_RSS> struct memusage_guard {
  rlimit new_rlimit, old_rlimit;
  std::new_handler old_handler;

  memusage_guard(rlim_t limit, std::new_handler handler) {
    assert(::getrlimit(resource, &old_rlimit) == 0);
    new_rlimit.rlim_max = old_rlimit.rlim_max;
    new_rlimit.rlim_cur = limit;
    assert(::setrlimit(resource, &new_rlimit) == 0);
    old_handler = std::set_new_handler(handler);
  }

  ~memusage_guard() {
    ::setrlimit(resource, &old_rlimit);
    std::set_new_handler(old_handler);
  }
};
