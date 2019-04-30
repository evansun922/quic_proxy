## QUIC Proxy

quic_proxy is a high-performance quic proxy service. it receive a  request with quic from frontend and proxy to backend, e.g. nginx, etc.

The chromium provide a demo service of quic, but this demo is a single process, and it download the full file from source station before it can be sent to the frontend, this is not acceptable for big file.

We have rewritten epoll_quic_server based on the chromium， the new quic server support for high concurrency. its features:

- Use SO_REUSEADDR to improve the ability of services to use multiple cores.
- Use recvmmsg,sendmmsg to reduce user mode and kernel mode switching and improve service performance.
- Use quic of chromium, we can always use the latest version of quic.
- This project is a c++ project, support for linux systems only.
- This project use libcurl to backend.
- Currently, this project support http-get only, we will continue to support http-post, chunked, etc.

## Building

1. Download chromium, see [how to download chromium](https://chromium.googlesource.com/chromium/src/+/master/docs/linux_build_instructions.md/).
2. Download and build [libcurl](https://curl.haxx.se/download.html), you must build libcurl with boringssl, or use --without-ssl, refer to: ./configure --enable-debug --disable-optimize --with-ssl=/path/boringssl --enable-shared=no --disable-pthreads .
3. Copy dir "proxy_quic" to "net/third_party/quiche/src/quic/" .
4. Modify the "net/BUILD.gn" in chromium that find the configuration of epoll_quic_server and append the following:

    ```executable("quic_proxy_server") {
    sources = [
      "third_party/quiche/src/quic/proxy_quic/quic_proxy_server_bin.cc",
      "third_party/quiche/src/quic/proxy_quic/quic_proxy_server.cc",
      "third_party/quiche/src/quic/proxy_quic/quic_proxy_backend.cc",
      "third_party/quiche/src/quic/proxy_quic/quic_proxy_curl.cc",
      "third_party/quiche/src/quic/proxy_quic/quic_curl_timer.cc",
      "third_party/quiche/src/quic/proxy_quic/quic_proxy_packet_writer.cc",
      "third_party/quiche/src/quic/proxy_quic/quic_proxy_packet_reader.cc",
      "third_party/quiche/src/quic/proxy_quic/sendmmsgtimer.cc",
    ]
    include_dirs = [
      "/usr/local/include"
    ]
    deps = [
      ":epoll_quic_tools",
      ":epoll_server",
      ":net",
      ":simple_quic_tools",
      "//base",
      "//third_party/boringssl",
    ]
    lib_dirs = [
      "/usr/local/lib"
    ]
    libs = [
      #"curl",
      "/usr/local/lib/libcurl.a",
      "/usr/lib/x86_64-linux-gnu/libz.a",
    ]
   }
   ```

5. cd src; gn gen out/Debug
6. ninja -C out/Debug quic_proxy_server (step 5，6 see [Build the QUIC client and server](https://www.chromium.org/quic/playing-with-quic))

## Running
out/Debug/quic_proxy_server --quic_proxy_backend_url=http://backend-host --certificate_file=/path/you.crt --key_file=/path/you.pkcs8

## Contact
my email: sswin0922@163.com