#ifndef QUICHE_QUIC_PROXYQUIC_QUIC_PROXY_SESSION_H_
#define QUICHE_QUIC_PROXYQUIC_QUIC_PROXY_SESSION_H_

#include "net/third_party/quiche/src/quic/tools/quic_simple_server_session.h"

namespace quic {


class QuicProxySession : public QuicSimpleServerSession {
 public:
  QuicProxySession(const QuicConfig& config,
                   const ParsedQuicVersionVector& supported_versions,
                   QuicConnection* connection,
                   QuicSession::Visitor* visitor,
                   QuicCryptoServerStream::Helper* helper,
                   const QuicCryptoServerConfig* crypto_config,
                   QuicCompressedCertsCache* compressed_certs_cache,
                   QuicSimpleServerBackend* quic_simple_server_backend);
  QuicProxySession(const QuicProxySession&) = delete;
  QuicProxySession& operator=(const QuicProxySession&) = delete;

  ~QuicProxySession() override;

 protected:
  // QuicSession methods:
  QuicSpdyStream* CreateIncomingStream(QuicStreamId id) override;
  QuicSpdyStream* CreateIncomingStream(PendingStream pending) override;
  QuicSimpleServerStream* CreateOutgoingBidirectionalStream() override;
  QuicSimpleServerStream* CreateOutgoingUnidirectionalStream() override;

};

}  // namespace quic

#endif  // QUICHE_QUIC_PROXYQUIC_QUIC_PROXY_SESSION_H_
