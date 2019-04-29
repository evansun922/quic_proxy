#ifndef QUICHE_QUIC_PROXYQUIC_QUIC_CURL_TIMER_H_
#define QUICHE_QUIC_PROXYQUIC_QUIC_CURL_TIMER_H_

#include <memory>
#include <sys/timerfd.h>

#include "net/third_party/quiche/src/quic/platform/api/quic_epoll.h"


namespace quic {

class QuicProxyBackend;

class QuicCurlTimer : public QuicEpollCallbackInterface {
 public:
  QuicCurlTimer(QuicProxyBackend* backend);
  QuicCurlTimer(const QuicCurlTimer&) = delete;
  QuicCurlTimer& operator=(const QuicCurlTimer&) = delete;
  ~QuicCurlTimer() override;

  int get_timerfd() { return timer_fd_; }
  void set_timerfd(int fd) { timer_fd_ = fd; }

  void InitializeCurlTimer();
  
  std::string Name() const override { return "QuicCurlTimer"; }  
  // From EpollCallbackInterface
  void OnRegistration(QuicEpollServer* eps, int fd, int event_mask) override {}
  void OnModification(int fd, int event_mask) override {}
  void OnEvent(int fd, QuicEpollEvent* event) override;
  void OnUnregistration(int fd, bool replaced) override {}

  void OnShutdown(QuicEpollServer* eps, int fd) override {}


 private:

  int timer_fd_;
  QuicProxyBackend* backend_;
};

}  // namespace quic

#endif  // QUICHE_QUIC_PROXYQUIC_QUIC_PROXY_HTTP_H_
