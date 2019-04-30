## 介绍

quic_proxy是一个quic代理服务，接收前端发来的quic协议请求，然后http代理到后端业务服务，例如nginx等。

chromium 项目本身提供了一个测试用的quic服务，但是这个服务是单进程，且http回源需要整个文件下载完才能发给前端，大的视频文件这种方式是不能接受的。
在epoll_quic_server这个demo基础上，基于chromium架构，重新开发了一个支持高并发，可商用的quic to http代理服务，其特点：

- 使用SO_REUSEADDR，提高udp服务使用多核的能力。
- 使用recvmmsg,sendmmsg减少用户态与内核态切换，降低cpu，提高服务性能。
- 使用chromium项目提供的quic协议，保证了总能使用最新版本的quic协议，避免使用其他quic开源项目协议更新慢，实现不完全的苦恼。
- 本项目是c++项目，仅支持linux系统。
- 由于chromium项目过于复杂难读，目前回源使用的是libcurl，未使用chromium项目自带的http代码。
- 仅支持http-get请求，不支持http-chunked。(后面会陆续开发http-post，ftp，chunked等功能)

## 编译步骤

1. 下载chromium，详见： [chromium下载](https://chromium.googlesource.com/chromium/src/+/master/docs/linux_build_instructions.md/)
2. 下载编译[libcurl](https://curl.haxx.se/download.html)，
请使用boringssl编译libcurl，或者 --without-ssl，参考编译选项：./configure --enable-debug --disable-optimize --with-ssl=/path/boringssl --enable-shared=no --disable-pthreads
3. copy代码目录proxy_quic到net/third_party/quiche/src/quic/下。
4. 修改 chromium项目的 net/BUILD.gn 文件，找到 epoll_quic_server 服务配置，在下面追加以下内容：

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
6. ninja -C out/Debug quic_proxy_server （5，6两步具体参数的使用，详见：[Build the QUIC client and server](https://www.chromium.org/quic/playing-with-quic)

## 运行
out/Debug/quic_proxy_server --quic_proxy_backend_url=http://backend-host --certificate_file=/path/you.crt --key_file=/path/you.pkcs8

## 联系
欢迎提出改进或者bug等问题，作者邮箱：sswin0922@163.com, QQ:15543852