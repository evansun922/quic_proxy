#include "net/third_party/quiche/src/quic/proxy_quic/quic_proxy_dispatcher.h"
#include "net/third_party/quiche/src/quic/proxy_quic/quic_proxy_session.h"

namespace quic {

QuicProxyDispatcher::QuicProxyDispatcher(
    const QuicConfig* config,
    const QuicCryptoServerConfig* crypto_config,
    QuicVersionManager* version_manager,
    std::unique_ptr<QuicConnectionHelperInterface> helper,
    std::unique_ptr<QuicCryptoServerStream::Helper> session_helper,
    std::unique_ptr<QuicAlarmFactory> alarm_factory,
    QuicSimpleServerBackend* quic_simple_server_backend,
    uint8_t expected_connection_id_length)
    : QuicSimpleDispatcher(config,
                           crypto_config,
                           version_manager,
                           std::move(helper),
                           std::move(session_helper),
                           std::move(alarm_factory),
                           quic_simple_server_backend,
                           expected_connection_id_length) {
}

QuicProxyDispatcher::~QuicProxyDispatcher() = default;

QuicServerSessionBase* QuicProxyDispatcher::CreateQuicSession(
    QuicConnectionId connection_id,
    const QuicSocketAddress& client_address,
    QuicStringPiece /*alpn*/,
    const ParsedQuicVersion& version) {
  // The QuicServerSessionBase takes ownership of |connection| below.
  QuicConnection* connection = new QuicConnection(
      connection_id, client_address, helper(), alarm_factory(), writer(),
      /* owns_writer= */ false, Perspective::IS_SERVER,
      ParsedQuicVersionVector{version});

  QuicServerSessionBase* session = new QuicProxySession(
      config(), GetSupportedVersions(), connection, this, session_helper(),
      crypto_config(), compressed_certs_cache(), server_backend());
  session->Initialize();
  return session;
}

}  // namespace quic
