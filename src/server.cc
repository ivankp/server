#include "server.hh"

#include <cstdlib>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/epoll.h>

#include <type_traits>

#include "error.hh"

// #include "debug.hh"

namespace ivan {
namespace {

void nonblock(int fd) {
  PCALL(fcntl)(fd, F_SETFL,
    PCALL(fcntl)(fd,F_GETFL,0) | O_NONBLOCK);
}

}

void basic_server::init() {
  main_socket = PCALL(socket)(AF_INET, SOCK_STREAM, 0);
  epoll = PCALL(epoll_create1)(0);

  { int val = 1;
    PCALL(setsockopt)(
      main_socket, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));
  }

  sockaddr_in addr;
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  ::memset(&addr.sin_zero, '\0', sizeof(addr.sin_zero));
  PCALL(bind)(main_socket,reinterpret_cast<sockaddr*>(&addr),sizeof(addr));
  nonblock(main_socket);
  PCALL(listen)(main_socket, SOMAXCONN/*backlog*/);

  if (epoll_add(main_socket)) THROW_ERRNO("epoll_add()");

  { // allocate dynamic storage
    size_t len[] {
      n_threads * sizeof(std::thread),
      n_epoll_events * sizeof(epoll_event),
      n_threads * thread_buffer_size
    };
    size_t r = len[0] % alignof(epoll_event);
    if (r) r = alignof(epoll_event) - r;
    len[1] += (len[0] += r);

    r = len[1] % 8;
    if (r) r = 8 - r;
    len[2] += (len[1] += r);

    char* m = reinterpret_cast<char*>(::malloc( len[std::size(len)-1] ));

    threads = reinterpret_cast<decltype(threads)>(m);
    epoll_events = reinterpret_cast<decltype(epoll_events)>(m + len[0]);
    thread_buffers = m + len[1];
  }
}
basic_server::~basic_server() {
  ::close(epoll);
  ::close(main_socket);

  std::destroy_n(threads,n_threads);
  ::free(threads);
}

int basic_server::epoll_add(int fd) {
  epoll_event event {
    .events = EPOLLIN | EPOLLRDHUP | EPOLLET,
    .data = { .fd = fd }
  };
  return ::epoll_ctl(epoll,EPOLL_CTL_ADD,fd,&event);
}

void basic_server::loop() noexcept {
  for (;;) {
    auto n = PCALL(epoll_wait)(epoll, epoll_events, n_epoll_events, -1);
    while (n > 0) {
      try {
        const auto& e = epoll_events[--n];
        const int fd = e.data.fd;

        const auto flags = e.events;

        // TODO: Firefox favicon request never completes

        // TODO: https://stackoverflow.com/q/27175281/2640636
        // TODO: is EPOLLRDHUP handled correctly?
        if (flags & (EPOLLERR | EPOLLHUP | EPOLLRDHUP) || !(flags & EPOLLIN)) {
          // TODO: can this be the main socket?
          ::close(fd);
        } else if (fd == main_socket) {
          for (;;) {
            sockaddr_in addr;
            socklen_t addr_size = sizeof(addr);
            const int sock = ::accept(
              main_socket, reinterpret_cast<sockaddr*>(&addr), &addr_size);

            // get connection IP address
            // char addr_buf[16];
            // inet_ntop(addr.sin_family, &addr.sin_addr, addr_buf, sizeof(addr_buf) );

            // To later get sockaddr_in from socket file descriptor:
            // getpeername(sock, reinterpret_cast<sockaddr*>(&addr), &addr_size);

            if (sock < 0) {
              const auto e = errno;
              if (e == EAGAIN || e == EWOULDBLOCK) break;
              else THROW_ERRNO("accept()");
            }

            static_assert(
              std::is_same_v<decltype(addr.sin_addr.s_addr),uint32_t>
            );
            if (!accept(addr.sin_addr.s_addr)) {
              ::close(sock);
              continue;
            }

            // TODO: setsockopt(): SO_RCVTIMEO, SO_SNDTIMEO

            nonblock(sock);
            if (epoll_add(sock)) {
              ::close(sock);
              THROW_ERRNO("epoll_add()");
            }
          }
        } else {
          queue.push(fd);
        }
      } catch (const std::exception& e) {
        std::cerr << "\033[31m" << e.what() << "\033[0m" << std::endl;
      }
    }
  }
}

} // end namespace ivan
