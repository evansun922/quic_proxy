#include "net/third_party/quiche/src/quic/proxy_quic/quic_proxy_curl.h"
#include "net/third_party/quiche/src/quic/proxy_quic/quic_proxy_backend.h"
#include "net/third_party/quiche/src/quic/proxy_quic/quic_proxy_stream.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_text_utils.h"
#include "net/http/http_util.h"


namespace quic {

QuicProxyCurl::QuicProxyCurl(const std::string& request_url,
                             QuicProxyBackend* backend,
                             QuicProxyStream* quic_stream)
  : sockfd_(-1), easy_(nullptr), request_url_(request_url),
    backend_(backend), quic_stream_(quic_stream),
    stream_id_(quic_stream->id()),
    is_pause_(false),
    content_length_(0),
    had_send_length_(0),
    request_header_list_(nullptr),
    request_body_(""),
    request_body_pos_(0) {
  error_[0] = '\0';
}


QuicProxyCurl::~QuicProxyCurl() {
  if (easy_) {
    curl_easy_cleanup(easy_);
    easy_ = nullptr;
    sockfd_ = -1;
    quic_stream_ = nullptr;
  }

  if (request_header_list_) {
    curl_slist_free_all(request_header_list_);
    request_header_list_ = nullptr;
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

void QuicProxyCurl::StartHttp(const spdy::SpdyHeaderBlock& request_headers,
                              const std::string& request_body) {
  // header
  for (auto header = request_headers.begin();
       header != request_headers.end();
       ++header) {
    if (header->first[0] == ':')
      continue;

    std::string h = std::string(header->first) +
                    ": " + std::string(header->second);
    request_header_list_ =
      curl_slist_append(request_header_list_, h.c_str());
  }
  // X-Real-IP
  std::string x_real_ip = "x-real-ip: " + quic_stream_->get_peer_ip();
  request_header_list_ =
    curl_slist_append(request_header_list_, x_real_ip.c_str());

  std::string method = "";
  auto it = request_headers.find(":method");
  if (it != request_headers.end()) {
    method.append(it->second.as_string());
  }

  if (!net::HttpUtil::IsValidHeaderName(method)) {
    LOG(DFATAL) << "Headers invalid or empty, ignoring: "
                << request_url_
                << " quic stream: " << stream_id_;
    return;
  }

  DVLOG(1) << "http method is " << method
           << " quic stream id " << quic_stream_->id()
           << " url is " << request_url_;
  
  curl_easy_setopt(easy_, CURLOPT_URL,
                   request_url_.c_str());
  curl_easy_setopt(easy_, CURLOPT_CUSTOMREQUEST,
                   method.c_str());
  if (request_header_list_) {
    curl_easy_setopt(easy_, CURLOPT_HTTPHEADER,
                     request_header_list_);
  }
  curl_easy_setopt(easy_, CURLOPT_WRITEFUNCTION,
                   QuicProxyCurl::WriteQuicCallback);
  curl_easy_setopt(easy_, CURLOPT_WRITEDATA, this);
  curl_easy_setopt(easy_, CURLOPT_HEADERFUNCTION,
                   QuicProxyCurl::HttpHeaderCallback);
  curl_easy_setopt(easy_, CURLOPT_HEADERDATA, this);
  curl_easy_setopt(easy_, CURLOPT_ERRORBUFFER, error_);
  curl_easy_setopt(easy_, CURLOPT_PRIVATE, this);
  // curl_easy_setopt(easy_, CURLOPT_PROGRESSFUNCTION,
  //                  QuicProxyCurl::ProgressQuicCallback);
  // curl_easy_setopt(easy_, CURLOPT_PROGRESSDATA, this);
  curl_easy_setopt(easy_, CURLOPT_FOLLOWLOCATION, 1L);
  // curl_easy_setopt(easy_, CURLOPT_LOW_SPEED_TIME, 3L);
  // curl_easy_setopt(easy_, CURLOPT_LOW_SPEED_LIMIT, 10L);
  
  // curl_easy_setopt(easy_, CURLOPT_NOPROGRESS, 0L);
  // curl_easy_setopt(easy_, CURLOPT_VERBOSE, 1L);
  curl_easy_setopt(easy_, CURLOPT_BUFFERSIZE, 1024*32L);
  // curl_easy_setopt(easy_, CURLOPT_MAX_RECV_SPEED_LARGE, (curl_off_t)(1024*1024*2));
  
  if (method == "POST" || method == "PUT" ||
      method == "PATCH") {
    // Upload content must be set
    if (!request_body.empty()) {
      LOG(INFO) << "request_body size is " << request_body.length();
      request_body_ = request_body;
      curl_easy_setopt(easy_, CURLOPT_UPLOAD, 1L);
      curl_easy_setopt(easy_, CURLOPT_INFILESIZE,
                       request_body_.length());
      /* we want to use our own read function */
      curl_easy_setopt(easy_, CURLOPT_READFUNCTION,
                       QuicProxyCurl::ReadQuicCallback);
      /* pointer to pass to our read function */
      curl_easy_setopt(easy_, CURLOPT_READDATA, this);
    }
  }

  DVLOG(1)
    << "Adding easy " << easy_ << " to " << request_url_;
  curl_multi_add_handle(backend_->get_curlm(), easy_);
}

void QuicProxyCurl::ContinueDownload(uint64_t buffer_size) {
  // const uint64_t continue_download_size = 1024*1024*20;
  // if (buffer_size <= continue_download_size &&
  //     is_pause_ == true) {
  //   is_pause_ = false;
  //   curl_easy_pause(easy_, CURLPAUSE_CONT);
  // }
}
  
void QuicProxyCurl::OnEvent(int fd, QuicEpollEvent* event) {
  DCHECK_EQ(fd, sockfd_);
  CURLMcode rc;
  struct itimerspec its;

  int action = (event->in_events & EPOLLIN ? CURL_CSELECT_IN : 0) |
               (event->in_events & EPOLLOUT ? CURL_CSELECT_OUT : 0);

  rc = curl_multi_socket_action(backend_->get_curlm(),
                     fd, action, &backend_->still_running_);

  backend_->CheckCurlMultiInfo();

  if (backend_->still_running_ <= 0) {
    memset(&its, 0, sizeof(struct itimerspec));
    timerfd_settime(backend_->get_curl_timer()->get_timerfd(), 0, &its, NULL);
  }
}

size_t QuicProxyCurl::WriteQuicCallback(void* contents,
                                        size_t size,
                                        size_t nmemb,
                                        void* userp) {
  QuicProxyCurl* proxy = reinterpret_cast<QuicProxyCurl*>(userp);
  if (proxy->quic_stream_ == NULL) {
    LOG(INFO) << "Send data, but quic_stream is nullptr. " << proxy->stream_id_;
    return CURLE_WRITE_ERROR;
  }
  
  size_t real_size = size * nmemb;
  proxy->had_send_length_ += real_size;
  QuicStringPiece body(reinterpret_cast<char*>(contents), real_size);
  proxy->quic_stream_->WriteOrBufferBody(body,
                      proxy->had_send_length_ == proxy->content_length_);
 
  // if (proxy->had_send_length_ == proxy->content_length_) {
  //   LOG(INFO) << "WriteQuicCallback " << real_size
  //             << " total recv size " << proxy->had_send_length_
  //             << " content_length "
  //             << proxy->content_length_
  //             << " quic stream id " << proxy->quic_stream_->id();
  // }
  
  return real_size;
}

size_t QuicProxyCurl::HttpHeaderCallback(void* contents,
                                        size_t size,
                                        size_t nmemb,
                                        void* userp) {
  QuicProxyCurl* proxy = reinterpret_cast<QuicProxyCurl*>(userp);
  if (proxy->quic_stream_ == nullptr) {
    LOG(INFO) << "Send data, but quic_stream is nullptr. " << proxy->stream_id_;
    return CURLE_WRITE_ERROR;
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
    std::string http_status("");
    auto it_status = proxy->spdy_headers_.find(":status");
    if (it_status != proxy->spdy_headers_.end()) {
      http_status = it_status->second.as_string();
    }
    // std::string http_ver("");
    // auto it_ver = proxy->spdy_headers_.find(":ver");
    // if (it_ver != proxy->spdy_headers_.end()) {
    //   http_ver = it_ver->second.as_string();
    //   proxy->spdy_headers_.erase(":ver");
    // }
    std::string transfer_encoding("");
    auto it_transfer_encoding = proxy->spdy_headers_.find("transfer-encoding");
    if (it_transfer_encoding != proxy->spdy_headers_.end()) {
      transfer_encoding = it_transfer_encoding->second.as_string();
    }

    bool fin = false;
    if (proxy->content_length_ == 0) {
      fin = true;
      if (http_status == "200" ||
          http_status == "100" ||
          transfer_encoding == "chunked") {
        fin = false;
      }
    }
        
    // LOG(INFO) << "send request header " << proxy->quic_stream_->id()
    //           << " content-length " << proxy->content_length_
    //           << " fin " << fin;
    ((proxy->quic_stream_))->WriteHeaders(std::move(proxy->spdy_headers_),
                                          fin, nullptr);
    return real_size;
  }

  QuicStringPiece line(header_line, real_size - 2);
  // LOG(INFO) << "Respond header: " << line;

  if (line.substr(0, 4) == "HTTP") {
    size_t pos = line.find(" ");
    if (pos == std::string::npos) {
      LOG(DFATAL) << "Headers invalid or empty, ignoring: "
                       << proxy->request_url_;
      return 0;
    }
    
    proxy->spdy_headers_[":status"] = line.substr(pos + 1, 3);
    // proxy->spdy_headers_[":ver"] = line.substr(5, 3);
    return real_size;
  }

  // Headers are "key: value".
  size_t pos = line.find(": ");
  if (pos == std::string::npos) {
    LOG(DFATAL) << "Headers invalid or empty, ignoring: "
                     << proxy->request_url_;
    return real_size;
  }
  proxy->spdy_headers_.AppendValueOrAddHeader(
              QuicTextUtils::ToLower(line.substr(0, pos)), line.substr(pos + 2));
  return real_size;
}

size_t QuicProxyCurl::ReadQuicCallback(void* dest,
                                        size_t size,
                                        size_t nmemb,
                                        void *userp) {
  QuicProxyCurl* proxy = reinterpret_cast<QuicProxyCurl*>(userp);
  size_t buffer_size = size * nmemb;
  size_t copy_this_much = proxy->request_body_.length()
                          - proxy->request_body_pos_;
  if (copy_this_much > 0) {
    if (copy_this_much > buffer_size)
      copy_this_much = buffer_size;
    memcpy(dest,
           proxy->request_body_.c_str()+proxy->request_body_pos_,
           copy_this_much);
    proxy->request_body_pos_ += copy_this_much;
    return copy_this_much;
  }

  return 0;
}

int QuicProxyCurl::ProgressQuicCallback(void* userp,
                                        double dltotal,
                                        double dlnow,
                                        double ultotal,
                                        double ulnow) {
  QuicProxyCurl* proxy = reinterpret_cast<QuicProxyCurl*>(userp);
  if (proxy->quic_stream_ == nullptr) {
    LOG(INFO) << "Progress data, but quic_stream is nullptr. " << proxy->stream_id_;
    return CURLE_ABORTED_BY_CALLBACK;    
  }

  const uint64_t pause_download_size = 1024*1024*40;
  
  if (proxy->quic_stream_->BufferedDataBytes() >= pause_download_size &&
      proxy->is_pause_ == false) {
    proxy->is_pause_ = true;
    curl_easy_pause(proxy->easy_, CURLPAUSE_RECV);
  }
  return CURLE_OK;
}

}  // namespace quic
