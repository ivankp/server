#ifndef IVANP_SERVER_HH
#define IVANP_SERVER_HH

#include <cstdint>
#include <vector>
#include <thread>
#include <iostream>
#include <utility>

#include "thread_safe_queue.hh"

struct epoll_event; // <sys/epoll.h>

namespace ivanp {

using port_t = uint16_t;

class basic_server {
protected:
  int main_socket, epoll;
  std::vector<std::thread> threads;
  thread_safe_queue<int> queue;
  epoll_event* epoll_events;
  const unsigned n_epoll_events;

  struct thread_buffer {
    char* m;
    size_t size;

    explicit thread_buffer(size_t size) noexcept
    : m(new char[size]), size(size) { }
    ~thread_buffer() { delete[] m; }
    thread_buffer() = delete;
    thread_buffer(const thread_buffer&) = delete;
    thread_buffer& operator=(const thread_buffer&) = delete;
    thread_buffer(thread_buffer&& o) noexcept
    : m(o.m), size(o.size) { o.m = nullptr; o.size = 0; }
    thread_buffer& operator=(thread_buffer&& o) = delete;
  };

  virtual bool event(int fd) = 0;

public:
  basic_server(port_t port, unsigned epoll_buffer_size);
  ~basic_server();

  bool epoll_add(int);
  void loop() noexcept;

  template <typename F>
  void operator()(
    unsigned nthreads, size_t buffer_size,
    F&& worker_function
  ) noexcept {
    threads.reserve(threads.size()+nthreads);
    for (unsigned i=0; i<nthreads; ++i) {
      threads.emplace_back([
        this,
        worker_function,
        buffer = thread_buffer(buffer_size)
      ]() mutable {
        for (;;) {
          try {
            const int fd = queue.pop();
            if (!event(fd))
              worker_function(fd, buffer.m, buffer.size);
          } catch (const std::exception& e) {
            std::cerr << "\033[31;1m" << e.what() << "\033[0m" << std::endl;
          }
        }
      });
    }
  }
};

template <typename... Mixins>
class server final: public basic_server, public Mixins... {
  basic_server* base() noexcept { return this; }
  const basic_server* base() const noexcept { return this; }

  using basic_server::basic_server;

  bool event(int fd) override {
    return ( Mixins::event(fd) || ... || false );
  }
};

} // end namespace ivanp

#endif
