#include "net/third_party/quiche/src/quic/proxy_quic/quic_proxy_curl.h"
#include "net/third_party/quiche/src/quic/proxy_quic/quic_proxy_backend.h"
#include "net/third_party/quiche/src/quic/tools/quic_simple_dispatcher.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_text_utils.h"

namespace quic {

QuicProxyCurl::QuicProxyCurl(const std::string& request_url,
                             QuicProxyBackend* backend,
                             QuicSpdyStream* quic_stream)
  : sockfd_(-1), easy_(nullptr), request_url_(request_url),
    backend_(backend), quic_stream_(quic_stream),
    content_length_(0),
    had_send_length_(0),
    requset_header_list_(nullptr) {
  error_[0] = '\0';
}


QuicProxyCurl::~QuicProxyCurl() {
  if (easy_) {
    curl_easy_cleanup(easy_);
    easy_ = nullptr;
    sockfd_ = -1;
    quic_stream_ = nullptr;
  }

  if (requset_header_list_) {
    curl_slist_free_all(requset_header_list_);
    requset_header_list_ = nullptr;
  }
}

bool QuicProxyCurl::InitializeProxyCurl() {
  easy_ = curl_easy_init();
  if (!easy_) {
    LOG(WARNING) << "curl_easy_init() failed, exiting!";
    return false;
  }
  
  return true;
}

void QuicProxyCurl::StartHttp(const spdy::SpdyHeaderBlock& request_headers) {
  // header
  for (auto header = request_headers.begin();
       header != request_headers.end();
       ++header) {
    if (header->first[0] == ':')
      continue;

    std::string h = std::string(header->first) +
                    ": " + std::string(header->second);
    requset_header_list_ =
      curl_slist_append(requset_header_list_, h.c_str());
  }

  curl_easy_setopt(easy_, CURLOPT_URL,
                   request_url_.c_str());
  curl_easy_setopt(easy_, CURLOPT_HTTPHEADER,
                   requset_header_list_);
  curl_easy_setopt(easy_, CURLOPT_WRITEFUNCTION,
                   QuicProxyCurl::WriteQuicCallback);
  curl_easy_setopt(easy_, CURLOPT_WRITEDATA, this);
  curl_easy_setopt(easy_, CURLOPT_HEADERFUNCTION,
                   QuicProxyCurl::HttpHeaderCallback);
  curl_easy_setopt(easy_, CURLOPT_HEADERDATA, this);
  curl_easy_setopt(easy_, CURLOPT_VERBOSE, 1L);
  curl_easy_setopt(easy_, CURLOPT_ERRORBUFFER, error_);
  curl_easy_setopt(easy_, CURLOPT_PRIVATE, this);
  curl_easy_setopt(easy_, CURLOPT_NOPROGRESS, 0L);
  // curl_easy_setopt(easy_, CURLOPT_PROGRESSFUNCTION, prog_cb);
  // curl_easy_setopt(easy_, CURLOPT_PROGRESSDATA, this);
  curl_easy_setopt(easy_, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(easy_, CURLOPT_LOW_SPEED_TIME, 3L);
  curl_easy_setopt(easy_, CURLOPT_LOW_SPEED_LIMIT, 10L);
  DVLOG(1)
    << "Adding easy " << easy_ << " to " << request_url_;
  curl_multi_add_handle(backend_->get_curlm(), easy_);  
}
  
void QuicProxyCurl::OnEvent(int fd, QuicEpollEvent* event) {
  DCHECK_EQ(fd, sockfd_);
  CURLMcode rc;
  struct itimerspec its;

  int action = (event->in_events & EPOLLIN ? CURL_CSELECT_IN : 0) |
               (event->in_events & EPOLLOUT ? CURL_CSELECT_OUT : 0);

  rc = curl_multi_socket_action(backend_->get_curlm(),
                     fd, action, &backend_->still_running_);
  // mcode_or_die("event_cb: curl_multi_socket_action", rc);

  backend_->CheckCurlMultiInfo();
  if (backend_->still_running_ <= 0) {
    // fprintf(MSG_OUT, "last transfer done, kill timeout\n");
    memset(&its, 0, sizeof(struct itimerspec));
    timerfd_settime(backend_->get_curl_timer()->get_timerfd(), 0, &its, NULL);
  }
}

size_t QuicProxyCurl::WriteQuicCallback(void *contents,
                                          size_t size,
                                          size_t nmemb,
                                          void *userp) {
  QuicProxyCurl* proxy = reinterpret_cast<QuicProxyCurl*>(userp);
  if (proxy->quic_stream_ == NULL) {
    LOG(INFO) << "Send data, but quic_stream is nullptr.";
    return 0;
  }
  
  size_t real_size = size * nmemb;
  proxy->had_send_length_ += real_size;
  QuicStringPiece body(reinterpret_cast<char*>(contents), real_size);
  proxy->quic_stream_->WriteOrBufferBody(body,
                      proxy->had_send_length_ == proxy->content_length_);
  // std::cout << "WriteQuicCallback " << real_size << " total recv size " << proxy->had_send_length_  << " content_length "
  //           << proxy->content_length_  << "\n" ;
  return real_size;
}

size_t QuicProxyCurl::HttpHeaderCallback(void *contents,
                                        size_t size,
                                        size_t nmemb,
                                        void *userp) {
  QuicProxyCurl* proxy = reinterpret_cast<QuicProxyCurl*>(userp);
  if (proxy->quic_stream_ == nullptr) {
    return 0;
  }
  
  size_t real_size = size * nmemb;
  char *header_line = reinterpret_cast<char*>(contents);
  if (real_size == 2 &&
      header_line[0] == '\r' &&
      header_line[1] == '\n') {
    // read header over and send
    auto content_length = proxy->spdy_headers_.find("content-length");
    if (content_length != proxy->spdy_headers_.end()) {
      QuicTextUtils::StringToSizeT(content_length->second,
                                   &proxy->content_length_);
    }
    
    ((proxy->quic_stream_))->WriteHeaders(std::move(proxy->spdy_headers_),
                                          false, nullptr);
    return real_size;
  }

  QuicStringPiece line(header_line, real_size - 2);

  if (line.substr(0, 4) == "HTTP") {
    size_t pos = line.find(" ");
    if (pos == std::string::npos) {
      QUIC_LOG(DFATAL) << "Headers invalid or empty, ignoring: "
                       << proxy->request_url_;
    } else {
      proxy->spdy_headers_[":status"] = line.substr(pos + 1, 3);
    }
    return real_size;
  }

  // Headers are "key: value".
  size_t pos = line.find(": ");
  if (pos == std::string::npos) {
    QUIC_LOG(DFATAL) << "Headers invalid or empty, ignoring: "
                     << proxy->request_url_;
    return real_size;
  }
  proxy->spdy_headers_.AppendValueOrAddHeader(
              QuicTextUtils::ToLower(line.substr(0, pos)), line.substr(pos + 2));
  return real_size;
}

}  // namespace quic
