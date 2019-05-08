#include "net/third_party/quiche/src/quic/proxy_quic/quic_proxy_stream.h"
#include "net/third_party/quiche/src/quic/proxy_quic/quic_proxy_curl.h"

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
  }
  return rs;
}



}  // namespace quic
