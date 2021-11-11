#include "server/keep_alive.hh"

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
    || ( empl && base()->epoll_add(t) )
  ) {
    ::close(s);
    ::close(t);
    return;
  }

  alive_s2t.emplace(s,t);
  alive_t2s.emplace(t,s);
}

void server_keep_alive::keep_alive_release(int s) {
  { std::shared_lock read_lock(mx);
    if (!alive_s2t.contains(s)) return;
  }
  std::unique_lock write_lock(mx);

  const auto st = alive_s2t.find(s);
  if (st == alive_s2t.end()) return;
  const int t = st->second;

  ::close(t);
  alive_s2t.erase(st);
  alive_t2s.erase(t);
}

// TODO: how to make sure this doesn't close an active socket
bool server_keep_alive::event(int t) { // timer ran out
  { std::shared_lock read_lock(mx);
    if (!alive_t2s.contains(t)) return false; // not this type of event
  }
  std::unique_lock write_lock(mx);
  // TODO: check if socket is queued
  // TODO: if queued, drop timer (even if socket hasn't been given to a thread)

  const auto ts = alive_t2s.find(t);
  if (ts == alive_t2s.end()) return true;
  const int s = ts->second;

  ::close(s);
  ::close(t);
  alive_s2t.erase(s);
  alive_t2s.erase(ts);

  return true;
}

} // end namespace ivanp
