#include "net/third_party/quiche/src/quic/proxy_quic/quic_proxy_session.h"
#include "net/third_party/quiche/src/quic/proxy_quic/quic_proxy_stream.h"


namespace quic {

QuicProxySession::QuicProxySession(
    const QuicConfig& config,
    const ParsedQuicVersionVector& supported_versions,
    QuicConnection* connection,
    QuicSession::Visitor* visitor,
    QuicCryptoServerStream::Helper* helper,
    const QuicCryptoServerConfig* crypto_config,
    QuicCompressedCertsCache* compressed_certs_cache,
    QuicSimpleServerBackend* quic_simple_server_backend)
    : QuicSimpleServerSession(config,
                              supported_versions,
                              connection,
                              visitor,
                              helper,
                              crypto_config,
                              compressed_certs_cache,
                              quic_simple_server_backend) {
}

QuicProxySession::~QuicProxySession() = default;

QuicSpdyStream* QuicProxySession::CreateIncomingStream(QuicStreamId id) {
  if (!ShouldCreateIncomingStream(id)) {
    return nullptr;
  }

  QuicSpdyStream* stream = new QuicProxyStream(
                id, this, BIDIRECTIONAL, server_backend());
  ActivateStream(QuicWrapUnique(stream));
  return stream;
}

QuicSpdyStream* QuicProxySession::CreateIncomingStream(
    PendingStream* pending) {
  QuicSpdyStream* stream = new QuicProxyStream(
                pending, this, BIDIRECTIONAL, server_backend());
  ActivateStream(QuicWrapUnique(stream));
  return stream;
}

QuicSimpleServerStream*
QuicProxySession::CreateOutgoingBidirectionalStream() {
  DCHECK(false);
  return nullptr;
}

QuicSimpleServerStream*
QuicProxySession::CreateOutgoingUnidirectionalStream() {
  if (!ShouldCreateOutgoingUnidirectionalStream()) {
    return nullptr;
  }

  QuicSimpleServerStream* stream = new QuicProxyStream(
      GetNextOutgoingUnidirectionalStreamId(), this, WRITE_UNIDIRECTIONAL,
      server_backend());
  ActivateStream(QuicWrapUnique(stream));
  return stream;
}

}  // namespace quic
