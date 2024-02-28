#ifndef IVAN_SERVER_HH
#define IVAN_SERVER_HH

#include <cstdint>
#include <thread>
#include <iostream>
#include <utility>
#include <memory>
#include <deque>
#include <atomic>
#include <bit>

#include "socket.hh"
#include "thread_safe_queue.hh"

struct epoll_event; // <sys/epoll.h>

namespace ivan {

using port_t = uint16_t;

class basic_server {
public:
  port_t port = 80;
  unsigned n_threads = std::thread::hardware_concurrency();
  unsigned n_epoll_events = 64;
  size_t thread_buffer_size = 1 << 20;

protected:
  int main_socket, epoll;
  thread_safe_queue<int> queue;
  std::thread* threads;
  epoll_event* epoll_events;
  char* thread_buffers;

  // std::set<int> engaged_fds;
  std::deque<std::atomic<bool>> engaged_fds;
  std::mutex engaged_fds_mx;

  int epoll_add(int);
  virtual void init();

  virtual bool accept(uint32_t addr) = 0;
  virtual bool event (int fd) = 0;

private:
  void loop() noexcept;

public:
  ~basic_server();

  template <typename F>
  void operator()(F&& worker_function) {
    init();

    for (unsigned i=0; i<n_threads; ++i) {
      std::construct_at(threads+i, [
        this,
        worker_function,
        buffer = thread_buffers + thread_buffer_size*i,
        buffer_size = thread_buffer_size
      ]() mutable {
        for (;;) {
          const int fd = queue.pop();
          const size_t i = fd;

          // TODO: attempt to prevent multiple threads from getting the same socket
          // Doesn't prevent this:
          // socket 5 from src/socket.cc:70: getpeername(): 9: Bad file descriptor
          // socket 5 from 127.0.0.1
          { std::lock_guard lock(engaged_fds_mx);
            const size_t n1 = engaged_fds.size();
            if (i >= n1) {
              const size_t n2 = std::bit_ceil(i);
              engaged_fds.resize(n2);
              for (size_t i=n1; i<n2; ++i)
                engaged_fds[i].store(false);
            } else if (engaged_fds[i].load()) {
              continue;
            }
            engaged_fds[i].store(true);
          }

          try {
            if (!event(fd)) {
              worker_function(fd, buffer, buffer_size);
              // TODO: make sure worker_function doesn't run indefinitely
            }
          } catch (const std::exception& e) {
            std::cerr << "\033[31;1m" << e.what() << "\033[0m" << std::endl;
          } catch (...) {
            std::cerr << "\033[31;1m" "UNKNOWN EXCEPTION" "\033[0m" << std::endl;
          }

          engaged_fds[i].store(false);
        }
      });
    }

    loop();
  }
};

template <typename... Mixins>
class server final: public basic_server, public Mixins... {
  basic_server* base() noexcept { return this; }
  const basic_server* base() const noexcept { return this; }

  using basic_server::basic_server;

  void init() override {
    basic_server::init();
    ( [&]{
      if constexpr (requires { Mixins::init(); })
        Mixins::init();
    }(), ... );
  }

  bool accept(uint32_t addr) override {
    return ( [&]() -> bool {
      if constexpr (requires { Mixins::accept(addr); })
        return Mixins::accept(addr);
      else
        return true;
    }() && ... ); // default is true
  }

  bool event(int fd) override {
    return ( [&]() -> bool {
      if constexpr (requires { Mixins::event(fd); })
        return Mixins::event(fd);
      else
        return false;
    }() || ... ); // default is false
  }
};

} // end namespace ivan

#endif
