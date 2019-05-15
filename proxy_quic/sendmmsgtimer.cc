#include "net/third_party/quiche/src/quic/proxy_quic/sendmmsgtimer.h"
#include "net/third_party/quiche/src/quic/proxy_quic/quic_proxy_packet_writer.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_flags.h"


namespace quic {

SendMMsgTimer::SendMMsgTimer(QuicProxyPacketWriter* writer)
  : timer_fd_(-1), writer_(writer) {}

SendMMsgTimer::~SendMMsgTimer() {
  if (timer_fd_ != -1) {
    ::close(timer_fd_);
    timer_fd_ = -1;
  }
}

void SendMMsgTimer::InitializeTimer(QuicEpollServer* epoll_server, int interval) {
  if (timer_fd_ == -1) {
    timer_fd_ = timerfd_create(CLOCK_REALTIME, TFD_NONBLOCK | TFD_CLOEXEC);
    struct itimerspec its;
    memset(&its, 0, sizeof(struct itimerspec));
    its.it_interval.tv_sec = 0;
    its.it_interval.tv_nsec = interval; //40ms
    its.it_value.tv_sec = 0;
    its.it_value.tv_nsec = interval; //40ms 
    timerfd_settime(timer_fd_, TIMER_ABSTIME, &its, NULL);

    epoll_server->RegisterFD(timer_fd_, this,
                             EPOLLIN | EPOLLOUT | EPOLLET);
  }
}
  
void SendMMsgTimer::OnEvent(int fd, QuicEpollEvent* event) {
  DCHECK_EQ(fd, timer_fd_);
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

  writer_->Flush();
}



}  // namespace quic
