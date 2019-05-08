#ifndef QUICHE_QUIC_PROXYQUIC_QUIC_PROXY_DISPATCHER_H_
#define QUICHE_QUIC_PROXYQUIC_QUIC_PROXY_DISPATCHER_H_

#include "net/third_party/quiche/src/quic/tools/quic_simple_dispatcher.h"

namespace quic {

class QuicProxyDispatcher : public QuicSimpleDispatcher {
 public:
  QuicProxyDispatcher(
      const QuicConfig* config,
      const QuicCryptoServerConfig* crypto_config,
      QuicVersionManager* version_manager,
      std::unique_ptr<QuicConnectionHelperInterface> helper,
      std::unique_ptr<QuicCryptoServerStream::Helper> session_helper,
      std::unique_ptr<QuicAlarmFactory> alarm_factory,
      QuicSimpleServerBackend* quic_simple_server_backend,
      uint8_t expected_connection_id_length);

  ~QuicProxyDispatcher() override;

 protected:
  QuicServerSessionBase* CreateQuicSession(
      QuicConnectionId connection_id,
      const QuicSocketAddress& client_address,
      QuicStringPiece alpn,
      const ParsedQuicVersion& version) override;

};

}  // namespace quic

#endif  // QUICHE_QUIC_PROXYQUIC_QUIC_PROXY_DISPATCHER_H_
