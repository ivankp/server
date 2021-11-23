#ifndef IVANP_THREAD_SAFE_FD_QUEUE_HH
#define IVANP_THREAD_SAFE_FD_QUEUE_HH

#include <queue>
#include <vector>
#include <mutex>
#include <condition_variable>

namespace ivanp {

class thread_safe_fd_queue {
  std::queue<int> q;
  std::vector<bool> u; // TODO: can fd be used by multiple threads?
  std::mutex mx;
  std::condition_variable cv;

public:
  thread_safe_fd_queue(): u(16) { }

  void push(int fd) {
    { std::lock_guard lock(mx);
      q.emplace(fd);
      const decltype(u)::size_type i = fd;
      if (u.size() <= i) u.resize(u.size()*2);
      u[i] = true;
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
    u[fd] = false;
  }
  bool active(int fd) {
    std::lock_guard lock(mx);
    return u[fd];
  }
};

}

#endif
