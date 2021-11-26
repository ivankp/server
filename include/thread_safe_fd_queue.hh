#ifndef IVANP_THREAD_SAFE_FD_QUEUE_HH
#define IVANP_THREAD_SAFE_FD_QUEUE_HH

#include <queue>
#include <vector>
#include <mutex>
#include <condition_variable>

#include "index_dict.hh"

namespace ivanp {

class thread_safe_fd_queue {
  std::queue<int> q;
  index_dict<unsigned> u; // fd reference counts
  std::mutex mx;
  std::condition_variable cv;

public:
  thread_safe_fd_queue(): u(16) { }

  void push(int fd) {
    { std::lock_guard lock(mx);
      q.emplace(fd);
      ++u[fd];
    }
    cv.notify_all();
  }

  int pop() {
    std::unique_lock lock(mx);
    while (q.empty()) cv.wait(lock);
    const int fd = q.front();
    q.pop();
    return fd;
  }

  void release(int fd) {
    std::lock_guard lock(mx);
    --u[fd];
  }
  bool active(int fd) {
    std::lock_guard lock(mx);
    return u[fd];
  }
};

}

#endif
