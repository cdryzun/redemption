/*
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

   Product name: redemption, a FLOSS RDP proxy
   Copyright (C) Wallix 2010
   Author(s): Christophe Grosjean, Meng Tan

   Early Transport Protocol Security Negotiation stage

*/

#pragma once

#include "configs/autogen/enums.hpp"
#include "transport/transport.hpp"
#include "utils/verbose_flags.hpp"
#include "translation/translation.hpp"
#include "mod/rdp/rdp_verbose.hpp"

#include <memory>
#include <vector>

class rdpClientNTLM;
#ifndef __EMSCRIPTEN__
class rdpCredsspClientKerberos;
#endif
class Random;
class TimeBase;
class InStream;
class TpduBuffer;

struct RdpNego
{
public:
    enum {
        EXTENDED_CLIENT_DATA_SUPPORTED = 0x01
    };

    const bool tls;
    const bool nla;
    const bool nla_ntlm_fallback;
    const bool tls_only;
    const bool rdp_legacy;

private:
    const bool krb;
    bool krb_state = krb;
public:
    const bool restricted_admin_mode;

private:
    bool nla_tried = false;

public:
    uint32_t selected_protocol = 0;

private:
    uint32_t enabled_protocols;
    std::string username;

    std::string hostname;
    std::string user;
    std::string service_user;
    uint8_t password[2048];
    uint8_t service_password[2048];
    std::string keytab_path;
    std::string service_keytab_path;
    std::vector<uint8_t> domain;
    const char * target_host;

    uint8_t * current_password;
    uint8_t * current_service_password;
    Random & rand;
    const TimeBase & time_base;
    char * lb_info;

    std::unique_ptr<rdpClientNTLM> NTLM;
    #ifndef __EMSCRIPTEN__
    std::unique_ptr<rdpCredsspClientKerberos> credsspKerberos;
    #endif

    enum class [[nodiscard]] State
    {
        Negotiate,
        SslHybrid,
        Tls,
        Credssp,
        Final,
    };

    State state = State::Negotiate;

public:
    TlsConfig tls_config;

    REDEMPTION_VERBOSE_FLAGS(private, verbose)
    {
        none,
        credssp     = underlying_cast(RDPVerbose::credssp),
        negotiation = underlying_cast(RDPVerbose::negotiation),
    };

    [[nodiscard]] bool enhanced_rdp_security_is_in_effect() const;
    void set_lb_info(uint8_t * lb_info, size_t lb_info_length);

    RdpNego(
        std::string_view username, const char * target_host,
        bool nla, const bool krb, const bool nla_ntlm,
        const bool tls_only, const bool rdp_legacy, bool admin_mode,
        Random & rand, const TimeBase & time_base,
        TlsConfig const& tls_config, const Verbose verbose);

    ~RdpNego();

    /**
     * \brief Set the identity used for authenticating.
     *
     * \param[in] username User name.
     * \param[in] password User password.
     * \param[in] domain Domain name.
     * \param[in] hostname Host name.
     * \param[in] service_username Service user name.
     * \param[in] service_password Service user password.
     * \param[in] service_keytab_path Service keytab file path.
     */
    void set_identity(bytes_view username, char const * password,
        bytes_view domain, chars_view hostname,
        char const * service_username = nullptr,
        char const * service_password = nullptr,
        char const * service_keytab_path = nullptr);

    void send_negotiation_request(OutTransport trans);

    [[nodiscard]] const char * get_target_host() const {
        return this->target_host;
    }

    [[nodiscard]] const std::string & get_user_name() const {
        return this->username;
    }

    using CertificateChecker = Transport::CertificateChecker;

    /// \return false if terminal state
    [[nodiscard]]
    bool recv_next_data(TpduBuffer& buf, Transport& trans, CertificateChecker certificate_checker);

private:
    State fallback_to_tls(OutTransport trans);

    State recv_connection_confirm(OutTransport trans, InStream x224_stream, CertificateChecker certificate_checker);

    State activate_ssl_tls(OutTransport trans, CertificateChecker certificate_checker) const;

    State activate_ssl_hybrid(OutTransport trans, CertificateChecker certificate_checker);

    State recv_credssp(OutTransport trans, bytes_view in_data);
};
