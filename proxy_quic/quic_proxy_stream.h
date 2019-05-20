#ifndef QUICHE_QUIC_PROXYQUIC_QUIC_PROXY_STREAM_H_
#define QUICHE_QUIC_PROXYQUIC_QUIC_PROXY_STREAM_H_

#include "net/third_party/quiche/src/quic/tools/quic_simple_server_stream.h"

namespace quic {

class QuicProxyCurl;
  
// All this does right now is aggregate data, and on fin, send an HTTP
// response.
class QuicProxyStream : public QuicSimpleServerStream {
 public:
  QuicProxyStream(QuicStreamId id,
                  QuicSpdySession* session,
                  StreamType type,
                  QuicSimpleServerBackend* quic_simple_server_backend);
  QuicProxyStream(PendingStream pending,
                  QuicSpdySession* session,
                  StreamType type,
                  QuicSimpleServerBackend* quic_simple_server_backend);
  QuicProxyStream(const QuicProxyStream&) = delete;
  QuicProxyStream& operator=(const QuicProxyStream&) = delete;
  ~QuicProxyStream() override;

  void set_proxy_curl(QuicProxyCurl* proxy) { quic_proxy_curl_ = proxy; }
  QuicProxyCurl* get_proxy_curl() { return quic_proxy_curl_; }
  std::string get_peer_ip();

  
  bool OnStreamFrameAcked(QuicStreamOffset offset,
                          QuicByteCount data_length,
                          bool fin_acked,
                          QuicTime::Delta ack_delay_time,
                          QuicByteCount* newly_acked_length) override;

 private:
  QuicProxyCurl* quic_proxy_curl_;  // Not owned.
};

}  // namespace quic

#endif  // QUICHE_QUIC_PROXYQUIC_QUIC_PROXY_STREAM_H_
