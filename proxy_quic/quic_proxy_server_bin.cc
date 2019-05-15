// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A binary wrapper for QuicServer.  It listens forever on --port
// (default 6121) until it's killed or ctrl-cd to death.

#include <sys/sysinfo.h>
#include <sys/wait.h>
#include <vector>

#include "base/at_exit.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_default_proof_providers.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_flags.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_socket_address.h"
#include "net/third_party/quiche/src/quic/proxy_quic/quic_proxy_backend.h"
#include "net/third_party/quiche/src/quic/proxy_quic/quic_proxy_server.h"

DEFINE_QUIC_COMMAND_LINE_FLAG(int32_t,
                              port,
                              6121,
                              "The port the quic server will listen on.");

DEFINE_QUIC_COMMAND_LINE_FLAG(
    std::string,
    quic_proxy_backend_url,
    "",
    "<http/https>://<hostname_ip>:<port_number>"
    "The URL for the single backend server hostname"
    "For example, \"http://xyz.com:80\"");


DEFINE_QUIC_COMMAND_LINE_FLAG(bool,
                              daemon,
                              false,
                              "The daemon runing.");


static void worker();

int main(int argc, char* argv[]) {
  base::AtExitManager exit_manager;
  
  const char* usage = "Usage: quic_server [options]";
  std::vector<std::string> non_option_args =
    quic::QuicParseCommandLineFlags(usage, argc, argv);
  if (!non_option_args.empty()) {
    quic::QuicPrintCommandLineFlagHelp(usage);
    exit(0);
  }

  logging::LoggingSettings settings;
  settings.logging_dest = logging::LOG_TO_SYSTEM_DEBUG_LOG;
  CHECK(logging::InitLogging(settings));

  // SetQuicReloadableFlag(quic_default_to_bbr, true);
  
  // // test
  // worker();
  // return 0;
  
  if (true == GetQuicFlag(FLAGS_daemon)) {
    if (0 !=daemon(1, 1) ) {
      exit(0);
    }
  }
  
  int cpu_count = ::get_nprocs();
  for(int i = 0; i < cpu_count; i++) {
    pid_t pid = ::fork();
    if (-1 == pid) {
      exit(0);
    } else if (0 == pid) {
      cpu_set_t mask;
      CPU_ZERO(&mask);
      CPU_SET(i,&mask);
      ::sched_setaffinity(0, sizeof(mask), &mask);
      worker();
      exit(0);
    }
  }

  int wstatus;
  while (true) {
    pid_t pid = ::wait(&wstatus);
    if (-1 == pid) {
      continue;
    }
    pid = ::fork();
    if (0 == pid) {
      worker();
    }
  }

  return 0;
}


static void worker() {
  // init global curl
  curl_global_init(CURL_GLOBAL_ALL);

  quic::QuicProxyBackend proxy_backend;
  if (!GetQuicFlag(FLAGS_quic_proxy_backend_url).empty()) {
    proxy_backend.InitializeBackend(
        GetQuicFlag(FLAGS_quic_proxy_backend_url));
  }

  quic::QuicProxyServer server(quic::CreateDefaultProofSource(),
                          &proxy_backend);

  if (!server.CreateUDPSocketAndListen(quic::QuicSocketAddress(
          quic::QuicIpAddress::Any6(), GetQuicFlag(FLAGS_port)))) {
    return;
  }

  proxy_backend.InitializeCURLM();

  while (true) {
    server.WaitForEvents();
  }

  curl_global_cleanup();
}
