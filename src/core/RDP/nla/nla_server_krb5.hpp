/*
  This program is free software; you can redistribute it and/or modify it
  under the terms of the GNU General Public License as published by the
  Free Software Foundation; either version 2 of the License, or (at your
  option) any later version.

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
  Public License for more details.

  You should have received a copy of the GNU General Public License along
  with this program; if not, write to the Free Software Foundation, Inc.,
  675 Mass Ave, Cambridge, MA 02139, USA.

  Product name: redemption, a FLOSS RDP proxy
  Copyright (C) Wallix 2013
  Author(s): Christophe Grosjean, Raphael Zhou, Meng Tan
*/

#include "core/RDP/nla/credssp.hpp"

#include <gssapi/gssapi.h>
#include <krb5.h>

#pragma once

class Krb5Server final
{
private:
    const std::vector<uint8_t> public_key;

    bool verbose = false;
    bool dump = false;

    uint64_t nla_recv_sequence_no = 0;
    uint64_t nla_send_sequence_no = 0;

    enum class NLAStep
    {
        negoToken,
        pubKeyAuth,
        authInfo,

        final
    } nla_step = NLAStep::negoToken;

    static const char *nlaStepToString(NLAStep nla_step)
    {
        switch (nla_step)
        {
        case NLAStep::negoToken:
            return "negoToken";
        case NLAStep::pubKeyAuth:
            return "pubKeyAuth";
        case NLAStep::authInfo:
            return "authInfo";

        case NLAStep::final:
            return "<final>";
        default:
            return "<unknown>";
        }
    }

    krb5_context context = nullptr;
	krb5_ccache ccache = nullptr;
    krb5_keytab keytab = nullptr;

    krb5_auth_context auth_context = nullptr;

    static constexpr uint32_t SSPI_GSS_C_DELEG_FLAG = 1;
    static constexpr uint32_t SSPI_GSS_C_MUTUAL_FLAG = 2;
    static constexpr uint32_t SSPI_GSS_C_REPLAY_FLAG = 4;
    static constexpr uint32_t SSPI_GSS_C_SEQUENCE_FLAG = 8;
    static constexpr uint32_t SSPI_GSS_C_CONF_FLAG = 16;
    static constexpr uint32_t SSPI_GSS_C_INTEG_FLAG = 32;

    static constexpr uint8_t FLAG_SENDER_IS_ACCEPTOR = 0x01;
    static constexpr uint8_t FLAG_WRAP_CONFIDENTIAL = 0x02;
    static constexpr uint8_t FLAG_ACCEPTOR_SUBKEY = 0x04;

    static constexpr int32_t KG_USAGE_ACCEPTOR_SEAL = 22;
    static constexpr int32_t KG_USAGE_ACCEPTOR_SIGN = 23;
    static constexpr int32_t KG_USAGE_INITIATOR_SEAL = 24;
    static constexpr int32_t KG_USAGE_INITIATOR_SIGN = 25;

    static constexpr uint16_t TOK_ID_AP_REQ = 0x0100;
    static constexpr uint16_t TOK_ID_AP_REP = 0x0200;
    static constexpr uint16_t TOK_ID_ERROR = 0x0300;
    static constexpr uint16_t TOK_ID_TGT_REQ = 0x0400;
    static constexpr uint16_t TOK_ID_TGT_REP = 0x0401;

    static constexpr uint16_t TOK_ID_MIC = 0x0404;
    static constexpr uint16_t TOK_ID_WRAP = 0x0504;
    static constexpr uint16_t TOK_ID_MIC_V1 = 0x0101;
    static constexpr uint16_t TOK_ID_WRAP_V1 = 0x0201;

    static const char* tokIDToString(uint16_t tok_id)
    {
        switch (tok_id)
        {
        case TOK_ID_AP_REQ:
            return "AP_REQ";
        case TOK_ID_AP_REP:
            return "AP_REP";
        case TOK_ID_ERROR:
            return "ERROR";
        case TOK_ID_TGT_REQ:
            return "TGT_REQ";
        case TOK_ID_TGT_REP:
            return "TGT_REP";
        case TOK_ID_MIC:
            return "MIC";
        case TOK_ID_WRAP:
            return "WRAP";
        case TOK_ID_MIC_V1:
            return "MIC_V1";
        case TOK_ID_WRAP_V1:
            return "WRAP_V1";

        default:
            return "<unknown>";
        }
    }

    // RFC4120 - 7.5.7. Kerberos Message Types
    enum class KerberosMessageType : int32_t
    {
        KRB_AP_REQ = 14,        // Application request to server
        KRB_AP_REP = 15,        // Response to KRB_AP_REQ_MUTUAL
        KRB_RESERVED16 = 16,    // Reserved for user-to-user krb_tgt_request
        KRB_RESERVED17 = 17,    // Reserved for user-to-user krb_tgt_reply
    };

    static const char* kerberosMessageTypeToString(
        KerberosMessageType kerberos_message_type)
    {
        switch (kerberos_message_type)
        {
        case KerberosMessageType::KRB_AP_REQ:
            return "KRB_AP_REQ";
        case KerberosMessageType::KRB_AP_REP:
            return "KRB_AP_REP";
        case KerberosMessageType::KRB_RESERVED16:
            return "KRB_TGT_REQ";
        case KerberosMessageType::KRB_RESERVED17:
            return "KRB_TGT_REP";

        default:
            return "<unknown>";
        }
    }

    enum class KerberosState
    {
        Initial,
        TgtReq,
        ApReq,
        Final
    } kerberos_state = KerberosState::Initial;

    static const char* kerberosStateToString(KerberosState kerberos_state)
    {
        switch (kerberos_state)
        {
        case KerberosState::Initial:
            return "Initial";
        case KerberosState::TgtReq:
            return "TgtReq";
        case KerberosState::ApReq:
            return "ApReq";
        case KerberosState::Final:
            return "Final";

        default:
            return "<unknown>";
        }
    }

    krb5_key kerberos_session_key = nullptr;
    krb5_key kerberos_initiator_key = nullptr;
    krb5_key kerberos_acceptor_key = nullptr;

    krb5_enctype kerberos_encryption_type = 0;

    uint32_t kerberos_flags = 0;

    int32_t kerberos_local_sequence_no = 0;
    int32_t kerberos_remote_sequence_no = 0;

    unsigned int kerberos_security_trailer_size = 0;

    static constexpr gss_OID_desc oid_spnego {
        .length = 6,
        .elements = const_cast<char*>("\x2b\x06\x01\x05\x05\x02")
    };
    static constexpr gss_OID_desc oid_ms_kerberos {
        .length = 9,
        .elements = const_cast<char*>("\x2a\x86\x48\x82\xf7\x12\x01\x02\x02")
    };
    static constexpr gss_OID_desc oid_kerberos {
        .length = 9,
        .elements = const_cast<char*>("\x2a\x86\x48\x86\xf7\x12\x01\x02\x02")
    };
    static constexpr gss_OID_desc oid_ms_negoex {
        .length = 10,
        .elements = const_cast<char*>("\x2b\x06\x01\x04\x01\x82\x37\x02\x02\x1e")
    };
    static constexpr gss_OID_desc oid_ms_nlmp {
        .length = 10,
        .elements = const_cast<char*>("\x2b\x06\x01\x04\x01\x82\x37\x02\x02\x0a")
    };
    static constexpr gss_OID_desc oid_ms_user_to_user {
        .length = 10,
        .elements = const_cast<char*>("\x2a\x86\x48\x86\xf7\x12\x01\x02\x02\x03")
    };

    const gss_OID_desc* accepted_mechanism = GSS_C_NO_OID;

    static bool oidIsEqual(gss_const_OID oid1, gss_const_OID oid2)
    {
        if (!oid1 || !oid2)
            return false;
        if (oid1->length != oid2->length)
            return false;
        return (::memcmp(oid1->elements, oid2->elements, oid1->length) == 0);
    }

    static const char* oidToLabel(gss_const_OID oid)
    {
        if (oidIsEqual(oid, &Krb5Server::oid_spnego))
        {
            return "SPNego";
        }
        else if (oidIsEqual(oid, &Krb5Server::oid_ms_kerberos))
        {
            return "MS-Kerberos";
        }
        else if (oidIsEqual(oid, &Krb5Server::oid_kerberos))
        {
            return "Kerberos";
        }
        else if (oidIsEqual(oid, &Krb5Server::oid_ms_negoex))
        {
            return "MS-NegoEx";
        }
        else if (oidIsEqual(oid, &Krb5Server::oid_ms_nlmp))
        {
            return "MS-NLMP";
        }
        else if (oidIsEqual(oid, &Krb5Server::oid_ms_user_to_user))
        {
            return "MS-UserToUser";
        }
        else
        {
            return "<unknown>";
        }
    }

    static const char* oidToString(gss_const_OID oid)
    {
        if (oidIsEqual(oid, &Krb5Server::oid_spnego))
        {
            return "1.3.6.1.5.5.2";
        }
        else if (oidIsEqual(oid, &Krb5Server::oid_ms_kerberos))
        {
            return "1.2.840.48018.1.2.2";
        }
        else if (oidIsEqual(oid, &Krb5Server::oid_kerberos))
        {
            return "1.2.840.113554.1.2.2";
        }
        else if (oidIsEqual(oid, &Krb5Server::oid_ms_negoex))
        {
            return "1.3.6.1.4.1.311.2.2.30";
        }
        else if (oidIsEqual(oid, &Krb5Server::oid_ms_nlmp))
        {
            return "1.3.6.1.4.1.311.2.2.10";
        }
        else if (oidIsEqual(oid, &Krb5Server::oid_ms_user_to_user))
        {
            return "1.2.840.113554.1.2.2.3";
        }
        else
        {
            return "<unknown>";
        }
    }

    bool use_spnego = false;

    enum NegState
    {
        Accept_Complete,
        Accept_Incomplete,
        Reject,
        Request_Mic
    };

    static const char *negStateToString(int negState)
    {
        switch (negState)
        {
            case Accept_Complete: return "accept-completed";
            case Accept_Incomplete: return "accept-incomplete";
            case Reject: return "reject";
            case Request_Mic: return "request-mic";

            default: return "<unknown>";
        }
    }

    std::string client_principal_name;
    TSCredentials ts_credentials;

    static void generatePublicKeyHashClientToServer(
        std::vector<uint8_t> const &clientNonce,
        std::vector<uint8_t> const &public_key,
        std::vector<uint8_t> &clientServerHash)
    {
        SslSha256 sha256;
        sha256.update("CredSSP Client-To-Server Binding Hash\0"_av);
        sha256.update(clientNonce);
        sha256.update(public_key);
        uint8_t hash[SslSha256::DIGEST_LENGTH]{};
        sha256.final(make_writable_sized_array_view(hash));

        clientServerHash.resize(sizeof(hash));
        memcpy(clientServerHash.data(), hash, sizeof(hash));
    }

    static void generatePublicKeyHashServerToClient(
        std::vector<uint8_t> const &clientNonce,
        std::vector<uint8_t> const &public_key,
        std::vector<uint8_t> &serverClientHash)
    {
        SslSha256 sha256;
        sha256.update("CredSSP Server-To-Client Binding Hash\0"_av);
        sha256.update(clientNonce);
        sha256.update(public_key);
        uint8_t hash[SslSha256::DIGEST_LENGTH]{};
        sha256.final(make_writable_sized_array_view(hash));

        serverClientHash.resize(sizeof(hash));
        memcpy(serverClientHash.data(), hash, sizeof(hash));
    }

    static void logKerbrosError(const char* fn, const char* msg, krb5_context ctx,
         krb5_error_code error_code)
    {
        const char* error_message = ::krb5_get_error_message(ctx, error_code);

        LOG(LOG_ERR, "%s - \"%s\" Error=\"%s\"(%d)",
            fn, msg, error_message, error_code);

        ::krb5_free_error_message(ctx, error_message);
    }

    bool kerberosDecryptToken(std::vector<uint8_t>& token_ref)
    {
        LOG_IF(this->verbose, LOG_INFO,
            "Krb5Server::kerberosDecryptToken(): ... TokenLength=%lu", token_ref.size());

        InStream in_s(token_ref);

        uint16_t const tok_id = in_s.in_uint16_be();
        LOG_IF(this->verbose, LOG_INFO,
            "Krb5Server::kerberosDecryptToken(): TokID=%s(0x%X)",
            tokIDToString(tok_id), unsigned(tok_id));

        uint8_t const decrypt_flags = in_s.in_uint8();
        LOG_IF(this->verbose, LOG_INFO,
            "Krb5Server::kerberosDecryptToken(): Flags=0x%02X", decrypt_flags);

        if (tok_id != TOK_ID_WRAP || in_s.in_uint8() != 0xFF) {
            LOG(LOG_ERR,
                "Krb5Server::kerberosDecryptToken(): Invalid token! Returns false.");
            return false;
        }

        if (FLAG_SENDER_IS_ACCEPTOR & decrypt_flags) {
            LOG(LOG_ERR,
                "Krb5Server::kerberosDecryptToken(): Invalid token! Sender cannot be an acceptor. Returns false.");
            return false;
        }

        if (!(FLAG_WRAP_CONFIDENTIAL & decrypt_flags)) {
            LOG(LOG_ERR,
                "Krb5Server::kerberosDecryptToken(): Invalid token! Confidentiality. Returns false.");
            return false;
        }

        uint16_t const ec = in_s.in_uint16_be();
        LOG_IF(this->verbose, LOG_INFO,"Krb5Server::kerberosDecryptToken(): ec=%u", ec);
        uint16_t const rrc = in_s.in_uint16_be();
        LOG_IF(this->verbose, LOG_INFO, "Krb5Server::kerberosDecryptToken(): rrc=%u", rrc);

        if ((rrc < 16) && rrc) {
            LOG(LOG_ERR, "Krb5Server::kerberosDecryptToken(): Invalud Right Rotation Count! Returns false.");
            return false;
        }

        uint64_t const seq_no = in_s.in_uint64_be();
        LOG_IF(this->verbose, LOG_INFO, "Krb5Server::kerberosDecryptToken(): seq_no=%lu", seq_no);

        if ((this->kerberos_flags & ISC_REQ_SEQUENCE_DETECT) &&
            (seq_no != this->kerberos_remote_sequence_no + this->nla_recv_sequence_no)) {
            LOG(LOG_ERR,
                "Krb5Server::kerberosDecryptToken(): Out of sequence! Returns false.");
            return false;
        }

        if (rrc)
        {
            krb5_crypto_iov decrypt_iov[] = {
                { KRB5_CRYPTO_TYPE_HEADER, { 0, 0, nullptr } },
                { KRB5_CRYPTO_TYPE_DATA, { 0, 0, nullptr } },
                { KRB5_CRYPTO_TYPE_DATA, { 0, 0, nullptr } },
                { KRB5_CRYPTO_TYPE_PADDING, { 0, 0, nullptr } },
                { KRB5_CRYPTO_TYPE_TRAILER, { 0, 0, nullptr } }
            };
            decrypt_iov[1].data.length = token_ref.size() - this->kerberos_security_trailer_size;
            decrypt_iov[2].data.length = 16;

            krb5_error_code error_code = ::krb5_c_crypto_length_iov(this->context,
                this->kerberos_encryption_type, decrypt_iov, sizeof(decrypt_iov) / sizeof(decrypt_iov[0]));
            if (error_code)
            {
                logKerbrosError(
                    "Krb5Server::kerberosDecryptToken(): krb5_c_crypto_length_iov",
                        "Failed to Fill in lengths for header, trailer and padding in IOV array! Returns false.",
                    this->context, error_code);

                return false;
            }

// for (int i = 0; i < sizeof(decrypt_iov) / sizeof(decrypt_iov[0]); ++i)
// {
//     LOG_IF(this->verbose, LOG_INFO,
//         "Krb5Server::kerberosDecryptToken(): decrypt_iov[%d].data.length=%u",
//         i, decrypt_iov[i].data.length);
// }

            if (rrc != 16 + decrypt_iov[3].data.length + decrypt_iov[4].data.length)
            {
                LOG(LOG_ERR,
                    "Krb5Server::kerberosDecryptToken(): Invalid token! (2) Returns false.");
                return false;
            }

            if (this->kerberos_security_trailer_size != 16 + rrc + decrypt_iov[0].data.length)
            {
                LOG(LOG_ERR,
                    "Krb5Server::kerberosDecryptToken(): Invalid token! (3) Returns false.");
                return false;
            }

            decrypt_iov[0].data.data = char_ptr_cast(&token_ref[16 + rrc + ec]);
            decrypt_iov[1].data.data = char_ptr_cast(&token_ref[this->kerberos_security_trailer_size]);
            decrypt_iov[2].data.data = char_ptr_cast(&token_ref[16 + ec]);
            char* data2 = decrypt_iov[2].data.data;
            decrypt_iov[3].data.data = &data2[decrypt_iov[2].data.length];

            char* data3 = decrypt_iov[3].data.data;
            decrypt_iov[4].data.data = &data3[decrypt_iov[3].data.length];

// for (int i = 0; i < sizeof(decrypt_iov) / sizeof(decrypt_iov[0]); ++i)
// {
//     LOG_IF(this->verbose, LOG_INFO,
//         "Krb5Server::kerberosDecryptToken(): decrypt_iov[%d].data.length=%u Offset=%ld",
//         i, decrypt_iov[i].data.length, byte_ptr_cast(decrypt_iov[i].data.data) - &token_ref[0]);
//     hexdump_d(decrypt_iov[i].data.data, decrypt_iov[i].data.length);
// }

            error_code = ::krb5_k_decrypt_iov(this->context, this->kerberos_acceptor_key,
                KG_USAGE_INITIATOR_SEAL, nullptr, decrypt_iov, sizeof(decrypt_iov) / sizeof(decrypt_iov[0]));
            if (error_code)
            {
                logKerbrosError(
                    "Krb5Server::kerberosDecryptToken(): krb5_k_decrypt_iov",
                        "Failed to decrypt data! Returns false.",
                    this->context, error_code);

                return false;
            }

            OutStream os_decrypt_iov({decrypt_iov[2].data.data, decrypt_iov[2].data.length});
            os_decrypt_iov.out_skip_bytes(4);
            os_decrypt_iov.out_uint16_be(ec);
            os_decrypt_iov.out_uint16_be(rrc);
            if (memcmp(decrypt_iov[2].data.data, token_ref.data(), 16) != 0)
            {
                LOG(LOG_ERR,
                    "Krb5Server::kerberosDecryptToken(): Message altered! Returns false.");
                return false;
            }

            LOG_IF(this->verbose, LOG_INFO,
                "Krb5Server::kerberosDecryptToken(): Done. Returns true.");

            return true;
        }

        krb5_data plain = { KV5M_DATA, 0, nullptr };
        krb5_enc_data cipher = { KV5M_ENC_DATA, 0, 0, { KV5M_DATA, 0, nullptr } };

        cipher.enctype = this->kerberos_encryption_type;
        cipher.ciphertext.length = token_ref.size() - 16;
        cipher.ciphertext.data = char_ptr_cast(&token_ref[16]);

        std::vector<uint8_t> plain_data(cipher.ciphertext.length);

        plain.length = cipher.ciphertext.length;
        plain.data = char_ptr_cast(plain_data.data());

        krb5_error_code error_code = ::krb5_k_decrypt(this->context,
            this->kerberos_acceptor_key, KG_USAGE_INITIATOR_SEAL, nullptr,
            &cipher, &plain);
        if (error_code)
        {
            logKerbrosError(
                "Krb5Server::kerberosDecryptToken(): krb5_k_decrypt",
                    "Failed to decrypt data! Returns false.",
                this->context, error_code);

            return false;
        }

        OutStream os_decrypt_iov({byte_ptr_cast(plain.data) + plain.length - 16, 16});
        os_decrypt_iov.out_skip_bytes(4);
        os_decrypt_iov.out_uint16_be(ec);
        os_decrypt_iov.out_uint16_be(rrc);
        if (memcmp(plain.data + plain.length - 16, token_ref.data(), 16) != 0)
        {
            LOG(LOG_ERR,
                "Krb5Server::kerberosDecryptToken(): Message altered! (2) Returns false.");
            return false;
        }

        memcpy(&token_ref[this->kerberos_security_trailer_size], plain.data, plain.length - 16);

        LOG_IF(this->verbose, LOG_INFO,
            "Krb5Server::kerberosDecryptToken(): Done. (2) Returns true.");

        return true;
    }

public:
    Krb5Server(bytes_view public_key, bool verbose, bool dump)
        : public_key(public_key.data(), public_key.data() + public_key.size())
        , verbose(verbose)
        , dump(dump)
    {
        krb5_error_code error_code = ::krb5_init_context(&this->context);
        if (error_code)
        {
            LOG(LOG_ERR,
                "Krb5Server::Krb5Server(): krb5_init_context - "
                    "\"Failed to create krb5 library context!\" ErrorCode=%d",
                error_code);

            throw Error(ERR_RDP_NEGOTIATION);
        }

        char keytab_file[PATH_MAX]{};
        error_code = ::krb5_kt_default_name(this->context, keytab_file, sizeof(keytab_file));
        if (error_code)
        {
            this->logKerbrosError(
                "Krb5Server::Krb5Server(): krb5_kt_default_name",
                "Failed to get default key table name!",
                this->context, error_code);

            ::krb5_free_context(this->context);

            throw Error(ERR_RDP_NEGOTIATION);
        }
        LOG_IF(this->verbose, LOG_INFO,
            "Krb5Server::Krb5Server(): DefaultKeyTableName=\"%s\"", keytab_file);

        error_code = ::krb5_cc_new_unique(this->context,
            "MEMORY", nullptr, &this->ccache);
        if (error_code)
        {
            this->logKerbrosError(
                "Krb5Server::Krb5Server(): krb5_cc_new_unique",
                "Failed to create krb5 (memory) credential cache!",
                this->context, error_code);

            ::krb5_free_context(this->context);

            throw Error(ERR_RDP_NEGOTIATION);
        }

        error_code = ::krb5_kt_resolve(this->context, keytab_file, &this->keytab);
        if (error_code)
        {
            this->logKerbrosError(
                "Krb5Server::Krb5Server(): krb5_kt_resolve",
                "Failed to get handle for key table!",
                this->context, error_code);

            ::krb5_cc_destroy(this->context, this->ccache);
            ::krb5_free_context(this->context);

            throw Error(ERR_RDP_NEGOTIATION);
        }
    }

    ~Krb5Server()
    {
        if (this->auth_context)
        {
            assert(this->context);

            krb5_error_code error_code = ::krb5_auth_con_free(
                this->context, this->auth_context);
            if (error_code)
            {
                this->logKerbrosError(
                    "Krb5Server::~Krb5Server(): krb5_cc_destroy",
                    "Failed to free authentication context!",
                    this->context, error_code);
            }
        }

        ::krb5_k_free_key(this->context, this->kerberos_session_key);
        ::krb5_k_free_key(this->context, this->kerberos_initiator_key);
        ::krb5_k_free_key(this->context, this->kerberos_acceptor_key);

        assert(this->ccache);
        krb5_error_code error_code = ::krb5_cc_destroy(this->context, this->ccache);
        if (error_code)
        {
            this->logKerbrosError(
                "Krb5Server::~Krb5Server(): krb5_cc_destroy",
                "Failed to destroy credential cache!",
                this->context, error_code);
        }

        assert(this->context);
        ::krb5_free_context(this->context);
    }

    credssp::State authenticate(bytes_view in_data,
                                std::vector<uint8_t> &out_data)
    {
        LOG_IF(this->verbose, LOG_INFO,
               "Krb5Server::authenticate(): ... in_data.size()=%lu",
               in_data.size());

        if (NLAStep::negoToken != this->nla_step &&
            NLAStep::pubKeyAuth != this->nla_step &&
            NLAStep::authInfo != this->nla_step)
        {
            LOG(LOG_ERR, "Krb5Server::authenticate(): "
                         "Wrong nla_step! CurrentStep=%s(%d) Err.",
                nlaStepToString(this->nla_step), this->nla_step);
            return credssp::State::Err;
        }

        TSRequest ts_request = recvTSRequest(in_data, this->verbose);

        if (not ts_request.negoTokens.empty())
        {
            if (NLAStep::negoToken == this->nla_step)
            {
                LOG_IF(this->verbose, LOG_INFO,
                    "Krb5Server::authenticate(): Process negoToken - Length=%lu",
                    ts_request.negoTokens.size());
                if (this->dump) {
                    hexdump_d(ts_request.negoTokens);
                }

                std::string server_name;
                bytes_view input_token;

                const gss_OID_desc* OID_token = GSS_C_NO_OID;

                // -- Start decoding the message --
                {
                    InStream in_s(ts_request.negoTokens);

                    const char ssp[] = "NTLMSSP";
                    if (in_s.in_remain() >= sizeof(ssp) &&
                        ::strncmp(char_ptr_cast(in_s.get_current()), ssp, sizeof(ssp)) == 0)
                    {
                        LOG(LOG_ERR, "Krb5Server::authenticate(): "
                                     "NTLM token is not supported! Err.");
                        return credssp::State::Err;
                    }

                    LOG_IF(this->verbose, LOG_INFO,
                        "Krb5Server::authenticate(): KerberosState=%s(%d)",
                        kerberosStateToString(this->kerberos_state), this->kerberos_state);

                    if (KerberosState::Initial == this->kerberos_state)
                    {
                        // Generic Security Service Application Program Interface
                        //     Version 2, Update 1
                        //
                        // https://datatracker.ietf.org/doc/html/rfc2743
                        //
                        // 3.1: Mechanism-Independent Token Format

                        // InitialContextToken ::=
                        // -- option indication (delegation, etc.) indicated within
                        // -- mechanism-specific token
                        // [APPLICATION 0] IMPLICIT SEQUENCE {
                        //         thisMech MechType,
                        //         innerContextToken ANY DEFINED BY thisMech
                        //             -- contents mechanism-specific
                        //             -- ASN.1 structure not required
                        //         }

                        // InitialContextToken
                        if (BER::check_ber_appl_tag(in_s.remaining_bytes(), 0))
                        {
                            in_s.in_skip_bytes(1);
                            auto [len0, queue0] = BER::pop_length(in_s.remaining_bytes(),
                                "GSS-API::InitialContextToken", ERR_RDP_NEGOTIATION);
                            in_s.in_skip_bytes(in_s.in_remain() - queue0.size());
                            //LOG(LOG_INFO, "Krb5Server::authenticate(): len0=%lu queue0.size()=%lu", len0, queue0.size());

                            // thisMech MechType
                            auto [len1, queue1] = BER::pop_tag_length(in_s.remaining_bytes(),
                                BER::CLASS_UNIV | BER::PC_PRIMITIVE | BER::TAG_OBJECT_IDENTIFIER,
                                "InitialContextToken::thisMech MechType", ERR_RDP_NEGOTIATION);
                            in_s.in_skip_bytes(in_s.in_remain() - queue1.size());
                            //LOG(LOG_INFO, "Krb5Server::authenticate(): len1=%lu queue1.size()=%lu", len1, queue1.size());
                            auto OID_bytes = in_s.in_skip_bytes(len1);
                            gss_OID_desc OID {
                                .length = static_cast<OM_uint32>(OID_bytes.size()),
                                .elements = const_cast<uint8_t*>(OID_bytes.data())
                            };
                            LOG_IF(this->verbose, LOG_INFO,
                                "Krb5Server::authenticate(): InitialContextToken::thisMech=%s(%s)",
                                oidToLabel(&OID), oidToString(&OID));
                            if (oidIsEqual(&Krb5Server::oid_spnego, &OID))
                            {
                                this->use_spnego = true;
                            }
                            else if (oidIsEqual(&Krb5Server::oid_kerberos, &OID))
                            {
                                this->accepted_mechanism = &Krb5Server::oid_kerberos;

                                OID_token = &Krb5Server::oid_kerberos;
                            }
                            else
                            {
                                LOG(LOG_ERR,
                                    "Krb5Server::authenticate(): Unexpected mechanism! Err.");
                                return credssp::State::Err;
                            }
                        }
                    }

                    if (this->use_spnego)
                    {
                        // NegotiationToken ::= CHOICE {
                        //     negTokenInit    [0] NegTokenInit,
                        //     negTokenResp    [1] NegTokenResp
                        // }

                        // negTokenInit [0] NegTokenInit
                        if (BER::check_ber_ctxt_tag(in_s.remaining_bytes(), 0))
                        {
                            in_s.in_skip_bytes(1);
                            auto [len0, queue0] = BER::pop_length(in_s.remaining_bytes(),
                                "NegotiationToken::negTokenInit [1] NegTokenInit (1)", ERR_RDP_NEGOTIATION);
                            in_s.in_skip_bytes(in_s.in_remain() - queue0.size());
                            //LOG(LOG_INFO, "Krb5Server::authenticate(): len0=%lu queue0.size()=%lu", len0, queue0.size());

                            // NegTokenInit ::= SEQUENCE {
                            //     mechTypes       [0] MechTypeList,
                            //     reqFlags        [1] ContextFlags  OPTIONAL,
                            //       -- inherited from RFC 2478 for backward compatibility,
                            //       -- RECOMMENDED to be left out
                            //     mechToken       [2] OCTET STRING  OPTIONAL,
                            //     mechListMIC     [3] OCTET STRING  OPTIONAL,
                            //     ...
                            // }

                            auto [len1, queue1] = BER::pop_tag_length(in_s.remaining_bytes(),
                                BER::CLASS_UNIV | BER::PC_CONSTRUCT | BER::TAG_SEQUENCE,
                                "NegotiationToken::negTokenInit [0] NegTokenInit (2)", ERR_RDP_NEGOTIATION);
                            in_s.in_skip_bytes(in_s.in_remain() - queue1.size());
                            //LOG(LOG_INFO, "Krb5Server::authenticate(): len1=%lu queue1.size()=%lu", len1, queue1.size());

                            // 0000 - 0x30, 0x81, 0x92, 0xa0, 0x03, 0x02, 0x01, 0x06, 0xa1, 0x81, 0x8a, 0x30, 0x81, 0x87, 0x30, 0x81,  // 0..........0..0.
                            // 0010 - 0x84, 0xa0, 0x81, 0x81, 0x04, 0x7f, 0x60, 0x7d, 0x06, 0x06, 0x2b, 0x06, 0x01, 0x05, 0x05, 0x02,  // ......`}..+.....
                            // 0020 - 0xa0, 0x73, 0x30, 0x71, 0xa0, 0x30, 0x30, 0x2e, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x82, 0xf7, 0x12,  // .s0q.00...*.H...
                            // 0030 - 0x01, 0x02, 0x02, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x12, 0x01, 0x02, 0x02, 0x06, 0x0a,  // .....*.H........
                            // 0040 - 0x2b, 0x06, 0x01, 0x04, 0x01, 0x82, 0x37, 0x02, 0x02, 0x1e, 0x06, 0x0a, 0x2b, 0x06, 0x01, 0x04,  // +.....7.....+...
                            // 0050 - 0x01, 0x82, 0x37, 0x02, 0x02, 0x0a, 0xa2, 0x3d, 0x04, 0x3b, 0x60, 0x39, 0x06, 0x0a, 0x2a, 0x86,  // ..7....=.;`9..*.
                            // 0060 - 0x48, 0x86, 0xf7, 0x12, 0x01, 0x02, 0x02, 0x03, 0x04, 0x00, 0x30, 0x29, 0xa0, 0x03, 0x02, 0x01,  // H.........0)....
                            // 0070 - 0x05, 0xa1, 0x03, 0x02, 0x01, 0x10, 0xa2, 0x1d, 0x30, 0x1b, 0xa0, 0x03, 0x02, 0x01, 0x02, 0xa1,  // ........0.......
                            // 0080 - 0x14, 0x30, 0x12, 0x1b, 0x07, 0x54, 0x45, 0x52, 0x4d, 0x53, 0x52, 0x56, 0x1b, 0x07, 0x52, 0x5a,  // .0...TERMSRV..RZ
                            // 0090 - 0x48, 0x2d, 0x56, 0x2d, 0x4d,                                                                    // H-V-M

                            // MechTypeList ::= SEQUENCE OF MechType

                            // MechType ::= OBJECT IDENTIFIER

                            // mechTypes [0] MechTypeList
                            auto [len2, queue2] = BER::pop_tag_length(in_s.remaining_bytes(),
                                BER::CLASS_CTXT | BER::PC_CONSTRUCT | 0,
                                "NegTokenInit::mechTypes [0] (1)", ERR_RDP_NEGOTIATION);
                            in_s.in_skip_bytes(in_s.in_remain() - queue2.size());
                            //LOG(LOG_INFO, "Krb5Server::authenticate(): len2=%lu queue2.size()=%lu", len2, queue2.size());

                            auto [len3, queue3] = BER::pop_tag_length(in_s.remaining_bytes(),
                                BER::CLASS_UNIV | BER::PC_CONSTRUCT | BER::TAG_SEQUENCE_OF,
                                "NegTokenInit::mechTypes [0] (2)", ERR_RDP_NEGOTIATION);
                            in_s.in_skip_bytes(in_s.in_remain() - queue3.size());
                            //LOG(LOG_INFO, "Krb5Server::authenticate(): len3=%lu queue3.size()=%lu", len3, queue3.size());

                            InStream in_s_mech_types({in_s.get_current(), len3});
                            do
                            {
                                auto [len4, queue4] = BER::pop_tag_length(in_s_mech_types.remaining_bytes(),
                                    BER::CLASS_UNIV | BER::PC_PRIMITIVE | BER::TAG_OBJECT_IDENTIFIER,
                                    "NegTokenInit::mechTypes [0] (3)", ERR_RDP_NEGOTIATION);
                                in_s_mech_types.in_skip_bytes(in_s_mech_types.in_remain() - queue4.size());
                                //LOG(LOG_INFO, "Krb5Server::authenticate(): len4=%lu queue4.size()=%lu", len4, queue4.size());
                                auto OID_mech_type_bytes = in_s_mech_types.in_skip_bytes(len4);
                                gss_OID_desc OID_mech_type {
                                    .length = static_cast<OM_uint32>(OID_mech_type_bytes.size()),
                                    .elements = const_cast<uint8_t*>(OID_mech_type_bytes.data())
                                };

                                LOG_IF(this->verbose, LOG_INFO,
                                    "Krb5Server::authenticate(): NegTokenInit::mechTypes=%s(%s)",
                                    oidToLabel(&OID_mech_type), oidToString(&OID_mech_type));

                                if (GSS_C_NO_OID == this->accepted_mechanism)
                                {
                                    if (oidIsEqual(&OID_mech_type, &Krb5Server::oid_ms_kerberos))
                                    {
                                        this->accepted_mechanism = &Krb5Server::oid_ms_kerberos;
                                    }
                                    else if (oidIsEqual(&OID_mech_type, &Krb5Server::oid_ms_user_to_user))
                                    {
                                        this->accepted_mechanism = &Krb5Server::oid_ms_user_to_user;
                                    }
                                }
                            }
                            while (in_s_mech_types.in_remain());
                            in_s.in_skip_bytes(len3);

                            if (GSS_C_NO_OID == this->accepted_mechanism)
                            {
                                LOG(LOG_ERR,
                                    "Krb5Server::authenticate(): No mechanism is accepted! Err.");
                                return credssp::State::Err;
                            }

                            // NegTokenInit::reqFlags [1] ContextFlags OPTIONAL
                            if (BER::check_ber_ctxt_tag(in_s.remaining_bytes(), 1))
                            {
                                LOG(LOG_ERR,
                                    "Krb5Server::authenticate(): NegTokenInit::reqFlags is not yet supported! Err.");
                                return credssp::State::Err;
                            }

                            // NegTokenInit::mechToken [2] OCTET STRING OPTIONAL
                            if (BER::check_ber_ctxt_tag(in_s.remaining_bytes(), 2))
                            {
                                auto [len4, queue4] = BER::pop_tag_length(in_s.remaining_bytes(),
                                    BER::CLASS_CTXT | BER::PC_CONSTRUCT | 2,
                                    "NegTokenInit::mechToken [2] OCTET STRING OPTIONAL (1)",
                                    ERR_RDP_NEGOTIATION);
                                //LOG(LOG_INFO, "Krb5Server::authenticate(): len4=%lu queue4.size()=%lu", len4, queue4.size());
                                in_s.in_skip_bytes(in_s.in_remain() - queue4.size());

                                auto [len5, queue5] = BER::pop_tag_length(in_s.remaining_bytes(),
                                    BER::CLASS_UNIV | BER::PC_PRIMITIVE | BER::TAG_OCTET_STRING,
                                    "NegTokenInit::mechToken [2] OCTET STRING OPTIONAL (2)",
                                    ERR_RDP_NEGOTIATION);
                                //LOG(LOG_INFO, "Krb5Server::authenticate(): len5=%lu queue5.size()=%lu", len5, queue5.size());
                                in_s.in_skip_bytes(in_s.in_remain() - queue5.size());

                                auto [len6, queue6] = BER::pop_tag_length(in_s.remaining_bytes(),
                                    BER::CLASS_APPL | BER::PC_CONSTRUCT | 0,
                                    "NegTokenInit::mechToken [2] OCTET STRING OPTIONAL (3)",
                                    ERR_RDP_NEGOTIATION);
                                //LOG(LOG_INFO, "Krb5Server::authenticate(): len6=%lu queue6.size()=%lu", len6, queue6.size());
                                in_s.in_skip_bytes(in_s.in_remain() - queue6.size());

                                auto [len7, queue7] = BER::pop_tag_length(in_s.remaining_bytes(),
                                    BER::CLASS_UNIV | BER::PC_PRIMITIVE | BER::TAG_OBJECT_IDENTIFIER,
                                    "NegTokenInit::mechToken [2] OCTET STRING OPTIONAL (4)",
                                    ERR_RDP_NEGOTIATION);
                                //LOG(LOG_INFO, "Krb5Server::authenticate(): len7=%lu queue7.size()=%lu", len7, queue7.size());
                                in_s.in_skip_bytes(in_s.in_remain() - queue7.size());
                                auto OID_mechToken_bytes = in_s.in_skip_bytes(len7);
                                gss_OID_desc OID_mechToken {
                                    .length = static_cast<OM_uint32>(OID_mechToken_bytes.size()),
                                    .elements = const_cast<uint8_t*>(OID_mechToken_bytes.data())
                                };
                                LOG_IF(this->verbose, LOG_INFO,
                                    "Krb5Server::authenticate(): NegTokenInit::mechToken=%s(%s)",
                                    oidToLabel(&OID_mechToken), oidToString(&OID_mechToken));

                                if (oidIsEqual(&OID_mechToken, &Krb5Server::oid_ms_user_to_user))
                                {
                                    OID_token = &Krb5Server::oid_ms_user_to_user;
                                }
                                else
                                {
                                    LOG(LOG_ERR,
                                        "Krb5Server::authenticate(): "
                                            "Unexpected optimistic mechanism was found in NegTokenInit! Err.");
                                    return credssp::State::Err;
                                }
                            }
                        }
                        // negTokenResp [1] NegTokenResp
                        else if (BER::check_ber_ctxt_tag(in_s.remaining_bytes(), 1))
                        {
                            in_s.in_skip_bytes(1);
                            auto [len0, queue0] = BER::pop_length(in_s.remaining_bytes(),
                                "NegotiationToken::negTokenResp [1] NegTokenResp (1)", ERR_RDP_NEGOTIATION);
                            in_s.in_skip_bytes(in_s.in_remain() - queue0.size());
                            //LOG(LOG_INFO, "Krb5Server::authenticate(): len0=%lu queue0.size()=%lu", len0, queue0.size());

                            // NegTokenResp ::= SEQUENCE {
                            //     negState       [0] ENUMERATED {
                            //         accept-completed    (0),
                            //         accept-incomplete   (1),
                            //         reject              (2),
                            //         request-mic         (3)
                            //     }                                 OPTIONAL,
                            //       -- REQUIRED in the first reply from the target
                            //     supportedMech   [1] MechType      OPTIONAL,
                            //       -- present only in the first reply from the target
                            //     responseToken   [2] OCTET STRING  OPTIONAL,
                            //     mechListMIC     [3] OCTET STRING  OPTIONAL,
                            //     ...
                            // }

                            auto [len1, queue1] = BER::pop_tag_length(in_s.remaining_bytes(),
                            BER::CLASS_UNIV | BER::PC_CONSTRUCT | BER::TAG_SEQUENCE,
                            "NegotiationToken::negTokenResp [1] NegTokenResp (2)", ERR_RDP_NEGOTIATION);
                            in_s.in_skip_bytes(in_s.in_remain() - queue1.size());
                            //LOG(LOG_INFO, "Krb5Server::authenticate(): len1=%lu queue1.size()=%lu", len1, queue1.size());

                            if (BER::check_ber_ctxt_tag(in_s.remaining_bytes(), 0))
                            {
                                // negState [0] ENUMERATED
                                auto [value2, queue2] = BER::pop_enumerated_field(in_s.remaining_bytes(), 0,
                                    "NegTokenResp::negState [0] ENUMERATED", ERR_RDP_NEGOTIATION);
                                in_s.in_skip_bytes(in_s.in_remain() - queue2.size());
                                //LOG(LOG_INFO, "Krb5Server::authenticate(): value2=%d queue2.size()=%lu", value2, queue2.size());
                                LOG_IF(this->verbose, LOG_INFO,
                                    "Krb5Server::authenticate(): negState=%d", value2);
                                if (Accept_Incomplete != value2)
                                {
                                    LOG(LOG_ERR,
                                        "Krb5Server::authenticate(): Unexpected state of negotiation! Expected=%s(%d) Got=%s(%d)",
                                        negStateToString(Accept_Incomplete), static_cast<int>(Accept_Incomplete),
                                        negStateToString(value2), value2);
                                    return credssp::State::Err;
                                }
                            }

                            // NegTokenResp::responseToken [2] OCTET STRING OPTIONAL
                            if (BER::check_ber_ctxt_tag(in_s.remaining_bytes(), 2))
                            {
                                auto [len3, queue3] = BER::pop_tag_length(in_s.remaining_bytes(),
                                    BER::CLASS_CTXT | BER::PC_CONSTRUCT | 2,
                                    "NegTokenResp::responseToken [2] OCTET STRING OPTIONAL (1)",
                                    ERR_RDP_NEGOTIATION);
                                //LOG(LOG_INFO, "Krb5Server::authenticate(): len3=%lu queue3.size()=%lu", len3, queue3.size());
                                in_s.in_skip_bytes(in_s.in_remain() - queue3.size());

                                auto [len4, queue4] = BER::pop_tag_length(in_s.remaining_bytes(),
                                    BER::CLASS_UNIV | BER::PC_PRIMITIVE | BER::TAG_OCTET_STRING,
                                    "NegTokenResp::responseToken [2] OCTET STRING OPTIONAL (2)",
                                    ERR_RDP_NEGOTIATION);
                                //LOG(LOG_INFO, "Krb5Server::authenticate(): len4=%lu queue4.size()=%lu", len4, queue4.size());
                                in_s.in_skip_bytes(in_s.in_remain() - queue4.size());

                                auto [len5, queue5] = BER::pop_tag_length(in_s.remaining_bytes(),
                                    BER::CLASS_APPL | BER::PC_CONSTRUCT | 0,
                                    "NegTokenResp::responseToken [2] OCTET STRING OPTIONAL (3)",
                                    ERR_RDP_NEGOTIATION);
                                //LOG(LOG_INFO, "Krb5Server::authenticate(): len5=%lu queue5.size()=%lu", len5, queue5.size());
                                in_s.in_skip_bytes(in_s.in_remain() - queue5.size());

                                auto [len6, queue6] = BER::pop_tag_length(in_s.remaining_bytes(),
                                    BER::CLASS_UNIV | BER::PC_PRIMITIVE | BER::TAG_OBJECT_IDENTIFIER,
                                    "NegTokenResp::responseToken [2] OCTET STRING OPTIONAL (4)",
                                    ERR_RDP_NEGOTIATION);
                                in_s.in_skip_bytes(in_s.in_remain() - queue6.size());
                                //LOG(LOG_INFO, "Krb5Server::authenticate(): len6=%lu queue6.size()=%lu", len6, queue6.size());
                                auto OID_responseToken_bytes = in_s.in_skip_bytes(len6);
                                gss_OID_desc OID_responseToken {
                                    .length = static_cast<OM_uint32>(OID_responseToken_bytes.size()),
                                    .elements = const_cast<uint8_t*>(OID_responseToken_bytes.data())
                                };
                                LOG_IF(this->verbose, LOG_INFO,
                                    "Krb5Server::authenticate(): NegTokenResp::responseToken=%s(%s)",
                                    oidToLabel(&OID_responseToken), oidToString(&OID_responseToken));

                                if (oidIsEqual(&OID_responseToken, &Krb5Server::oid_ms_user_to_user))
                                {
                                    OID_token = &Krb5Server::oid_ms_user_to_user;
                                }
                                else if (oidIsEqual(&OID_responseToken, &Krb5Server::oid_ms_kerberos))
                                {
                                    OID_token = &Krb5Server::oid_ms_kerberos;
                                }
                                else
                                {
                                    LOG(LOG_ERR,
                                        "Krb5Server::authenticate(): "
                                            "Unexpected mechanism was found in NegTokenResp! Err.");
                                    return credssp::State::Err;
                                }
                            }
                        }
                    }

                    if (GSS_C_NO_OID != OID_token)
                    {
                        auto const u16TokID = in_s.in_uint16_be();
                        LOG_IF(this->verbose, LOG_INFO,
                            "Krb5Server::authenticate(): TokID=%s(0x%X)",
                            tokIDToString(u16TokID), unsigned(u16TokID));

                        if (TOK_ID_TGT_REQ == u16TokID)
                        {
                            // KERB-TGT-REQUEST ::= SEQUENCE {
                            //     pvno[0]                         INTEGER,
                            //     msg-type[1]                     INTEGER,
                            //     server-name[2]                  PrincipalName OPTIONAL,
                            //     realm[3]                        Realm OPTIONAL
                            // }

                            auto [len0, queue0] = BER::pop_tag_length(in_s.remaining_bytes(),
                                BER::CLASS_UNIV | BER::PC_CONSTRUCT | BER::TAG_SEQUENCE,
                                "KERB-TGT-REQUEST", ERR_RDP_NEGOTIATION);
                            in_s.in_skip_bytes(in_s.in_remain() - queue0.size());
                            //LOG(LOG_INFO, "Krb5Server::authenticate(): len0=%lu queue0.size()=%lu", len0, queue0.size());

                            // KERB-TGT-REQUEST::pvno [0] INTEGER
                            auto [len1, queue1] = BER::pop_tag_length(in_s.remaining_bytes(),
                                BER::CLASS_CTXT | BER::PC_CONSTRUCT | 0,
                                "KERB-TGT-REQUEST::Pvno [0] INTEGER (1)", ERR_RDP_NEGOTIATION);
                            in_s.in_skip_bytes(in_s.in_remain() - queue1.size());
                            //LOG(LOG_INFO, "Krb5Server::authenticate(): len1=%lu queue1.size()=%lu", len1, queue1.size());

                            auto [value2, queue2] = BER::pop_integer(in_s.remaining_bytes(),
                                "KERB-TGT-REQUEST::Pvno [0] INTEGER (2)", ERR_RDP_NEGOTIATION);
                                in_s.in_skip_bytes(in_s.in_remain() - queue2.size());
                            //LOG(LOG_INFO, "Krb5Server::authenticate(): value2=%d queue2.size()=%lu", value2, queue2.size());
                            LOG_IF(this->verbose, LOG_INFO,
                                "Krb5Server::authenticate(): KERB-TGT-REQUEST::Pvno=%d", value2);
                            if (5 != value2)
                            {
                                LOG(LOG_ERR,
                                    "Krb5Server::authenticate(): Unexpected Kerberos protocol version! Err.");
                                return credssp::State::Err;
                            }

                            // KERB-TGT-REQUEST::msg-type [1] INTEGER
                            auto [len3, queue3] = BER::pop_tag_length(in_s.remaining_bytes(),
                                BER::CLASS_CTXT | BER::PC_CONSTRUCT | 1,
                                "KERB-TGT-REQUEST::msg-type [1] INTEGER (1)", ERR_RDP_NEGOTIATION);
                            in_s.in_skip_bytes(in_s.in_remain() - queue3.size());
                            //LOG(LOG_INFO, "Krb5Server::authenticate(): len3=%lu queue3.size()=%lu", len3, queue3.size());

                            auto [value4, queue4] = BER::pop_integer(in_s.remaining_bytes(),
                                "KERB-TGT-REQUEST::msg-type [1] INTEGER (2)", ERR_RDP_NEGOTIATION);
                                in_s.in_skip_bytes(in_s.in_remain() - queue4.size());
                            //LOG(LOG_INFO, "Krb5Server::authenticate(): value4=%d queue4.size()=%lu", value4, queue4.size());
                            LOG_IF(this->verbose, LOG_INFO,
                                "Krb5Server::authenticate(): KERB-TGT-REQUEST::msg-type=%s(%d)",
                                kerberosMessageTypeToString(static_cast<KerberosMessageType>(value4)), value4);
                            if (static_cast<int>(KerberosMessageType::KRB_RESERVED16) != value4)
                            {
                                LOG(LOG_ERR,
                                    "Krb5Server::authenticate(): Unexpected Kerberos message type! Err.");
                                return credssp::State::Err;
                            }

                            // KERB-TGT-REQUEST::server-name [2] PrincipalName OPTIONAL
                            if (BER::check_ber_ctxt_tag(in_s.remaining_bytes(), 2))
                            {
                                auto [len5, queue5] = BER::pop_tag_length(in_s.remaining_bytes(),
                                    BER::CLASS_CTXT | BER::PC_CONSTRUCT | 2,
                                    "KERB-TGT-REQUEST::server-name [2] PrincipalName OPTIONAL (1)",
                                    ERR_RDP_NEGOTIATION);
                                in_s.in_skip_bytes(in_s.in_remain() - queue5.size());
                                //LOG(LOG_INFO, "Krb5Server::authenticate(): len5=%lu queue5.size()=%lu", len5, queue5.size());

                                auto [len6, queue6] = BER::pop_tag_length(in_s.remaining_bytes(),
                                    BER::CLASS_UNIV | BER::PC_CONSTRUCT | BER::TAG_SEQUENCE,
                                    "KERB-TGT-REQUEST::server-name [2] PrincipalName OPTIONAL (2)",
                                    ERR_RDP_NEGOTIATION);
                                in_s.in_skip_bytes(in_s.in_remain() - queue6.size());
                                //LOG(LOG_INFO, "Krb5Server::authenticate(): len6=%lu queue6.size()=%lu", len6, queue6.size());

                                // PrincipalName   ::= SEQUENCE {
                                //     name-type       [0] Int32,
                                //     name-string     [1] SEQUENCE OF KerberosString
                                // }

                                // PrincipalName::name-type [0] Int32
                                auto [len7, queue7] = BER::pop_tag_length(in_s.remaining_bytes(),
                                    BER::CLASS_CTXT | BER::PC_CONSTRUCT | 0,
                                    "PrincipalName::name-type [0] Int32 (1)", ERR_RDP_NEGOTIATION);
                                in_s.in_skip_bytes(in_s.in_remain() - queue7.size());
                                //LOG(LOG_INFO, "Krb5Server::authenticate(): len7=%lu queue7.size()=%lu", len7, queue7.size());

                                auto [value8, queue8] = BER::pop_integer(in_s.remaining_bytes(),
                                    "PrincipalName::name-type [0] Int32 (2)", ERR_RDP_NEGOTIATION);
                                    in_s.in_skip_bytes(in_s.in_remain() - queue8.size());
                                //LOG(LOG_INFO, "Krb5Server::authenticate(): value8=%d queue8.size()=%lu", value8, queue8.size());
                                LOG_IF(this->verbose, LOG_INFO,
                                    "Krb5Server::authenticate(): PrincipalName::name-type=%d", value8);

                                // NameString[1] SEQUENCE

                                auto [len9, queue9] = BER::pop_tag_length(in_s.remaining_bytes(),
                                    BER::CLASS_CTXT | BER::PC_CONSTRUCT | 1,
                                    "PrincipalName::name-string [1] SEQUENCE OF KerberosString (1)",
                                    ERR_RDP_NEGOTIATION);
                                in_s.in_skip_bytes(in_s.in_remain() - queue9.size());
                                //LOG(LOG_INFO, "Krb5Server::authenticate(): len9=%lu queue9.size()=%lu", len9, queue9.size());

                                auto [len10, queue10] = BER::pop_tag_length(in_s.remaining_bytes(),
                                    BER::CLASS_UNIV | BER::PC_CONSTRUCT | BER::TAG_SEQUENCE_OF,
                                    "PrincipalName::name-string [1] SEQUENCE OF KerberosString (2)",
                                    ERR_RDP_NEGOTIATION);
                                in_s.in_skip_bytes(in_s.in_remain() - queue10.size());
                                //LOG(LOG_INFO, "Krb5Server::authenticate(): len10=%lu queue10.size()=%lu", len10, queue10.size());

                                InStream in_s_name_strings({in_s.get_current(), len10});
                                do
                                {
                                    auto [len11, queue11] = BER::pop_tag_length(in_s_name_strings.remaining_bytes(),
                                        BER::CLASS_UNIV | BER::TAG_GENERAL_STRING,
                                        "PrincipalName::name-string [1] SEQUENCE OF KerberosString (3)",
                                        ERR_RDP_NEGOTIATION);
                                    in_s_name_strings.in_skip_bytes(in_s_name_strings.in_remain() - queue11.size());
                                    //LOG(LOG_INFO, "Krb5Server::authenticate(): len11=%lu queue11.size()=%lu", len11, queue11.size());
                                    auto NameString = in_s_name_strings.in_skip_bytes(len11);

                                    if (!server_name.empty())
                                    {
                                        server_name += "/";
                                    }
                                    server_name.append(char_ptr_cast(NameString.data()), NameString.size());

                                    LOG_IF(this->verbose, LOG_INFO,
                                        "Krb5Server::authenticate(): PrincipalName::name-string=%.*s",
                                        static_cast<int>(NameString.size()), NameString.data());
                                }
                                while (in_s_name_strings.in_remain());
                                in_s.in_skip_bytes(len10);
                            }

                            // KERB-TGT-REQUEST::realm [3] Realm OPTIONAL
                            if (BER::check_ber_ctxt_tag(in_s.remaining_bytes(), 3))
                            {
                                auto [len15, queue15] = BER::pop_tag_length(in_s.remaining_bytes(),
                                    BER::CLASS_CTXT | BER::PC_CONSTRUCT | 3,
                                    "KERB-TGT-REQUEST::realm [3] Realm OPTIONAL (1)", ERR_RDP_NEGOTIATION);
                                in_s.in_skip_bytes(in_s.in_remain() - queue15.size());
                                //LOG(LOG_INFO, "Krb5Server::authenticate(): len15=%lu queue15.size()=%lu", len15, queue15.size());

                                // Realm ::= KerberosString

                                auto [len16, queue16] = BER::pop_tag_length(in_s.remaining_bytes(),
                                    BER::CLASS_UNIV | BER::TAG_GENERAL_STRING,
                                    "KERB-TGT-REQUEST::realm [3] Realm OPTIONAL (2)", ERR_RDP_NEGOTIATION);
                                in_s.in_skip_bytes(in_s.in_remain() - queue16.size());
                                //LOG(LOG_INFO, "Krb5Server::authenticate(): len16=%lu queue16.size()=%lu", len16, queue16.size());
                                auto Realm = in_s.in_skip_bytes(len16);
                                LOG_IF(this->verbose, LOG_INFO,
                                    "Krb5Server::authenticate(): KERB-TGT-REQUEST::realm=%.*s", static_cast<int>(Realm.size()), Realm.data());
                            }

                            this->kerberos_state = KerberosState::TgtReq;

                            LOG_IF(this->verbose, LOG_INFO,
                                "Krb5Server::authenticate(): KerberosState=%s(%d) (2)",
                                kerberosStateToString(this->kerberos_state), this->kerberos_state);
                        }
                        else if (TOK_ID_AP_REQ == u16TokID)
                        {
                            input_token = in_s.remaining_bytes();
                            LOG_IF(this->verbose, LOG_INFO,
                                "Krb5Server::authenticate(): InputTokenLength=%lu", input_token.size());
                            if (this->dump) {
                                hexdump_d(input_token);
                            }

                            this->kerberos_state = KerberosState::ApReq;

                            LOG_IF(this->verbose, LOG_INFO,
                                "Krb5Server::authenticate(): KerberosState=%s(%d) (3)",
                                kerberosStateToString(this->kerberos_state), this->kerberos_state);
                        }
                        else
                        {
                            LOG(LOG_ERR,
                                "Krb5Server::authenticate(): Unexpected token! Err.");
                            return credssp::State::Err;
                        }
                    }
                }
                // -- End of negoToken decoding --

                if (KerberosState::TgtReq == this->kerberos_state)
                {
                    LOG_IF(this->verbose, LOG_INFO,
                        "Krb5Server::authenticate(): ServerName=%s", server_name.c_str());

                    const char* realm = strchr(server_name.c_str(), '@');
                    if (realm)
                    {
                        realm++;

                        LOG_IF(this->verbose, LOG_INFO,
                            "Krb5Server::authenticate(): Realm=%s", realm);
                    }

                    krb5_principal principal = nullptr;

                    krb5_error_code error_code = ::krb5_parse_name_flags(
                        this->context, server_name.c_str(),
                        KRB5_PRINCIPAL_PARSE_NO_REALM, &principal);
                    if (error_code)
                    {
                        logKerbrosError(
                            "Krb5Server::authenticate(): krb5_parse_name_flags",
                            "Failed to convert string principal name (no realm must be present)!",
                            this->context, error_code);

                        return credssp::State::Err;
                    }

                    if (realm)
                    {
                        error_code = ::krb5_set_principal_realm(
                            this->context, principal, realm);

                        if (error_code)
                        {
                            logKerbrosError(
                                "Krb5Server::authenticate(): krb5_set_principal_realm",
                                "Failed to set realm field of principal!",
                                this->context, error_code);

                            return credssp::State::Err;
                        }
                    }

                    krb5_keytab_entry entry{};
                    krb5_kt_cursor cursor = nullptr;
                    error_code = ::krb5_kt_start_seq_get(this->context, this->keytab, &cursor);
                    if (error_code)
                    {
                        logKerbrosError(
                            "Krb5Server::authenticate(): krb5_kt_start_seq_get",
                            "Failed to start sequential reading of each key in the key table!",
                            this->context, error_code);

                        return credssp::State::Err;
                    }
                    else
                    {
                        do
                        {
                            error_code = ::krb5_kt_next_entry(
                                this->context, this->keytab, &entry, &cursor);
                            if (KRB5_KT_END == error_code)
                            {
                                LOG(LOG_ERR,
                                    "Krb5Server::authenticate(): "
                                        "No suitable entry found in the key table!");
                                break;
                            }
                            else if (error_code)
                            {
                                logKerbrosError(
                                    "Krb5Server::authenticate(): krb5_kt_next_entry",
                                    "Failed to etrieve the next entry from the key table!",
                                    this->context, error_code);

                                return credssp::State::Err;
                            }

                            if (::krb5_principal_compare_any_realm(this->context, principal, entry.principal) &&
                                (!realm || krb5_realm_compare(this->context, principal, entry.principal)))
                            {
                                LOG_IF(this->verbose, LOG_INFO,
                                    "Krb5Server::authenticate(): "
                                        "Suitable entry found in the key talbe.");
                                break;
                            }

                            error_code = ::krb5_free_keytab_entry_contents(this->context, &entry);
                            if (error_code)
                            {
                                logKerbrosError(
                                    "Krb5Server::authenticate(): krb5_free_keytab_entry_contents",
                                    "Failed to free the contents of key table entry! (1)",
                                    this->context, error_code);

                                return credssp::State::Err;
                            }

                            ::memset(&entry, 0, sizeof(entry));
                        }
                        while (true);

                        error_code =
                            ::krb5_kt_end_seq_get(this->context, this->keytab, &cursor);
                        if (error_code)
                        {
                            logKerbrosError(
                                "Krb5Server::authenticate(): krb5_kt_end_seq_get",
                                "Failed to release key table reading cursor!",
                                this->context, error_code);

                            return credssp::State::Err;
                        }
                    }

                    if (!entry.principal)
                    {
                        return credssp::State::Err;
                    }

                    // Get the TGT.
                    krb5_creds creds{};
                    error_code = ::krb5_get_init_creds_keytab(
                        this->context, &creds, entry.principal, this->keytab, 0,
                        nullptr, nullptr);
                    if (error_code)
                    {
                        logKerbrosError(
                            "Krb5Server::authenticate(): krb5_get_init_creds_keytab",
                            "Failed to get initial credentials using key table!",
                            this->context, error_code);

                            ::krb5_free_keytab_entry_contents(this->context, &entry);

                        return credssp::State::Err;
                    }

                    error_code = ::krb5_free_keytab_entry_contents(this->context, &entry);
                    if (error_code)
                    {
                        logKerbrosError(
                            "Krb5Server::authenticate(): krb5_free_keytab_entry_contents",
                            "Failed to free the contents of key table entry! (2)",
                            this->context, error_code);

                        return credssp::State::Err;
                    }

                    LOG_IF(this->verbose, LOG_INFO,
                        "Krb5Server::authenticate(): InitCredsLength=%u", creds.ticket.length);


                    // KERB-TGT-REPLY ::= SEQUENCE {

                    //     pvno[0]                         INTEGER,
                    //     msg-type[1]                     INTEGER,
                    //     ticket[2]                       Ticket,
                    //     server-name[4]                  PrincipalName OPTIONAL,
                    // }

                    // pvno[0] INTEGER,
                    auto pvno_field = BER::mkSmallIntegerField(5, 0);
                    // msg-type[1] INTEGER,
                    auto msg_type_field = BER::mkSmallIntegerField(
                        static_cast<uint8_t>(KerberosMessageType::KRB_RESERVED17), 1);
                    // ticket[2] Ticket
                    auto ticket_header = BER::mkContextualFieldHeader(creds.ticket.length, 2);

                    // KERB-TGT-REPLY
                    size_t krb_tgt_replay_length = pvno_field.size()
                        + msg_type_field.size()
                        + ticket_header.size() + creds.ticket.length;

                    auto krb_tgt_replay_sequence_header = BER::mkSequenceHeader(krb_tgt_replay_length);

                    StaticOutStream<8> token_id;
                    token_id.out_uint16_be(TOK_ID_TGT_REP);

                    auto this_mech_header = BER::mkObjectIdentifierHeader(Krb5Server::oid_ms_user_to_user.length);

                    auto application_header = BER::mkApplicationFieldHeader(
                          this_mech_header.size()
                        + Krb5Server::oid_ms_user_to_user.length
                        + token_id.get_offset()
                        + krb_tgt_replay_sequence_header.size()
                        + krb_tgt_replay_length,
                        0
                    );

                    std::vector<uint8_t> application = std::vector<uint8_t>{}
                        << application_header
                        << this_mech_header;
                    application.insert(application.end(),
                        static_cast<uint8_t*>(Krb5Server::oid_ms_user_to_user.elements),
                        static_cast<uint8_t*>(Krb5Server::oid_ms_user_to_user.elements) + Krb5Server::oid_ms_user_to_user.length);
                    application << token_id.get_produced_bytes()
                        << krb_tgt_replay_sequence_header
                        << pvno_field
                        << msg_type_field
                        << ticket_header;
                    application.insert(application.end(), creds.ticket.data, creds.ticket.data + creds.ticket.length);

                    LOG_IF(this->verbose, LOG_INFO,
                        "Krb5Server::authenticate(): TgtReplyLength=%lu", application.size());
                    if (this->dump) {
                        hexdump_d(application);
                    }

                    error_code = ::krb5_auth_con_init(this->context, &this->auth_context);
                    if (error_code)
                    {
                        logKerbrosError(
                            "Krb5Server::authenticate(): krb5_get_init_creds_keytab",
                            "Failed to create and initialize authentication context!",
                            this->context, error_code);

                        return credssp::State::Err;
                    }

                    error_code = ::krb5_auth_con_setuseruserkey(
                        this->context, this->auth_context, &creds.keyblock);
                    if (error_code)
                    {
                        logKerbrosError(
                            "Krb5Server::authenticate(): krb5_get_init_creds_keytab",
                            "Failed to set the session key in auth context!",
                            this->context, error_code);

                        return credssp::State::Err;
                    }

                    // NegTokenResp ::= SEQUENCE {
                    //     negState       [0] ENUMERATED {
                    //         accept-completed    (0),
                    //         accept-incomplete   (1),
                    //         reject              (2),
                    //         request-mic         (3)
                    //     }                                 OPTIONAL,
                    //       -- REQUIRED in the first reply from the target
                    //     supportedMech   [1] MechType      OPTIONAL,
                    //       -- present only in the first reply from the target
                    //     responseToken   [2] OCTET STRING  OPTIONAL,
                    //     mechListMIC     [3] OCTET STRING  OPTIONAL,
                    //     ...
                    // }

                    // negotiate_write_neg_token

                    // 0000 - 0xa1 0x82 0x05 0x4d 0x30 0x82 0x05 0x49 0xa0 0x03 0x0a 0x01 0x01 0xa1 0x0b 0x06
                    // 0010 - 0x09 0x2a 0x86 0x48 0x82 0xf7 0x12 0x01 0x02 0x02 0xa2 0x82 0x05 0x33 0x04 0x82
                    // 0020 - 0x05 0x2f

                    auto neg_state_field = BER::mkSmallEnumeratedField(Accept_Incomplete, 0);

                    auto ms_kerberos_header = BER::mkObjectIdentifierHeader(this->accepted_mechanism->length);

                    auto supported_mech_header = BER::mkContextualFieldHeader(
                        ms_kerberos_header.size() + this->accepted_mechanism->length, 1);

                    auto octet_string_header = BER::mkOctetStringHeader(application.size());

                    auto response_token_header = BER::mkContextualFieldHeader(
                        octet_string_header.size() + application.size(), 2);

                    size_t const neg_token_resp_length = neg_state_field.size()
                        + supported_mech_header.size()
                        + ms_kerberos_header.size()
                        + this->accepted_mechanism->length
                        + response_token_header.size()
                        + octet_string_header.size() + application.size();

                    auto neg_token_resp_sequence_header = BER::mkSequenceHeader(neg_token_resp_length);

                    // TSRequest ::= SEQUENCE {
                    //     version [0] INTEGER,
                    //     negoTokens [1] NegoData OPTIONAL,
                    //     authInfo [2] OCTET STRING OPTIONAL,
                    //     pubKeyAuth [3] OCTET STRING OPTIONAL,
                    //     errorCode [4] INTEGER OPTIONAL,
                    //     clientNonce [5] OCTET STRING OPTIONAL
                    // }

                    auto nego_tokens_header = BER::mkContextualFieldHeader(
                        neg_token_resp_sequence_header.size() + neg_token_resp_length, 1);

                    LOG_IF(this->verbose, LOG_INFO,
                        "Krb5Server::authenticate(): AcceptedMechanism=%s(%s)",
                        oidToLabel(this->accepted_mechanism), oidToString(this->accepted_mechanism));

                    std::vector<uint8_t> nego_tokens = std::vector<uint8_t>{}
                        << nego_tokens_header
                        << neg_token_resp_sequence_header
                        << neg_state_field
                        << supported_mech_header
                        << ms_kerberos_header;
                    nego_tokens.insert(nego_tokens.end(),
                        static_cast<uint8_t*>(this->accepted_mechanism->elements),
                        static_cast<uint8_t*>(this->accepted_mechanism->elements) + this->accepted_mechanism->length);
                    nego_tokens << response_token_header << octet_string_header;
                    nego_tokens.insert(nego_tokens.end(),
                        application.data(), application.data() + application.size());

                    LOG_IF(this->verbose, LOG_INFO,
                        "Krb5Server::authenticate(): negoTokensLength=%lu", nego_tokens.size());
                    if (this->dump) {
                        hexdump_d(nego_tokens);
                    }

                    ts_request.negoTokens.assign(
                        nego_tokens.data(),
                        nego_tokens.data() + nego_tokens.size());
                }
                else if (KerberosState::ApReq == this->kerberos_state)
                {
                    krb5_data inbuf {
                        .magic = 0,
                        .length = static_cast<unsigned int>(input_token.size()),
                        .data = const_cast<char*>(char_ptr_cast(input_token.data()))
                    };
                    krb5_flags ap_flags = 0;
                    krb5_ticket* ticket = nullptr;
                    krb5_error_code error_code = ::krb5_rd_req(
                        this->context, &this->auth_context, &inbuf, NULL,
                        this->keytab, &ap_flags, &ticket);
                    if (error_code)
                    {
                        logKerbrosError(
                            "Krb5Server::authenticate(): krb5_rd_req",
                                "Failed to parse and decrypt KRB_AP_REQ message!",
                            this->context, error_code);

                        return credssp::State::Err;
                    }

                    char *client_principal = nullptr;
                    error_code = ::krb5_unparse_name(this->context, ticket->enc_part2->client,
                        &client_principal);
                    if (error_code)
                    {
                        logKerbrosError(
                            "Krb5Server::authenticate(): krb5_unparse_name",
                                "Failed to convert (client) krb5_principal structure to string representation!",
                            this->context, error_code);
                    }
                    else
                    {
                        this->client_principal_name = client_principal;

                        LOG_IF(this->verbose, LOG_INFO,
                            "Krb5Server::authenticate(): ClientPrincipal=%s", this->client_principal_name.c_str());

                        ::krb5_free_unparsed_name(this->context, client_principal);
                    }

                    ::krb5_free_ticket(this->context, ticket);

                    error_code = ::krb5_auth_con_setflags(this->context, this->auth_context,
                        KRB5_AUTH_CONTEXT_DO_SEQUENCE | KRB5_AUTH_CONTEXT_USE_SUBKEY);
                    if (error_code)
                    {
                        logKerbrosError(
                            "Krb5Server::authenticate(): krb5_auth_con_setflags",
                                "Failed to set flags field in authentication context structure!",
                            this->context, error_code);

                        return credssp::State::Err;
                    }

                    krb5_authenticator* authenticator = nullptr;
                    error_code = ::krb5_auth_con_getauthenticator(this->context,
                        this->auth_context, &authenticator);
                    if (error_code)
                    {
                        logKerbrosError(
                            "Krb5Server::authenticate(): krb5_auth_con_getauthenticator",
                                "Failed to retrieve the authenticator from authentication context!",
                            this->context, error_code);

                        return credssp::State::Err;
                    }

                    #define GSS_CHECKSUM_TYPE 0x8003
                    if (!authenticator || !authenticator->checksum ||
                        authenticator->checksum->checksum_type != GSS_CHECKSUM_TYPE || authenticator->checksum->length < 24)
                    {
                        LOG(LOG_ERR,
                            "Krb5Server::authenticate(): Bad token! Err.");
                        return credssp::State::Err;
                    }
                    this->kerberos_flags = *reinterpret_cast<uint32_t*>(authenticator->checksum->contents + 20);
                    LOG_IF(this->verbose, LOG_INFO,
                        "Krb5Server::authenticate(): Flags=0x%X", this->kerberos_flags);

                    std::vector<uint8_t> application;
                    if ((ap_flags & AP_OPTS_MUTUAL_REQUIRED) && (this->kerberos_flags & SSPI_GSS_C_MUTUAL_FLAG))
                    {
                        krb5_data output_token{};
                        error_code = ::krb5_mk_rep(this->context, this->auth_context, &output_token);
                        if (error_code)
                        {
                            logKerbrosError(
                                "Krb5Server::authenticate(): krb5_mk_rep",
                                    "Failed to format and encrypt a KRB_AP_REP message!",
                                this->context, error_code);

                            return credssp::State::Err;
                        }

                        StaticOutStream<8> token_id;
                        token_id.out_uint16_be(TOK_ID_AP_REP);

                        auto this_mech_header = BER::mkObjectIdentifierHeader(OID_token->length);

                        auto application_header = BER::mkApplicationFieldHeader(
                            this_mech_header.size()
                            + OID_token->length
                            + token_id.get_offset()
                            + output_token.length,
                            0
                        );

                        application = std::vector<uint8_t>{}
                            << application_header
                            << this_mech_header;
                        application.insert(application.end(),
                            static_cast<uint8_t*>(OID_token->elements),
                            static_cast<uint8_t*>(OID_token->elements) + OID_token->length);
                        application << token_id.get_produced_bytes();
                        application.insert(application.end(), output_token.data, output_token.data + output_token.length);

                        LOG_IF(this->verbose, LOG_INFO,
                            "Krb5Server::authenticate(): ApReplyLength=%lu", application.size());
                        if (this->dump) {
                            hexdump_d(application);
                        }

                        if (this->kerberos_flags & SSPI_GSS_C_SEQUENCE_FLAG)
                        {
                            error_code = ::krb5_auth_con_getlocalseqnumber(this->context, this->auth_context, &this->kerberos_local_sequence_no);
                            if (error_code)
                            {
                                logKerbrosError(
                                    "Krb5Server::authenticate(): krb5_auth_con_getlocalseqnumber",
                                        "Failed to retrieve the local sequence number from auth context!",
                                    this->context, error_code);

                                return credssp::State::Err;
                            }
                            LOG_IF(this->verbose, LOG_INFO,
                                "Krb5Server::authenticate(): LocalSeqNumber=%d", this->kerberos_local_sequence_no);

                            error_code = ::krb5_auth_con_getremoteseqnumber(this->context, this->auth_context, &this->kerberos_remote_sequence_no);
                            if (error_code)
                            {
                                logKerbrosError(
                                    "Krb5Server::authenticate(): krb5_auth_con_getremoteseqnumber",
                                        "Failed to retrieve the remote sequence number from auth context!",
                                    this->context, error_code);

                                return credssp::State::Err;
                            }
                            LOG_IF(this->verbose, LOG_INFO,
                                "Krb5Server::authenticate(): RemoteSeqNumber=%d", this->kerberos_remote_sequence_no);
                        }

                        ::krb5_k_free_key(this->context, this->kerberos_session_key);
                        ::krb5_k_free_key(this->context, this->kerberos_initiator_key);
                        ::krb5_k_free_key(this->context, this->kerberos_acceptor_key);

                        error_code = ::krb5_auth_con_getkey_k(this->context, this->auth_context, &this->kerberos_session_key);
                        if (error_code)
                        {
                            logKerbrosError(
                                "Krb5Server::authenticate(): krb5_auth_con_getkey_k",
                                    "Failed to retrieve the session key from auth context!",
                                this->context, error_code);

                            return credssp::State::Err;
                        }

                        error_code = ::krb5_auth_con_getsendsubkey_k(this->context, this->auth_context, &this->kerberos_acceptor_key);
                        if (error_code)
                        {
                            logKerbrosError(
                                "Krb5Server::authenticate(): krb5_auth_con_getkey_k",
                                    "Failed to retrieve the acceptor key from auth context!",
                                this->context, error_code);

                            return credssp::State::Err;
                        }

                        this->kerberos_encryption_type =
                            ::krb5_k_key_enctype(this->context, this->kerberos_acceptor_key);

                        error_code = ::krb5_auth_con_getrecvsubkey_k(this->context, this->auth_context, &this->kerberos_initiator_key);
                        if (error_code)
                        {
                            logKerbrosError(
                                "Krb5Server::authenticate(): krb5_auth_con_getkey_k",
                                    "Failed to retrieve the initiator key from auth context!",
                                this->context, error_code);

                            return credssp::State::Err;
                        }
                    }

                    if (this->use_spnego)
                    {
                        // NegTokenResp ::= SEQUENCE {
                        //     negState       [0] ENUMERATED {
                        //         accept-completed    (0),
                        //         accept-incomplete   (1),
                        //         reject              (2),
                        //         request-mic         (3)
                        //     }                                 OPTIONAL,
                        //       -- REQUIRED in the first reply from the target
                        //     supportedMech   [1] MechType      OPTIONAL,
                        //       -- present only in the first reply from the target
                        //     responseToken   [2] OCTET STRING  OPTIONAL,
                        //     mechListMIC     [3] OCTET STRING  OPTIONAL,
                        //     ...
                        // }

                        // negotiate_write_neg_token

                        // 0000 - 0xa1 0x81 0xaa 0x30 0x81 0xa7 0xa0 0x03 0x0a 0x01 0x00 0xa2 0x81 0x9f 0x04 0x81
                        // 0010 - 0x9c

                        auto neg_state_field = BER::mkSmallEnumeratedField(Accept_Complete, 0);

                        auto octet_string_header = BER::mkOctetStringHeader(application.size());

                        auto response_token_header = BER::mkContextualFieldHeader(
                            octet_string_header.size() + application.size(), 2);

                        size_t const neg_token_resp_length = neg_state_field.size()
                            + response_token_header.size()
                            + octet_string_header.size() + application.size();

                        auto neg_token_resp_sequence_header = BER::mkSequenceHeader(neg_token_resp_length);

                        // TSRequest ::= SEQUENCE {
                        //     version [0] INTEGER,
                        //     negoTokens [1] NegoData OPTIONAL,
                        //     authInfo [2] OCTET STRING OPTIONAL,
                        //     pubKeyAuth [3] OCTET STRING OPTIONAL,
                        //     errorCode [4] INTEGER OPTIONAL,
                        //     clientNonce [5] OCTET STRING OPTIONAL
                        // }

                        auto nego_tokens_header = BER::mkContextualFieldHeader(
                            neg_token_resp_sequence_header.size() + neg_token_resp_length, 1);

                        std::vector<uint8_t> nego_tokens = std::vector<uint8_t>{}
                            << nego_tokens_header
                            << neg_token_resp_sequence_header
                            << neg_state_field;
                        nego_tokens << response_token_header << octet_string_header;
                        nego_tokens.insert(nego_tokens.end(),
                            application.data(), application.data() + application.size());

                        LOG_IF(this->verbose, LOG_INFO,
                            "Krb5Server::authenticate(): negoTokensLength=%lu", nego_tokens.size());
                        if (this->dump) {
                            hexdump_d(nego_tokens);
                        }

                        ts_request.negoTokens.assign(
                            nego_tokens.data(),
                            nego_tokens.data() + nego_tokens.size());
                    }
                    else
                    {
                        ts_request.negoTokens.assign(
                            application.data(),
                            application.data() + application.size());
                    }

                    if (this->kerberos_flags & SSPI_GSS_C_CONF_FLAG)
                    {
                        unsigned int header_size = 0;
                        error_code = ::krb5_c_crypto_length(this->context,
                            this->kerberos_encryption_type, KRB5_CRYPTO_TYPE_HEADER, &header_size);
                        if (error_code)
                        {
                            logKerbrosError(
                                "Krb5Server::authenticate(): krb5_c_crypto_length",
                                    "Failed to retrieve the length of message header!",
                                this->context, error_code);

                            return credssp::State::Err;
                        }
                        LOG_IF(this->verbose, LOG_INFO,
                            "Krb5Server::authenticate(): HeaderSize=%u", header_size);

                        unsigned int padding_size = 0;
                        error_code = ::krb5_c_crypto_length(this->context,
                            this->kerberos_encryption_type, KRB5_CRYPTO_TYPE_PADDING, &padding_size);
                        if (error_code)
                        {
                            logKerbrosError(
                                "Krb5Server::authenticate(): krb5_c_crypto_length",
                                    "Failed to retrieve the length of message padding!",
                                this->context, error_code);

                            return credssp::State::Err;
                        }
                        LOG_IF(this->verbose, LOG_INFO,
                            "Krb5Server::authenticate(): PaddingSize=%u", padding_size);

                        unsigned int trailer_size = 0;
                        error_code = ::krb5_c_crypto_length(this->context,
                            this->kerberos_encryption_type, KRB5_CRYPTO_TYPE_TRAILER, &trailer_size);
                        if (error_code)
                        {
                            logKerbrosError(
                                "Krb5Server::authenticate(): krb5_c_crypto_length",
                                    "Failed to retrieve the length of message trailer!",
                                this->context, error_code);

                            return credssp::State::Err;
                        }
                        LOG_IF(this->verbose, LOG_INFO,
                            "Krb5Server::authenticate(): TrailerSize=%u", trailer_size);

                        // GSS header (16 bytes) + encrypted header = 32 bytes
                        this->kerberos_security_trailer_size =
                            header_size + padding_size + trailer_size + 32;

                        LOG_IF(this->verbose, LOG_INFO,
                            "Krb5Server::authenticate(): SecurityTrailerSize=%u", this->kerberos_security_trailer_size);
                    }

                    this->nla_step = NLAStep::pubKeyAuth;
                }
            }
            else
            {
                LOG(LOG_ERR,
                    "Krb5Server::authenticate(): "
                       "negoToken received at wrong nla_step! CurrentStep=%s(%d) Err.",
                    nlaStepToString(this->nla_step), this->nla_step);
                this->nla_step = NLAStep::final;
                return credssp::State::Err;
            }
        }

        if (not ts_request.authInfo.empty())
        {
            if (NLAStep::authInfo == this->nla_step)
            {
                LOG_IF(this->verbose, LOG_INFO,
                    "Krb5Server::authenticate(): Process authInfo - Length=%lu",
                    ts_request.authInfo.size());
                if (this->dump) {
                    hexdump_d(ts_request.authInfo);
                }

                if (!this->kerberosDecryptToken(ts_request.authInfo))
                {
                    LOG(LOG_ERR,
                        "Krb5Server::authenticate(): Failed to decrypt token! Err.");
                    return credssp::State::Err;
                }

                this->nla_recv_sequence_no++;

                bytes_view creds_data(
                    ts_request.authInfo.data() + this->kerberos_security_trailer_size,
                    ts_request.authInfo.size() - this->kerberos_security_trailer_size
                );

                LOG_IF(this->verbose, LOG_INFO,
                    "Krb5Server::authenticate(): TSCredentials Length=%lu", creds_data.size());
                if (this->dump) {
                    hexdump_d(creds_data);
                }

                // TSCredentials ::= SEQUENCE {
                //     credType [0] INTEGER,
                //     credentials [1] OCTET STRING
                // }

                // TSPasswordCreds ::= SEQUENCE {
                //     domainName [0] OCTET STRING,
                //     userName [1] OCTET STRING,
                //     password [2] OCTET STRING
                // }

                this->ts_credentials = recvTSCredentials(creds_data, this->verbose);

                this->nla_step = NLAStep::final;

                LOG_IF(this->verbose, LOG_INFO,
                       "Krb5Server::authenticate(): Finish.");
                return credssp::State::Finish;
            }
            else
            {
                LOG(LOG_ERR,
                    "Krb5Server::authenticate(): "
                        "authInfo received at wrong nla_step! CurrentStep=%s(%d) Err.",
                        nlaStepToString(this->nla_step), this->nla_step);

                this->nla_step = NLAStep::final;
                return credssp::State::Err;
            }
        }

        if (not ts_request.pubKeyAuth.empty())
        {
            if (NLAStep::pubKeyAuth == this->nla_step)
            {
                LOG_IF(this->verbose, LOG_INFO,
                    "Krb5Server::authenticate(): Process pubKeyAuth - Length=%lu",
                    ts_request.pubKeyAuth.size());
                if (this->dump) {
                    hexdump_d(ts_request.pubKeyAuth);
                }
                LOG_IF(this->verbose, LOG_INFO,
                    "Krb5Server::authenticate(): clientNonce - Length=%lu",
                    ts_request.clientNonce.clientNonce.size());
                if (this->dump) {
                    hexdump_d(ts_request.clientNonce.clientNonce);
                }

                if (!this->kerberosDecryptToken(ts_request.pubKeyAuth))
                {
                    LOG(LOG_ERR,
                        "Krb5Server::authenticate(): Failed to decrypt token! (2) Err.");
                    return credssp::State::Err;
                }

                std::vector<uint8_t> clientServerHash;
                generatePublicKeyHashClientToServer(
                    ts_request.clientNonce.clientNonce, this->public_key,
                    clientServerHash);

                if ((ts_request.pubKeyAuth.size() !=
                     (this->kerberos_security_trailer_size + clientServerHash.size())) ||
                    memcmp(&clientServerHash[0],
                           &ts_request.pubKeyAuth[this->kerberos_security_trailer_size],
                           clientServerHash.size()))
                {
                    LOG(LOG_ERR,
                        "Krb5Server::authenticate(): Could not verify server's hash! Err.");
                    return credssp::State::Err;
                }

                this->nla_recv_sequence_no++;

                LOG_IF(this->verbose, LOG_INFO, "Krb5Server::authenticate(): Public key hash decrypted.");

                ts_request.negoTokens.clear();

                std::vector<uint8_t> serverClientHash;
                generatePublicKeyHashServerToClient(
                    ts_request.clientNonce.clientNonce, this->public_key,
                    serverClientHash);

                std::vector<uint8_t> publicKeyHash(
                    this->kerberos_security_trailer_size + serverClientHash.size());
                memcpy(&publicKeyHash[this->kerberos_security_trailer_size],
                       &serverClientHash[0], serverClientHash.size());

                krb5_crypto_iov encrypt_iov[] = {
                    { KRB5_CRYPTO_TYPE_HEADER, { 0, 0, nullptr } },
                    { KRB5_CRYPTO_TYPE_DATA, { 0, 0, nullptr } },
                    { KRB5_CRYPTO_TYPE_DATA, { 0, 0, nullptr } },
                    { KRB5_CRYPTO_TYPE_PADDING, { 0, 0, nullptr } },
                    { KRB5_CRYPTO_TYPE_TRAILER, { 0, 0, nullptr } }
                };

                uint8_t const encrypt_flags =
                    FLAG_SENDER_IS_ACCEPTOR | FLAG_WRAP_CONFIDENTIAL | FLAG_ACCEPTOR_SUBKEY;

                encrypt_iov[1].data.length = serverClientHash.size();
                encrypt_iov[2].data.length = 16;

                krb5_error_code error_code = ::krb5_c_crypto_length_iov(this->context, this->kerberos_encryption_type,
                    encrypt_iov, sizeof(encrypt_iov) / sizeof(encrypt_iov[0]));
                if (error_code) {
                    logKerbrosError(
                        "Krb5Server::authenticate(): krb5_c_crypto_length_iov",
                            "Failed to Fill in lengths for header, trailer and padding in IOV array!",
                        this->context, error_code);

                    return credssp::State::Err;
                }
                if (encrypt_iov[0].data.length + encrypt_iov[3].data.length + encrypt_iov[4].data.length + 32 >
                    this->kerberos_security_trailer_size)
                {
                    LOG(LOG_ERR,
                        "Krb5Server::authenticate(): Insufficient memory! Err.");
                    return credssp::State::Err;
                }

                encrypt_iov[2].data.data = char_ptr_cast(&publicKeyHash[16]);
                encrypt_iov[3].data.data = encrypt_iov[2].data.data + encrypt_iov[2].data.length;
                encrypt_iov[4].data.data = encrypt_iov[3].data.data + encrypt_iov[3].data.length;
                encrypt_iov[0].data.data = encrypt_iov[4].data.data + encrypt_iov[4].data.length;
                encrypt_iov[1].data.data = char_ptr_cast(&publicKeyHash[this->kerberos_security_trailer_size]);

                {
                    OutStream os_encrypt_iov({publicKeyHash.data(), publicKeyHash.size()});
                    os_encrypt_iov.out_uint16_be(TOK_ID_WRAP);
                    os_encrypt_iov.out_uint8(encrypt_flags);
                    os_encrypt_iov.out_uint8(0xFF);
                    os_encrypt_iov.out_uint32_be(0);    // ec + rrc
                    os_encrypt_iov.out_uint64_be(
                        this->kerberos_local_sequence_no + this->nla_send_sequence_no);
                }

                memcpy(encrypt_iov[2].data.data, &publicKeyHash[0], 16);

                {
                    OutStream os_encrypt_iov({publicKeyHash.data(), publicKeyHash.size()});
                    os_encrypt_iov.out_skip_bytes(6);
                    os_encrypt_iov.out_uint16_be(
                        16 + encrypt_iov[3].data.length + encrypt_iov[4].data.length);
                }

                error_code = ::krb5_k_encrypt_iov(this->context, this->kerberos_acceptor_key, KG_USAGE_ACCEPTOR_SEAL,
                    NULL, encrypt_iov, sizeof(encrypt_iov) / sizeof(encrypt_iov[0]));
                if (error_code)
                {
                    LOG(LOG_ERR,
                        "Krb5Server::authenticate(): Failed to encrypt data! Err.");
                    return credssp::State::Err;
                }

                ts_request.pubKeyAuth = std::move(publicKeyHash);

                LOG_IF(this->verbose, LOG_INFO, "Krb5Server::authenticate(): Public key hash encrypted.");

                this->nla_send_sequence_no++;

                this->nla_step = NLAStep::authInfo;
            }
            else
            {
                LOG(LOG_ERR,
                    "Krb5Server::authenticate(): "
                        "pubKeyAuth received at wrong nla_step! CurrentStep=%s(%d) Err.",
                        nlaStepToString(this->nla_step), this->nla_step);

                this->nla_step = NLAStep::final;
                return credssp::State::Err;
            }
        }

        ts_request.error_code = 0;
        auto v = emitTSRequest(ts_request.version,
                               ts_request.negoTokens,
                               ts_request.authInfo,
                               ts_request.pubKeyAuth,
                               ts_request.error_code,
                               ts_request.clientNonce.clientNonce,
                               ts_request.clientNonce.initialized,
                               this->verbose);

        out_data = std::move(v);

        LOG_IF(this->verbose, LOG_INFO,
               "Krb5Server::authenticate(): Cont. out_data.size()=%lu",
               out_data.size());
        return credssp::State::Cont;
    }

    std::string const& getClientPrincipalName() const
    {
        return this->client_principal_name;
    }
};
