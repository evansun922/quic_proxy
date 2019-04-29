#ifndef QUICHE_QUIC_PROXYQUIC_SENDMMSGTIMER_H_
#define QUICHE_QUIC_PROXYQUIC_SENDMMSGTIMER_H_

#include <memory>
#include <sys/timerfd.h>

#include "net/third_party/quiche/src/quic/platform/api/quic_epoll.h"

namespace quic {

class QuicProxyPacketWriter;
  
class SendMMsgTimer : public QuicEpollCallbackInterface {
 public:
  SendMMsgTimer(QuicProxyPacketWriter* writer);
  SendMMsgTimer(const SendMMsgTimer&) = delete;
  SendMMsgTimer& operator=(const SendMMsgTimer&) = delete;
  ~SendMMsgTimer() override;

  void InitializeTimer(QuicEpollServer* epoll_server);
  
  int get_timerfd() { return timer_fd_; }
  void set_timerfd(int fd) { timer_fd_ = fd; }
  
  std::string Name() const override { return "SendMMsgTimer"; }  
  // From EpollCallbackInterface
  void OnRegistration(QuicEpollServer* eps, int fd, int event_mask) override {}
  void OnModification(int fd, int event_mask) override {}
  void OnEvent(int fd, QuicEpollEvent* event) override;
  void OnUnregistration(int fd, bool replaced) override {}

  void OnShutdown(QuicEpollServer* eps, int fd) override {}


 private:

  int timer_fd_;
  QuicProxyPacketWriter* writer_;
};

}  // namespace quic

#endif  // QUICHE_QUIC_PROXYQUIC_QUIC_PROXY_HTTP_H_
