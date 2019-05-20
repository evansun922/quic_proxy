#include "net/third_party/quiche/src/quic/proxy_quic/quic_proxy_stream.h"
#include "net/third_party/quiche/src/quic/proxy_quic/quic_proxy_curl.h"
#include "net/third_party/quiche/src/quic/core/quic_session.h"

namespace quic {

QuicProxyStream::QuicProxyStream(
    QuicStreamId id,
    QuicSpdySession* session,
    StreamType type,
    QuicSimpleServerBackend* quic_simple_server_backend)
    : QuicSimpleServerStream(id, session, type, quic_simple_server_backend),
      quic_proxy_curl_(nullptr) {
}

QuicProxyStream::QuicProxyStream(
    PendingStream pending,
    QuicSpdySession* session,
    StreamType type,
    QuicSimpleServerBackend* quic_simple_server_backend)
    : QuicSimpleServerStream(std::move(pending), session, type, quic_simple_server_backend),
      quic_proxy_curl_(nullptr) {
}

QuicProxyStream::~QuicProxyStream() = default;

std::string QuicProxyStream::get_peer_ip() {
  return session()->peer_address().host().Normalized().ToString();
}
  
bool QuicProxyStream::OnStreamFrameAcked(QuicStreamOffset offset,
                                         QuicByteCount data_length,
                                         bool fin_acked,
                                         QuicTime::Delta ack_delay_time,
                                         QuicByteCount* newly_acked_length) {
  bool rs = QuicSimpleServerStream::OnStreamFrameAcked(offset,
                                                       data_length,
                                                       fin_acked,
                                                       ack_delay_time,
                                                       newly_acked_length);
  if (quic_proxy_curl_) {
    quic_proxy_curl_->ContinueDownload(BufferedDataBytes());
  } else if (false == fin_sent() &&
             BufferedDataBytes() == 0) {
    // quic_proxy_curl_ is nullptr and close this stream
    set_fin_sent(true);
    CloseWriteSide();
  }
  return rs;
}



}  // namespace quic
