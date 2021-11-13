#include "server.hh"

#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/epoll.h>

#include "error.hh"
// #include "debug.hh"

namespace ivanp {
namespace {

void nonblock(int fd) {
  PCALL(fcntl)(fd, F_SETFL,
    PCALLR(fcntl)(fd,F_GETFL,0) | O_NONBLOCK);
}

}

basic_server::basic_server(port_t port, unsigned epoll_buffer_size)
: main_socket(PCALLR(socket)(AF_INET, SOCK_STREAM, 0)),
  epoll(PCALLR(epoll_create1)(0)),
  n_epoll_events(epoll_buffer_size)
{
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

  epoll_events = new epoll_event[n_epoll_events];
}
basic_server::~basic_server() {
  ::close(epoll);
  ::close(main_socket);
  delete[] epoll_events;
}

bool basic_server::epoll_add(int fd) {
  epoll_event event {
    .events = EPOLLIN | EPOLLRDHUP | EPOLLET,
    .data = { .fd = fd }
  };
  return ::epoll_ctl(epoll,EPOLL_CTL_ADD,fd,&event);
}

void basic_server::loop() noexcept {
  for (;;) {
    auto n = PCALLR(epoll_wait)(epoll, epoll_events, n_epoll_events, -1);
    while (n > 0) {
      try {
        const auto& e = epoll_events[--n];
        int fd = e.data.fd;

        const auto flags = e.events;
        if (flags & EPOLLHUP || flags & EPOLLERR || !(flags & EPOLLIN)) {
          ::close(fd);
        } else if (fd == main_socket) {
          for (;;) {
            sockaddr_in addr;
            socklen_t addr_size = sizeof(addr);
            const int sock = ::accept(
              main_socket, reinterpret_cast<sockaddr*>(&addr), &addr_size);
            if (sock < 0) {
              if (errno == EAGAIN || errno == EWOULDBLOCK) break;
              else THROW_ERRNO("accept()");
            }

            nonblock(sock);
            if (epoll_add(sock)) {
              ::close(sock);
              THROW_ERRNO("epoll_add()");
            }
          }
        } else {
          // TODO: try to release from mixins before queuing
          queue.push(fd);
        }
      } catch (const std::exception& e) {
        std::cerr << "\033[31m" << e.what() << "\033[0m" << std::endl;
      }
    }
  }
}

} // end namespace ivanp
