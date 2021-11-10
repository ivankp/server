#include "server/keep_alive.hh"

#include <unistd.h>
#include <sys/timerfd.h>

#include "server.hh"
#include "error.hh"

namespace ivanp {

void server_keep_alive::keep_alive(int sock, int sec) {
  const std::lock_guard lock(mx);

  const auto [st,empl] = alive_s2t.emplace(sock,0);

  struct itimerspec itspec { };
  int timer;
  if (empl) {
    timer = ::timerfd_create(CLOCK_REALTIME, TFD_NONBLOCK);
    if (timer == -1) goto close_socket;
    st->second = timer;
  } else {
    timer = st->second;
  }

  ::clock_gettime(CLOCK_REALTIME, &itspec.it_value);
  itspec.it_value.tv_sec += sec;

  if ( ::timerfd_settime(timer,TFD_TIMER_ABSTIME,&itspec,nullptr) == -1
    || ( empl && base()->epoll_add(timer) ) )
    goto close_both;

  if (empl) alive_t2s.emplace(timer,sock);
  return;

close_both:
  ::close(timer);
  if (!empl) alive_t2s.erase(timer);
close_socket:
  ::close(sock);
  alive_s2t.erase(st);
}

bool server_keep_alive::event(int timer) { // timer ran out
  const std::lock_guard lock(mx);

  const auto ts = alive_t2s.find(timer);
  if (ts == alive_t2s.end()) return false; // not this type of event

  uint64_t expired = 0;
  if (::read(timer, &expired, sizeof(expired)) < 0 && errno == EAGAIN)
    expired = 0;
  // TODO: is this the right thing to do
  // to make sure this event didn't fire right before timer was updated?

  if (expired != 0) {
    ::close(timer);
    alive_t2s.erase(ts);
    const int sock = ts->second;
    ::close(sock);
    alive_s2t.erase(sock);
  }
  return true;
}

} // end namespace ivanp
