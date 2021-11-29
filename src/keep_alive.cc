#include "keep_alive.hh"

#include <unistd.h>
#include <sys/timerfd.h>

#include "server.hh"
#include "error.hh"

namespace ivanp {

void server_keep_alive::keep_alive(int s, int sec) {
  std::lock_guard lock(mx);

  const int t = ::timerfd_create(CLOCK_REALTIME, TFD_NONBLOCK);
  if (t == -1) {
    ::close(s);
    return;
  }

  struct itimerspec itspec { };
  ::clock_gettime(CLOCK_REALTIME, &itspec.it_value);
  itspec.it_value.tv_sec += sec;

  if ( ::timerfd_settime(t,TFD_TIMER_ABSTIME,&itspec,nullptr) == -1
    || base()->epoll_add(t)
  ) {
    ::close(s);
    ::close(t);
    return;
  }

  alive_s2t[s] = t;
  alive_t2s[t] = s;
}

void server_keep_alive::release(int s) { // remove timer
  std::lock_guard lock(mx);

  auto& t = alive_s2t[s];
  if (t == -1) return; // not a timed socket

  ::close(t);
  t = { };
  alive_t2s[t] = { };
}

bool server_keep_alive::event(int t) { // check if a timer ran out
  std::lock_guard lock(mx);

  auto& s = alive_t2s[t];
  if (s == -1) return false; // not a keep-alive timer

  // Check if the timer has actually timed out.
  // This might have become a different timer while the event was queued.
  // In that case, release() has been called,
  // the socket either closed or given a different timer,
  // and the original timer has been closed.
  uint64_t ntimeouts = 0;
  ::read(t,&ntimeouts,sizeof(ntimeouts));

  if (ntimeouts > 0) {
    // a socket in use will be released when release() is called
    if (!base()->is_active_fd(s)) { // socket not in use
      ::close(s);
      ::close(t);
      s = { };
      alive_s2t[s] = { };
    }
  }
  return true;
}

} // end namespace ivanp
