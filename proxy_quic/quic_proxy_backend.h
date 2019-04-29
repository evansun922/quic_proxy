#ifndef QUICHE_QUIC_PROXYQUIC_QUIC_PROXY_BACKEND_H_
#define QUICHE_QUIC_PROXYQUIC_QUIC_PROXY_BACKEND_H_


#include <unordered_map>

#include "net/third_party/quiche/src/quic/tools/quic_simple_server_backend.h"
#include "net/third_party/quiche/src/quic/proxy_quic/quic_proxy_curl.h"
#include "net/third_party/quiche/src/quic/proxy_quic/quic_curl_timer.h"
#include "net/third_party/quiche/src/spdy/core/spdy_framer.h"

namespace quic {

class QuicProxyServer;

class QuicProxyBackend : public QuicSimpleServerBackend {
 public:
  QuicProxyBackend();
  QuicProxyBackend(const QuicProxyBackend&) = delete;
  QuicProxyBackend& operator=(const QuicProxyBackend&) = delete;
  ~QuicProxyBackend() override;

  QuicProxyServer *get_server() { return server_; }
  void set_server(QuicProxyServer *server) { server_ = server; }
  CURLM *get_curlm() { return multi_curl_; }
  QuicCurlTimer* get_curl_timer() { return &curl_timer_; }
  
  bool InitializeBackend(const std::string& backend_url) override;
  bool IsBackendInitialized() const override;
  void FetchResponseFromBackend(
      const spdy::SpdyHeaderBlock& request_headers,
      const std::string& request_body,
      QuicSimpleServerBackend::RequestHandler* quic_server_stream) override;
  void CloseBackendResponseStream(
      QuicSimpleServerBackend::RequestHandler* quic_server_stream) override;

  void InitializeCURLM();
  void CheckCurlMultiInfo();

  int still_running_;
  
 private:

  /* CURLMOPT_SOCKETFUNCTION */
  static int CurlSockCB(CURL *e, curl_socket_t s, int what,
                        void *cbp, void *sockp);
  void CurlRemoveSock(QuicProxyCurl* proxy);
  void CurlAddSock(curl_socket_t s, CURL *easy, int action);
  void CurlSetSock(QuicProxyCurl* proxy, curl_socket_t s,
                   CURL *e, int act);

  static int CurlMultiTimerCB(CURLM *multi, long timeout_ms,
                              QuicProxyBackend *backend);

  bool CreateProxyCurl(const std::string& request_url,
                       QuicSpdyStream* quic_stream,
                       std::shared_ptr<QuicProxyCurl>& proxy);

  bool cache_initialized_;

  // curl multiplexing IO 
  CURLM *multi_curl_;
  // timer filedescriptor
  int timer_fd_;
  QuicProxyServer *server_;
  std::unordered_map<QuicSpdyStream*,
                     std::shared_ptr<QuicProxyCurl>> proxy_stream_hash_;
  std::unordered_map<CURL*, std::shared_ptr<QuicProxyCurl>> proxy_http_hash_;
  std::list<std::shared_ptr<QuicProxyCurl>> proxy_http_dead_list_;

  QuicCurlTimer curl_timer_;
  GURL backend_url_;
};

}  // namespace quic

#endif  // QUICHE_QUIC_PROXYQUIC_QUIC_PROXY_BACKEND_H_
