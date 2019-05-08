#ifndef QUICHE_QUIC_PROXYQUIC_QUIC_PROXY_HTTP_H_
#define QUICHE_QUIC_PROXYQUIC_QUIC_PROXY_HTTP_H_

#include <memory>
#include <curl/curl.h>

#include "net/third_party/quiche/src/quic/platform/api/quic_epoll.h"
#include "net/third_party/quiche/src/spdy/core/spdy_framer.h"

namespace quic {

class QuicProxyBackend;
class QuicProxyStream;

class QuicProxyCurl : public QuicEpollCallbackInterface {
 public:
  QuicProxyCurl(const std::string& request_url,
                QuicProxyBackend* backend,
                QuicProxyStream* quic_stream);
  QuicProxyCurl(const QuicProxyCurl&) = delete;
  QuicProxyCurl& operator=(const QuicProxyCurl&) = delete;
  ~QuicProxyCurl() override;

  CURL* get_curl() { return easy_; }
  int get_sockfd() { return sockfd_; }
  void set_sockfd(int fd) { sockfd_ = fd; }
  QuicProxyStream* get_stream() { return quic_stream_; }
  void set_stream(QuicProxyStream* stream) { quic_stream_ = stream; }
  uint32_t get_stream_id() { return stream_id_; }
  
  bool InitializeProxyCurl();
  void StartHttp(const spdy::SpdyHeaderBlock& request_headers,
                 const std::string& request_body);
  void ContinueDownload(uint64_t buffer_size);

  std::string Name() const override { return "QuicProxyCurl"; }
  // From EpollCallbackInterface
  void OnRegistration(QuicEpollServer* eps, int fd, int event_mask) override {}
  void OnModification(int fd, int event_mask) override {}
  void OnEvent(int fd, QuicEpollEvent* event) override;
  void OnUnregistration(int fd, bool replaced) override {}

  void OnShutdown(QuicEpollServer* eps, int fd) override {}

  
 private:
  static size_t WriteQuicCallback(void* contents, size_t size,
                                  size_t nmemb, void* userp);
  static size_t HttpHeaderCallback(void* contents, size_t size,
                                   size_t nmemb, void* userp);
  static size_t ReadQuicCallback(void* dest,
                                 size_t size,
                                 size_t nmemb,
                                 void* userp);
  static int ProgressQuicCallback(void* userp,
                                  double dltotal,
                                  double dlnow,
                                  double ultotal,
                                  double ulnow);
    
  int sockfd_;
  CURL* easy_;
  std::string request_url_;
  char error_[CURL_ERROR_SIZE];
  QuicProxyBackend* backend_;
  QuicProxyStream* quic_stream_; 
  uint32_t stream_id_;
  bool is_pause_;
  
  spdy::SpdyHeaderBlock spdy_headers_;
  size_t content_length_;
  size_t had_send_length_;

  struct curl_slist *request_header_list_;
  std::string request_body_;
  size_t request_body_pos_;

};

}  // namespace quic

#endif  // QUICHE_QUIC_PROXYQUIC_QUIC_PROXY_HTTP_H_
