#include <utility>
#include <sys/timerfd.h>

#include "net/third_party/quiche/src/quic/proxy_quic/quic_proxy_backend.h"
#include "net/third_party/quiche/src/quic/proxy_quic/quic_proxy_server.h"
#include "net/third_party/quiche/src/quic/proxy_quic/quic_proxy_stream.h"

using spdy::SpdyHeaderBlock;

namespace quic {

QuicProxyBackend::QuicProxyBackend()
  : still_running_(0),
    cache_initialized_(false),
    multi_curl_(curl_multi_init()),
    server_(nullptr),
    curl_timer_(this){}

QuicProxyBackend::~QuicProxyBackend() {
  if (multi_curl_) {
    curl_multi_cleanup(multi_curl_);
    multi_curl_ = nullptr;
  }
}

bool QuicProxyBackend::InitializeBackend(
       const std::string& backend_url) {
  if (backend_url.empty()) {
    QUIC_BUG << "backend_url must not be empty."; 
    return false;
  }

  backend_url_ = GURL(backend_url);
  // Only Http(s) backend supported
  if (!backend_url_.is_valid() || !backend_url_.SchemeIsHTTPOrHTTPS()) {
    LOG(ERROR) << "QUIC Proxy Backend URL '" << backend_url
               << "' is not valid !";
    return false;
  }

  LOG(INFO)
    << "Successfully configured to run as a QUIC Proxy with Backend URL: "
    << backend_url_.spec();
  
  cache_initialized_ = true;
  return true;
}

bool QuicProxyBackend::IsBackendInitialized() const {
  return cache_initialized_;
}

void QuicProxyBackend::FetchResponseFromBackend(
    const SpdyHeaderBlock& request_headers,
    const std::string& request_body,
    QuicSimpleServerBackend::RequestHandler* quic_stream) {

  for (auto it = request_headers.begin(); it != request_headers.end(); ++it) {
    QUIC_LOG(INFO) << it->first.as_string() << ": " << it->second.as_string();
  }
  
  std::shared_ptr<QuicProxyCurl> proxy;
  auto path = request_headers.find(":path");
  std::string url = backend_url_.spec() +
    std::string(path->second.substr(1, path->second.length()));
  QuicProxyStream* quic_proxy_stream = reinterpret_cast<QuicProxyStream*>(
                         static_cast<QuicSimpleServerStream*>(quic_stream));
  if (false == CreateProxyCurl(url,
                               quic_proxy_stream,
                               proxy)) {
    LOG(WARNING) << "create proxy curl failed.";
  }
  
  quic_proxy_stream->set_proxy_curl(proxy.get());
  proxy->StartHttp(request_headers, request_body);
}

// The memory cache does not have a per-stream handler
void QuicProxyBackend::CloseBackendResponseStream(
    QuicSimpleServerBackend::RequestHandler* quic_stream) {
  QuicProxyStream* quic_proxy_stream = reinterpret_cast<QuicProxyStream*>(
          static_cast<QuicSimpleServerStream*>(quic_stream));
  LOG(INFO) << "CloseBackendResponseStream " << quic_proxy_stream->id();
  QuicProxyCurl* proxy_curl = quic_proxy_stream->get_proxy_curl();
  if (proxy_curl) {
    proxy_curl->set_stream(nullptr);
    quic_proxy_stream->set_proxy_curl(nullptr);
  }
}

void QuicProxyBackend::InitializeCURLM() {
  /* setup the generic multi interface options we want */
  curl_multi_setopt(multi_curl_, CURLMOPT_SOCKETFUNCTION,
                    QuicProxyBackend::CurlSockCB);
  curl_multi_setopt(multi_curl_, CURLMOPT_SOCKETDATA, this);
  curl_multi_setopt(multi_curl_, CURLMOPT_TIMERFUNCTION,
                    QuicProxyBackend::CurlMultiTimerCB);
  curl_multi_setopt(multi_curl_, CURLMOPT_TIMERDATA, this);

  curl_timer_.InitializeCurlTimer();  
}
  
void QuicProxyBackend::CheckCurlMultiInfo() {
  char *eff_url;
  CURLMsg *msg;
  int msgs_left;
  QuicProxyCurl* proxy;
  CURL *easy;
  CURLcode res;
  bool is_clean_dead_proxy = true;
  
  while ((msg = curl_multi_info_read(multi_curl_, &msgs_left))) {
    if(msg->msg == CURLMSG_DONE) {
      easy = msg->easy_handle;
      res = msg->data.result;
      curl_easy_getinfo(easy, CURLINFO_PRIVATE, &proxy);
      curl_easy_getinfo(easy, CURLINFO_EFFECTIVE_URL, &eff_url);
      // DVLOG(1) <<
      LOG(INFO) <<
        "DONE: " << eff_url << " => (" << res << ") stream id: "
                 << proxy->get_stream_id();

      curl_multi_remove_handle(multi_curl_, easy);
      auto proxy_ptr = proxy_http_hash_.find(easy);
      if (proxy_ptr != proxy_http_hash_.end()) {
        proxy_http_dead_list_.push_front(proxy_ptr->second);
        proxy_http_hash_.erase(proxy_ptr);
      }

      if (proxy->get_stream()) {
        proxy->get_stream()->set_proxy_curl(nullptr);
        proxy->set_stream(nullptr);
      }
    }
    is_clean_dead_proxy = false;
  }

  if (is_clean_dead_proxy) {
    proxy_http_dead_list_.clear();
  }
}



int QuicProxyBackend::CurlSockCB(CURL *e,
                                 curl_socket_t s,
                                 int what,
                                 void *cbp,
                                 void *sockp) {
  QuicProxyBackend *backend = reinterpret_cast<QuicProxyBackend*>(cbp);
  QuicProxyCurl* proxy = reinterpret_cast<QuicProxyCurl*>(sockp);
  const char *what_str[] = { "none", "IN", "OUT", "INOUT", "REMOVE" };

  DVLOG(1)
  // LOG(INFO)
    << "stream id: " << (proxy ? proxy->get_stream_id():0)
    << " socket callback: s=" << s << " e="
    << e << " what=" << what_str[what];

  if (what == CURL_POLL_REMOVE) {
    backend->CurlRemoveSock(proxy);
  } else {
    if (!proxy) {
      backend->CurlAddSock(s, e, what);
    } else {
      backend->CurlSetSock(proxy, s, e, what);
    }
  }
  return 0;
}

void QuicProxyBackend::CurlRemoveSock(QuicProxyCurl* proxy) {
  if (proxy) {
    if (proxy->get_sockfd() != -1) {
      server_->epoll_server()->UnregisterFD(proxy->get_sockfd());
    }
  }
}

void QuicProxyBackend::CurlAddSock(curl_socket_t s,
                                         CURL *easy,
                                         int action) {
  auto proxy_iterator = proxy_http_hash_.find(easy);
  if (proxy_iterator == proxy_http_hash_.end()) {
    LOG(WARNING) << "Can not find proxy_iterator in proxy_http_hash_."; 
    return;
  }

  QuicProxyCurl* proxy = proxy_iterator->second.get();
  proxy->set_sockfd(s);
  server_->epoll_server()->RegisterFD(s, proxy,
                         EPOLLIN | EPOLLOUT);
  CurlSetSock(proxy, s, easy, action);
  curl_multi_assign(multi_curl_, s, proxy);
}

void QuicProxyBackend::CurlSetSock(QuicProxyCurl* proxy,
                                         curl_socket_t s,
                                         CURL *e, int act) {
  if (act & CURL_POLL_IN) {
    server_->epoll_server()->StartRead(s);
  } else {
    server_->epoll_server()->StopRead(s);
  }

  if (act & CURL_POLL_OUT) {
    server_->epoll_server()->StartWrite(s);
  } else {
    server_->epoll_server()->StopWrite(s);
  }
}

int QuicProxyBackend::CurlMultiTimerCB(CURLM *multi,
                                       long timeout_ms,
                                       QuicProxyBackend *backend) {
  struct itimerspec its;

  if (timeout_ms > 0) {
    its.it_interval.tv_sec = 1;
    its.it_interval.tv_nsec = 0;
    its.it_value.tv_sec = timeout_ms / 1000;
    its.it_value.tv_nsec = (timeout_ms % 1000) * 1000 * 1000;
  } else if (timeout_ms == 0) {
    /* libcurl wants us to timeout now, however setting both fields of
     * new_value.it_value to zero disarms the timer. The closest we can
     * do is to schedule the timer to fire in 1 ns. */
    its.it_interval.tv_sec = 1;
    its.it_interval.tv_nsec = 0;
    its.it_value.tv_sec = 0;
    its.it_value.tv_nsec = 1;
  } else {
    memset(&its, 0, sizeof(struct itimerspec));
  }
 
  timerfd_settime(backend->curl_timer_.get_timerfd(), 0, &its, NULL);
  return 0;
}
  
bool QuicProxyBackend::CreateProxyCurl(
                          const std::string& request_url,
                          QuicProxyStream* quic_stream,
                          std::shared_ptr<QuicProxyCurl>& proxy) {
  auto proxy_ptr = std::make_shared<QuicProxyCurl>(request_url, this, quic_stream);
  if (false == proxy_ptr->InitializeProxyCurl()) {
    return false;
  }

  proxy = proxy_ptr;
  proxy_http_hash_[proxy->get_curl()] = proxy;
  return true;
}
  



  
}  // namespace quic
