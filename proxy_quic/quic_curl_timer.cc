#include <errno.h>
#include <features.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>

#include <cstdint>
#include <memory>

#include "net/third_party/quiche/src/quic/proxy_quic/quic_curl_timer.h"
#include "net/third_party/quiche/src/quic/proxy_quic/quic_proxy_server.h"
#include "net/third_party/quiche/src/quic/proxy_quic/quic_proxy_backend.h"



namespace quic {

QuicCurlTimer::QuicCurlTimer(QuicProxyBackend* backend)
  : timer_fd_(-1), backend_(backend) {}

QuicCurlTimer::~QuicCurlTimer() {
  if (timer_fd_ != -1) {
    ::close(timer_fd_);
    timer_fd_ = -1;
  }
  backend_ = nullptr;
}

void QuicCurlTimer::InitializeCurlTimer() {
  if (timer_fd_ == -1) {
    timer_fd_ = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
    struct itimerspec its;
    memset(&its, 0, sizeof(struct itimerspec));
    its.it_interval.tv_sec = 1;
    its.it_value.tv_sec = 1;
    timerfd_settime(timer_fd_, 0, &its, NULL);

    backend_->get_server()->epoll_server()->RegisterFD(timer_fd_, this,
                                                       EPOLLIN | EPOLLOUT | EPOLLET);
  }
}
  
void QuicCurlTimer::OnEvent(int fd, QuicEpollEvent* event) {
  DCHECK_EQ(fd, timer_fd_);
  CURLMcode rc;
  uint64_t count = 0;
  ssize_t err = 0;

  err = read(fd, &count, sizeof(uint64_t));
  if (err == -1) {
    if (errno == EAGAIN) {
      return;
    }
  }
  
  if (err != sizeof(uint64_t)) {
  }

  rc = curl_multi_socket_action(backend_->get_curlm(),
                                CURL_SOCKET_TIMEOUT, 0, &backend_->still_running_);
  backend_->CheckCurlMultiInfo();
}



}  // namespace quic
