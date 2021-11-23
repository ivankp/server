#include "keep_alive.hh"

#include <unistd.h>
#include <sys/timerfd.h>

#include "server.hh"
#include "error.hh"

namespace ivanp {

void server_keep_alive::keep_alive(int s, int sec) {
  std::unique_lock write_lock(mx);

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

  alive_s2t.emplace(s,t);
  alive_t2s.emplace(t,s);
}

void server_keep_alive::release(int s) { // remove timer
  { std::shared_lock read_lock(mx);
    if (!alive_s2t.contains(s)) return;
  }
  std::unique_lock write_lock(mx);

  const auto st = alive_s2t.find(s);
  if (st != alive_s2t.end()) { // this socket is still timed
    const int t = st->second; // timer

    ::close(t);
    alive_s2t.erase(st);
    alive_t2s.erase(t);
  }
}

bool server_keep_alive::event(int t) { // check if a timer ran out
  { std::shared_lock read_lock(mx);
    if (!alive_t2s.contains(t)) return false; // not this type of event
  }
  std::unique_lock write_lock(mx);

  const auto ts = alive_t2s.find(t);
  if (ts != alive_t2s.end()) { // this is still a keep-alive timer
    // Check if the timer has actually timed out.
    // This might have become a different timer while the event was queued.
    // In that case, release() has been called,
    // the socket either closed or given a different timer,
    // and the original timer has been closed.
    uint64_t ntimeouts = 0;
    ::read(t,&ntimeouts,sizeof(ntimeouts));

    if (ntimeouts > 0) {
      const int s = ts->second; // socket
      if (!base()->is_active_fd(s)) { // socket not in use
        // a socket in use will be released when release() is called
        ::close(s);
        ::close(t);
        alive_s2t.erase(s);
        alive_t2s.erase(ts);
      }
    }
  }
  return true;
}

} // end namespace ivanp
