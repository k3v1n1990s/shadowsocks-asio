
#include <boost/endian/arithmetic.hpp>
#include <common_utils/socks5.h>

#include "ss_proto/server.h"

using boost::asio::ip::tcp;

void ShadowsocksServer::DoReadHeader(Peer &peer, NextStage next, size_t at_least) {
    VLOG(1) << "start reading protocol header";
    boost::asio::async_read(
        peer.socket,
        peer.buf.GetBuffer(),
        boost::asio::transfer_at_least(at_least),
        [this, &peer, next = std::move(next)](boost::system::error_code ec, size_t length) {
            if (ec) {
                if (ec == boost::asio::error::misc_errors::eof
                    || ec == boost::asio::error::operation_aborted) {
                    VLOG(1) << ec.message() << " while initializing protocol";
                } else {
                    LOG(WARNING) << "unexcepted error while initializing protocol: " << ec.message();
                }
                return;
            }

            peer.buf.Append(length);
            ssize_t valid_length = UnWrap(peer.buf);
            if (valid_length == 0) {
                VLOG(2) << length << " bytes read, but need more";
                DoReadHeader(peer, std::move(next));
                return;
            } else if (valid_length < 0) {
                LOG(WARNING) <<  "protocol_hook error";
                return;
            }

            header_buf_.AppendData(peer.buf);
            peer.buf.Reset();

            size_t need_more = socks5::Request::NeedMore(header_buf_.GetData() - 3,
                                                         header_buf_.Size() + 3);
            if (need_more) {
                VLOG(2) << "need more: " << need_more;
                DoReadHeader(peer, std::move(next), need_more);
                return;
            }

            if (ParseHeader(header_buf_, 0) != socks5::SUCCEEDED_REP) {
                LOG(INFO) << "invalid header";
                return;
            }
            header_buf_.DeQueue(header_length_);
            peer.buf.AppendData(header_buf_);
            next();
        }
    );
}

