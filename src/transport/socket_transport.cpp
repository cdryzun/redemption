/*
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

   Product name: redemption, a FLOSS RDP proxy
   Copyright (C) Wallix 2013
   Author(s): Christophe Grosjean, Raphael Zhou, Meng Tan

   Transport layer abstraction, socket implementation with TLS support
*/

#include "transport/socket_transport.hpp"
#include "utils/netutils.hpp"
#include "utils/hexdump.hpp"
#include "utils/select.hpp"
#include "system/tls_context.hpp"
#include "utils/to_timeval.hpp"

#include <vector>
#include <cstring>
#include <memory>

#include <sys/types.h>
#include <sys/socket.h>


namespace
{
    ssize_t tls_recv_all(TLSContext & tls, uint8_t * data, size_t const len);
    ssize_t socket_recv_all(int sck, char const * name, uint8_t * data, size_t const len,
        std::chrono::milliseconds recv_timeout);
    ssize_t socket_recv_partial(int sck, uint8_t * data, size_t const len);
    ssize_t socket_send_partial(TLSContext* tls, int sck, const uint8_t * data, size_t len);

    void socket_transport_log(
        char const* meta_prefix, char const* dump_message_prefix,
        SocketTransport::Verbose verbose, bytes_view data, char const* name, int sck)
    {
        LOG(LOG_INFO, "%s on %s (%d) got %zu bytes", meta_prefix, name, sck, data.size());
        if (bool(verbose & SocketTransport::Verbose::dump)) {
            hexdump_c(data);
            LOG(LOG_INFO, " %s on %s (%d) of %zu bytes", dump_message_prefix, name, sck, data.size());
        }
    }
} // anonymous namespace

SocketTransport::SocketTransport(
    Name name, unique_fd sck, chars_view ip_address, int port,
    std::chrono::milliseconds connection_establishment_timeout,
    std::chrono::milliseconds tcp_user_timeout,
    std::chrono::milliseconds recv_timeout,
    Verbose verbose
)
    : sck(sck.release())
    , name(name.name)
    , port(port)
    , tls(nullptr)
    , recv_timeout(recv_timeout)
    , connection_establishment_timeout(connection_establishment_timeout)
    , tcp_user_timeout(tcp_user_timeout)
    , verbose(verbose)
{
    LOG_IF(bool(verbose & Verbose::basic), LOG_INFO,
        "SocketTransport: recv_timeout=%zu", size_t(recv_timeout.count()));

    auto minlen = std::min(ip_address.size(), sizeof(this->ip_address) - 1);
    memcpy(this->ip_address, ip_address.data(), minlen);
    this->ip_address[minlen] = 0;
}

SocketTransport::~SocketTransport()
{
    if (this->sck > INVALID_SOCKET){
        this->disconnect(); /*NOLINT(clang-analyzer-optin.cplusplus.VirtualCall)*/
    }

    this->tls.reset();

    LOG_IF(bool(verbose & Verbose::basic), LOG_INFO
      , "%s (%d): total_received=%lld, total_sent=%lld"
      , this->name, this->sck, this->total_received, this->total_sent);
}

bool SocketTransport::has_tls_pending_data() const
{
    return this->tls && this->tls->pending_data();
}


bool SocketTransport::has_data_to_write() const
{
    return !this->async_buffers.empty();
}

u8_array_view SocketTransport::get_public_key() const
{
    return this->tls ? this->tls->get_public_key() : nullptr;
}

Transport::TlsResult SocketTransport::enable_server_tls(const char * certificate_password, TlsConfig const& tls_config)
{
    auto process = [&, this] () -> TlsResult {
        switch (this->tls_state) {
            case TLSState::Uninit:
                if (this->has_data_to_write()) {
                    return TlsResult::Want;
                }
                LOG(LOG_INFO, "SocketTransport::enable_server_tls() start (%s)", this->name);
                this->tls = std::make_unique<TLSContext>(bool(this->verbose & Verbose::basic));
                if (!this->tls->enable_server_tls_start(this->sck, certificate_password, tls_config)) {
                    return TlsResult::Fail;
                }
                this->tls_state = TLSState::Want;
                [[fallthrough]];
            case TLSState::Want: {
                TlsResult ret = this->tls->enable_server_tls_loop(tls_config.show_common_cipher_list);
                if (ret == TlsResult::Ok) {
                    LOG(LOG_INFO, "SocketTransport::enable_server_tls() done (%s)", this->name);
                    this->tls_state = TLSState::Ok;
                }
                return ret;
            }
            case TLSState::Ok:
            case TLSState::WaitCertCb:
                return TlsResult::Fail;
        }
        REDEMPTION_UNREACHABLE();
    };

    auto reset_tls = [this]{
        this->tls_state = TLSState::Uninit;
        // Disconnect tls if needed
        this->tls.reset();
        LOG(LOG_ERR, "SocketTransport::enable_server_tls() failed");
    };

    try {
        auto res = process();
        if (res == TlsResult::Fail) {
            reset_tls();
        }
        return res;
    }
    catch (...) {
        reset_tls();
        throw;
    }
}

Transport::TlsResult SocketTransport::enable_client_tls(
    CertificateChecker certificate_checker,
    TlsConfig const& tls_config, AnonymousTls anonymous_tls)
{
    auto process = [&, this] () -> TlsResult {
        switch (this->tls_state) {
            case TLSState::Uninit:
                if (this->has_data_to_write()) {
                    return TlsResult::Want;
                }
                LOG(LOG_INFO, "SocketTransport::enable_client_tls() start (%s)", this->name);
                this->tls = std::make_unique<TLSContext>(bool(this->verbose & Verbose::basic));
                if (!this->tls->enable_client_tls_start(this->sck, tls_config)) {
                    return TlsResult::Fail;
                }
                this->tls_state = TLSState::Want;
                [[fallthrough]];
            case TLSState::Want: {
                TlsResult ret = this->tls->enable_client_tls_loop();
                if (ret == TlsResult::Ok) {
                    ret = this->tls->check_certificate(
                        certificate_checker,
                        this->ip_address, this->port, bool(anonymous_tls));
                    switch (ret) {
                        case TlsResult::Ok:
                            LOG(LOG_INFO, "SocketTransport::enable_client_tls() done");
                            this->tls_state = TLSState::Ok;
                            break;
                        case TlsResult::Want:
                        case TlsResult::Fail:
                            return TlsResult::Fail;
                        case TlsResult::WaitExternalEvent:
                            this->tls_state = TLSState::WaitCertCb;
                    }
                }
                return ret;
            }
            case TLSState::Ok:
                return TlsResult::Fail;
            case TLSState::WaitCertCb:
                switch (this->tls->certificate_external_validation(
                    certificate_checker, this->ip_address, this->port
                )) {
                    case TlsResult::Ok:
                        LOG(LOG_INFO, "SocketTransport::enable_client_tls() done");
                        this->tls_state = TLSState::Ok;
                        return TlsResult::Ok;
                    case TlsResult::Fail:
                    case TlsResult::Want:
                    case TlsResult::WaitExternalEvent:
                        break;
                }
                return TlsResult::Fail;
        }
        REDEMPTION_UNREACHABLE();
    };

    auto reset_tls = [this]{
        this->tls_state = TLSState::Uninit;
        // Disconnect tls if needed
        this->tls.reset();
        LOG(LOG_ERR, "SocketTransport::enable_client_tls() failed");
    };

    try {
        auto res = process();
        if (res == TlsResult::Fail) {
            reset_tls();
        }
        return res;
    }
    catch (...) {
        reset_tls();
        throw;
    }
}

bool SocketTransport::disconnect()
{
    // silent trace in the case of watchdog
    LOG_IF(!bool(this->verbose & Verbose::watchdog), LOG_INFO, "Socket %s (%d) : closing connection", this->name, this->sck);
    this->tls_state = TLSState::Uninit;
    // Disconnect tls if needed
    this->tls.reset();
    shutdown(this->sck, SHUT_RDWR);
    close(this->sck);
    this->sck = INVALID_SOCKET;
    return true;
}

bool SocketTransport::connect()
{
    if (this->sck <= INVALID_SOCKET){
        this->sck = ip_connect(this->ip_address,
                               this->port,
                               this->connection_establishment_timeout,
                               this->tcp_user_timeout)
            .release();
    }
    return this->sck > INVALID_SOCKET;
}

size_t SocketTransport::do_partial_read(uint8_t * buffer, size_t len)
{
    LOG_IF(bool(this->verbose & (Verbose::meta | Verbose::dump)), LOG_INFO,
        "SocketTransport::do_partial_read: %s (%d) asking for %zu bytes", this->name, this->sck, len);

    ssize_t const res = this->tls
      ? this->tls->privpartial_recv_tls(buffer, len)
      : socket_recv_partial(this->sck, buffer, len);

    if (REDEMPTION_UNLIKELY(res < 0)) {
        LOG_IF(!bool(this->verbose & Verbose::watchdog), LOG_ERR,
            "SocketTransport::do_partial_read: Failed to read from socket %s!", this->name);
        throw Error(ERR_TRANSPORT_NO_MORE_DATA, 0, this->sck);
    }

    this->total_received += res;
    std::size_t result_len = static_cast<std::size_t>(res);

    if (REDEMPTION_UNLIKELY(bool(this->verbose & (Verbose::meta | Verbose::dump)))) {
        socket_transport_log(
            "SocketTransport::do_partial_read: Recv done", "Dump done",
            this->verbose, {buffer, result_len}, this->name, this->sck
        );
    }

    return result_len;
}

SocketTransport::Read SocketTransport::do_atomic_read(uint8_t * buffer, size_t len)
{
    LOG_IF(bool(this->verbose & (Verbose::meta | Verbose::dump)), LOG_INFO,
        "SocketTransport::do_atomic_read: %s (%d) asking %zu bytes", this->name, this->sck, len);

    ssize_t res = this->tls
      ? tls_recv_all(*this->tls, buffer, len)
      : socket_recv_all(this->sck, this->name, buffer, len, this->recv_timeout);

    // we properly reached end of file on a block boundary
    if (res == 0) {
        return Read::Eof;
    }

    if (REDEMPTION_UNLIKELY(res < 0 || static_cast<size_t>(res) < len)) {
        LOG(LOG_ERR, "SocketTransport::do_atomic_read: %s to read from socket %s!",
            (res < 0) ? "Failed" : "Insufficient data", this->name);
        throw Error(ERR_TRANSPORT_NO_MORE_DATA, 0, this->sck);
    }

    if (REDEMPTION_UNLIKELY(bool(this->verbose & (Verbose::meta | Verbose::dump)))) {
        socket_transport_log(
            "SocketTransport::do_atomic_read: Recv done", "Dump done",
            this->verbose, {buffer, len}, this->name, this->sck
        );
    }

    this->total_received += res;
    return Read::Ok;
}

SocketTransport::AsyncBuf::AsyncBuf(const uint8_t* data, std::size_t len)
: data([](const uint8_t* data, std::size_t len){
        uint8_t * tmp = new uint8_t[len]; /*NOLINT*/
        memcpy(tmp, data, len);
        return tmp;
    }(data, len))
, p(this->data.get())
, e(this->p + len)
{
}

void SocketTransport::do_send(const uint8_t * const buffer, size_t const len)
{
    if (len == 0) { return; }

    LOG_IF(bool(this->verbose & (Verbose::meta | Verbose::dump)), LOG_INFO,
        "SocketTransport::do_send: %s (%d) asking for %zu bytes", this->name, this->sck, len);

    if (!this->async_buffers.empty()) {
        LOG_IF(bool(this->verbose & (Verbose::meta | Verbose::dump)), LOG_INFO,
            "SocketTransport::do_send: %s (%d) bufferize data", this->name, this->sck);
        this->async_buffers.emplace_back(buffer, len);
        return;
    }

    ssize_t res = socket_send_partial(this->tls.get(), this->sck, buffer, len);

    if (REDEMPTION_UNLIKELY(res < 0)) {
        LOG_IF(!bool(this->verbose & Verbose::watchdog), LOG_WARNING,
            "SocketTransport::Send failed on %s (%d) errno=%d [%s]",
            this->name, this->sck, errno, strerror(errno));
        throw Error(ERR_TRANSPORT_WRITE_FAILED, 0, this->sck);
    }

    this->total_sent += res;
    std::size_t result_len = static_cast<std::size_t>(res);

    if (REDEMPTION_UNLIKELY(bool(this->verbose & (Verbose::meta | Verbose::dump)))) {
        socket_transport_log(
            "SocketTransport::do_send: Sending done", "Sent dumped",
            this->verbose, {buffer, result_len}, this->name, this->sck
        );
    }

    if (result_len < len) {
        LOG_IF(bool(this->verbose & (Verbose::meta | Verbose::dump)), LOG_INFO,
            "SocketTransport::do_send: %s (%d) bufferize remaining data", this->name, this->sck);
        this->async_buffers.emplace_back(buffer + res, len - result_len);
    }
}

void SocketTransport::send_waiting_data()
{
    assert(not this->async_buffers.empty());

    LOG_IF(bool(this->verbose & (Verbose::meta | Verbose::dump)), LOG_INFO,
        "SocketTransport::send_waiting_data: %s (%d) send bufferized data", this->name, this->sck);

    auto first = begin(this->async_buffers);
    auto last = end(this->async_buffers);

    for (; first != last; ++first) {
        ssize_t const len = first->e - first->p;
        ssize_t res = socket_send_partial(this->tls.get(), this->sck, first->p, checked_int(len));

        // socket closed
        if (res == 0 && first == this->async_buffers.begin()) {
            res = -1;
        }

        if (res < 0) {
            LOG(LOG_WARNING,
                "SocketTransport::Send failed on %s (%d) errno=%d [%s]",
                this->name, this->sck, errno, strerror(errno));
            throw Error(ERR_TRANSPORT_WRITE_FAILED, 0, this->sck);
        }

        this->total_sent += res;
        std::size_t result_len = static_cast<std::size_t>(res);

        if (REDEMPTION_UNLIKELY(bool(this->verbose & (Verbose::meta | Verbose::dump)))) {
            socket_transport_log(
                "SocketTransport::send_waiting_data: Sending done", "Sent dumped",
                this->verbose, {first->p, result_len}, this->name, this->sck
            );
        }

        if (res != len) {
            first->p += res;
            break;
        }
    }

    LOG_IF(bool(this->verbose & (Verbose::meta | Verbose::dump)), LOG_INFO,
        "SocketTransport::send_waiting_data: %s (%d) %zu buffers completed",
        this->name, this->sck, static_cast<std::size_t>(first - begin(this->async_buffers)));

    this->async_buffers.erase(begin(this->async_buffers), first);
}

namespace
{
    ssize_t tls_recv_all(TLSContext & tls, uint8_t * data, size_t const len)
    {
        size_t remaining_len = len;
        while (remaining_len > 0) {
            ssize_t const res = tls.privpartial_recv_tls(data, remaining_len);

            if (REDEMPTION_UNLIKELY(res <= 0)) {
                if (res == 0) {
                    if (len != remaining_len) {
                        LOG(LOG_WARNING, "TLS receive for %zu bytes, ZERO RETURN got %zu",
                            len, len - remaining_len);
                    }
                    return checked_int(remaining_len - len);
                }
                return res;
            }

            remaining_len -= static_cast<std::size_t>(res);
            data += res;
        }

        return checked_int(len);
    }

    ssize_t socket_recv_all(
        int sck, char const* name, uint8_t* data, size_t const len,
        std::chrono::milliseconds recv_timeout)
    {
        size_t remaining_len = len;

        while (remaining_len > 0) {
            ssize_t res = ::recv(sck, data, remaining_len, 0);

            if (REDEMPTION_UNLIKELY(res <= 0)) {
                // no data received, socket closed
                if (res == 0) {
                    // if we were not able to receive the amount of data required, this is an error
                    // not need to process the received data as it will end badly
                    return -1;
                }

                // error, maybe EAGAIN
                if (try_again(errno)) {
                    fd_set fds;
                    struct timeval time = { 0, 100000 };
                    io_fd_zero(fds);
                    io_fd_set(sck, fds);
                    ::select(sck + 1, &fds, nullptr, nullptr, &time);
                    continue;
                }

                if (len != remaining_len) {
                    return checked_int(len - remaining_len);
                }

                // TODO replace this with actual error management, EOF is not even an option for sockets
                return -1;
            }

            // some data received

            remaining_len -= static_cast<std::size_t>(res);
            data += res;
            if (remaining_len) {
                fd_set fds;
                struct timeval time = to_timeval(recv_timeout);
                io_fd_zero(fds);
                io_fd_set(sck, fds);
                int ret = ::select(sck + 1, &fds, nullptr, nullptr, &time);
                if ((ret < 1) ||
                    !io_fd_isset(sck, fds)) {
                    LOG(LOG_ERR, "Recv fails on %s (%d) %zu bytes, ret=%d", name, sck, remaining_len, ret);
                    return -1;
                }
            }
        }

        return static_cast<ssize_t>(len);
    }

    ssize_t socket_recv_partial(int sck, uint8_t* data, size_t const len)
    {
        ssize_t const res = ::recv(sck, data, len, 0);

        if (REDEMPTION_UNLIKELY(res <= 0)) {
            // error, maybe EAGAIN
            if (res == -1) {
                return try_again(errno) ? 0 : -1;
            }

            // no data received, socket closed
            return -1;
        }

        return res;
    }

    ssize_t socket_send_partial(TLSContext* tls, int sck, const uint8_t* data, size_t len)
    {
        if (tls) {
            return tls->privpartial_send_tls(data, len);
        }
        ssize_t const sent = ::send(sck, data, len, 0);
        return sent == -1 && try_again(errno) ? 0 : sent;
    }
} // anonymous namespace
