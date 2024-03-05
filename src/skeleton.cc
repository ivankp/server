#define STR1(x) #x
#define STR(x) STR1(x)

#define ERROR_PREF STR(__LINE__) ": "

#define THROW_ERROR \
  throw [](const std::string& msg){ \
    return std::runtime_error(ERROR_PREF + msg); \
  }

#define THROW_ERRNO(FNAME) { \
  const auto e = errno; \
  THROW_ERROR( FNAME "(): " + std::to_string(e) + ": " + std::strerror(e) ); \
}

#define PCALL(F) [](auto&&... x){ \
  const auto r = ::F(x...); \
  if (r == -1) THROW_ERRNO(STR(F)) \
  return r; \
}

#include <iostream>
#include <stdexcept>

#include <cerrno>
#include <cstring>
#include <cstdlib>

#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/epoll.h>

void nonblock(int fd) {
  PCALL(fcntl)(fd, F_SETFL,
    PCALL(fcntl)(fd,F_GETFL,0) | O_NONBLOCK);
}

int main(int argc, char** argv) {

  const int main_socket = PCALL(socket)(AF_INET, SOCK_STREAM, 0);
  const int epoll = PCALL(epoll_create1)(0);

  { int val = 1;
    PCALL(setsockopt)(
      main_socket, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));
  }

  sockaddr_in addr;
  addr.sin_family = AF_INET;
  addr.sin_port = htons(8080);
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  ::memset(&addr.sin_zero, '\0', sizeof(addr.sin_zero));
  PCALL(bind)(main_socket, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
  nonblock(main_socket);
  PCALL(listen)(main_socket, SOMAXCONN/*backlog*/);

  auto epoll_add = [&](int fd){
    epoll_event event {
      .events = EPOLLIN | EPOLLRDHUP | EPOLLET,
      .data = { .fd = fd }
    };
    return ::epoll_ctl(epoll, EPOLL_CTL_ADD, fd, &event);
  };

  if (epoll_add(main_socket)) {
    THROW_ERRNO("epoll_add")
  }

  epoll_event epoll_events[64] { };

  for (;;) {
    try {
      int n = PCALL(epoll_wait)(
        epoll, epoll_events, std::size(epoll_events), -1
      );
      epoll_event
        * event = epoll_events,
        * const events_end = epoll_events + n;

      for (; event < events_end; ++event) {
        const int fd = event->data.fd;
        const auto flags = event->events;

        if (flags & (EPOLLERR | EPOLLHUP | EPOLLRDHUP) || !(flags & EPOLLIN)) {
          ::close(fd);
        } else if (fd == main_socket) {
          for (;;) {
            sockaddr_in addr;
            socklen_t addr_size = sizeof(addr);
            const int sock = ::accept(
              main_socket, reinterpret_cast<sockaddr*>(&addr), &addr_size
            );

            if (sock < 0) {
              const auto e = errno;
              if (e == EAGAIN || e == EWOULDBLOCK) break;
              else THROW_ERRNO("accept()");
            }

            nonblock(sock);
            if (epoll_add(sock)) {
              ::close(sock);
              THROW_ERRNO("epoll_add");
            }
          }
        } else {
          // queue.push(fd);
        }
      }
    } catch (const std::exception& e) {
      std::cerr << "\033[31m" << e.what() << "\033[0m" << std::endl;
    } catch (...) {
      std::cerr << "\033[31m" "UNEXPECTED EXCEPTION" "\033[0m" << std::endl;
    }
  }
}
