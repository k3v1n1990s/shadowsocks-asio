#ifndef __BASIC_STREAM_SESSION_H__
#define __BASIC_STREAM_SESSION_H__

#include <utility>
#include <sstream>
#include <boost/asio.hpp>

#include <common_utils/util.h>
#include <cares_service/cares.hxx>

#include "protocol_hooks/basic_protocol.h"

class BasicStreamSession {
protected:
    typedef boost::asio::ip::tcp tcp;
    using resolver_type = cares::tcp::resolver;

public:
    BasicStreamSession(tcp::socket socket,
                       std::unique_ptr<BasicProtocol> protocol,
                       std::shared_ptr<resolver_type> resolver,
                       size_t ttl = 5000)
        : context_(socket.get_executor().context()),
          client_(std::move(socket), ttl), target_(context_, ttl),
          resolver_(resolver), protocol_(std::move(protocol)) {
    }

    ~BasicStreamSession() = default;

    void Close() {
        VLOG(1) << "Closing: " << client_.socket.remote_endpoint();
        client_.CancelAll();
        target_.CancelAll();
        resolver_->cancel();
    }

    std::string DumpToStr() const {
        boost::system::error_code ec;
        std::ostringstream oss;
        oss << client_.socket.remote_endpoint(ec);
        if (ec) {
            oss << "(closed)";
        }
        oss << " <-> ";
        oss << target_.socket.remote_endpoint(ec);
        if (ec) {
            oss << "(closed)";
        }
        return oss.str();
    }

protected:
    using AfterConnected = std::function<void(void)>;

    template<typename Self, typename Port>
    void DoResolveTarget(Self self, std::string host, Port port, AfterConnected cb) {
        VLOG(2) << "Resolving to " << host << ":" << port;
        resolver_->async_resolve(
            std::move(host), std::move(port),
            [this, self, cb = std::move(cb)]
            (boost::system::error_code ec, resolver_type::results_type results) {
                if (ec) {
                    VLOG(1) << "Unable to resolve: " << ec.message();
                    client_.CancelAll();
                    return;
                }
                client_.timer.cancel();
                DoConnectTarget(self, std::move(results), std::move(cb));
            }
        );
        TimerAgain(self, client_);
    }

    template<class Self, class EndpointSequence>
    void DoConnectTarget(Self self, const EndpointSequence &results, AfterConnected cb) {
        boost::asio::async_connect(
            target_.socket, results,
            [this, self, cb = std::move(cb)](boost::system::error_code ec, tcp::endpoint ep) {
                if (ec) {
                    if (ec == boost::asio::error::operation_aborted) {
                        VLOG(1) << "Connect canceled";
                        return;
                    }
                    LOG(INFO) << "Cannot connect to remote: " << ec.message();
                    client_.CancelAll();
                    return;
                }
                client_.timer.cancel();
                VLOG(1) << "Connected to remote " << ep;
                cb();
            }
        );
        TimerAgain(self, client_);
    }

    template<typename Self>
    void DoRelayStream(Self self, Peer &src, Peer &dest, BasicProtocol::Wrapper wrapper) {
        src.socket.async_read_some(
            src.buf.GetBuffer(),
            [this, self, &src, &dest,
             wrapper = std::move(wrapper)](boost::system::error_code ec, size_t len) {
                if (ec) {
                    if (ec == boost::asio::error::misc_errors::eof) {
                        VLOG(2) << "Stream terminates normally";
                        src.CancelAll();
                        dest.CancelAll();
                        return;
                    } else if (ec == boost::asio::error::operation_aborted) {
                        VLOG(1) << "Read operation canceled";
                        return;
                    }
                    LOG(WARNING) << "Relay read unexcepted error: " << ec.message();
                    src.CancelAll();
                    dest.CancelAll();
                    return;
                }
                src.timer.cancel();
                src.buf.Append(len);
                ssize_t valid_length = wrapper(src.buf);
                if (valid_length == 0) { // need more
                    DoRelayStream(self, src, dest, std::move(wrapper));
                    return;
                } else if (valid_length < 0) { // error occurs
                    boost::system::error_code ep_ec;
                    LOG(WARNING) << "Protocol hook error, remote ep: " << src.socket.remote_endpoint(ep_ec);
                    if (ep_ec) {
                        LOG(INFO) << "cannot get error endpoint, " << ep_ec.message();
                    }
                    src.CancelAll();
                    dest.CancelAll();
                    return;
                }
                boost::asio::async_write(dest.socket,
                    src.buf.GetConstBuffer(),
                    [this, self, &src, &dest, wrapper = std::move(wrapper)]
                    (boost::system::error_code ec, size_t len) {
                        if (ec) {
                            if (ec == boost::asio::error::operation_aborted) {
                                VLOG(1) << "Write operation canceled";
                                return;
                            }
                            LOG(WARNING) << "Relay write unexcepted error: " << ec.message();
                            src.CancelAll();
                            dest.CancelAll();
                            return;
                        }
                        dest.timer.cancel();
                        src.buf.Reset();
                        DoRelayStream(self, src, dest, std::move(wrapper));
                    }
                );
                TimerAgain(self, dest);
            }
        );
        TimerAgain(self, src);
    }

    void TimerExpiredCallBack(Peer &peer, boost::system::error_code ec) {
        if (ec != boost::asio::error::operation_aborted) {
            if (peer.socket.is_open()) {
                VLOG(1) << peer.socket.remote_endpoint() << " TTL expired";
            } else {
                LOG(WARNING) << "timer of closed socket expired!";
            }
            client_.CancelAll();
            target_.CancelAll();
            resolver_->cancel();
        }
    }

    template<typename Self>
    void TimerAgain(Self self, Peer &peer) {
        peer.timer.expires_from_now(peer.ttl);
        peer.timer.async_wait(
            std::bind(&BasicStreamSession::TimerExpiredCallBack,
                      self, std::ref(peer),
                      std::placeholders::_1
            )
        );
    }

    boost::asio::io_context &context_;
    Peer client_;
    Peer target_;
    std::shared_ptr<resolver_type> resolver_;
    std::unique_ptr<BasicProtocol> protocol_;
};

#endif

