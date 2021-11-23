#ifndef IVANP_THREAD_SAFE_FD_QUEUE_HH
#define IVANP_THREAD_SAFE_FD_QUEUE_HH

#include <queue>
#include <unordered_map>
#include <mutex>
#include <condition_variable>

namespace ivanp {

class thread_safe_fd_queue {
  std::queue<int> q;
  std::unordered_map<int,unsigned> u;
  std::mutex mx;
  std::condition_variable cv;

public:
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
    if (!--u[fd]) u.erase(fd);
  }
  bool active(int fd) {
    std::lock_guard lock(mx);
    return u.contains(fd);
  }
};

}

#endif
