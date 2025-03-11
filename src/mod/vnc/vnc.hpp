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
   Copyright (C) Wallix 2010
   Author(s): Christophe Grosjean, Javier Caverni, Clément Moroldo
   Based on xrdp Copyright (C) Jay Sorg 2004-2010

   Vnc module
*/

#pragma once

#include "transport/transport.hpp"

#include "core/events.hpp"
#include "core/buf64k.hpp"
#include "core/channel_list.hpp"
#include "core/front_api.hpp"
#include "core/server_cert_params.hpp"
#include "core/RDP/clipboard.hpp"
#include "core/RDP/orders/RDPOrdersSecondaryColorCache.hpp"
#include "core/WinNT/time.hpp"

#include "gdi/graphic_api.hpp"

#include "keyboard/keymapsym.hpp"

#include "utils/random.hpp"
#include "utils/utf.hpp"
#include "utils/zlib.hpp"
#include "utils/monotonic_clock.hpp"
#include "utils/stream.hpp"
#include "utils/sugar/not_null_ptr.hpp"

#include "system/ssl_sha256.hpp"

#include "mod/mod_api.hpp"
#include "mod/tls_params.hpp"

#include "mod/vnc/encoders/copyrect.hpp"
#include "mod/vnc/encoders/cursor.hpp"
#include "mod/vnc/encoders/hextile.hpp"
#include "mod/vnc/encoders/raw.hpp"
#include "mod/vnc/encoders/rre.hpp"
#include "mod/vnc/encoders/zrle.hpp"
#include "mod/vnc/encoders/rfb/cut_text.hpp"
#include "mod/vnc/encoders/uvnc_file_transfer.hpp"
#include "mod/vnc/vnc_verbose.hpp"

#include "mod/vnc/rdp_adapters/cliprdr_adapter.hpp"
#include "mod/vnc/rdp_adapters/vnc_file_list.hpp"
#include "mod/vnc/rdp_adapters/cliprdr_file_list.hpp"

#include "mod/vnc/file_transfer/file_transfer_gui.hpp"
#include "mod/vnc/vnc_params.hpp"

#include "configs/autogen/enums.hpp"


class UltraDSM;
class ClientExecute;

enum class FileValidatorId : uint32_t;

// got extracts of VNC documentation from
// https://github.com/rfbproto/rfbproto

class mod_vnc : public mod_api
{
    // TODO
    static constexpr int maxSpokenVncProcotol = 3 * 1000 + 8; // 3.8

    /* mod data */
    char mod_name[256] {0};
    char username[256] {0};
    char password[256] {0};

    FrontAPI& front;

public:

    /** @brief transport for VNC */
    class VncTransport
    {
    public:
        VncTransport(mod_vnc & mod, Transport & t)
        : m_trans(t)
        , m_mod(mod)
        {}

        void send(byte_ptr buffer, size_t len)
        {
            send(bytes_view(buffer, len));
        }

        void send(bytes_view buffer);

        int get_fd() const {
            return m_trans.get_fd();
        }

        Transport &get_transport() const { return m_trans; }

    private:
        Transport & m_trans;
        mod_vnc & m_mod;
    };

    /** @brief a custom Vnc Buf64k */
    struct VncBuf64k : Buf64k
    {
        VncBuf64k(mod_vnc &mod)
            : m_mod(mod)
        {
        }

        size_type read_from(VncTransport vncTrans);

    private:
        mod_vnc & m_mod;
    };

    /**
     *
     */
    struct Mouse {
        void move(OutStream & out_stream, uint16_t x, uint16_t y)
        {
            this->x = x;
            this->y = y;
            this->send(out_stream);
        }

        void click(OutStream & out_stream, uint16_t x, uint16_t y, uint8_t mask, bool set)
        {
            if (set) {
                this->mod_mouse_state |= mask;
            }
            else {
                this->mod_mouse_state &= ~mask;
            }
            this->x = x;
            this->y = y;
            this->send(out_stream);
        }

        void scroll(OutStream & out_stream, uint8_t mask) const
        {
            this->write(out_stream, this->mod_mouse_state | mask);
            this->write(out_stream, this->mod_mouse_state);
        }

        uint16_t mouse_x() const noexcept
        {
            return this->x;
        }

        uint16_t mouse_y() const noexcept
        {
            return this->y;
        }

    private:
        uint8_t mod_mouse_state = 0;
        uint16_t x = 0;
        uint16_t y = 0;

        void write(OutStream & stream, uint8_t state) const
        {
            stream.out_uint8(5);
            stream.out_uint8(state);
            stream.out_uint16_be(this->x);
            stream.out_uint16_be(this->y);
        }

        void send(OutStream & out_stream) const
        {
            this->write(out_stream, this->mod_mouse_state);
        }
    } mouse;


private:
    VncTransport t;
    std::unique_ptr<UltraDSM> dsm;
    bool dsmEncryption;

    uint16_t width;
    uint16_t height;
    BitsPerPixel bpp {};
    // TODO BytesPerPixel ?
    uint8_t depth = 0;

    uint8_t endianess;
    uint8_t true_color_flag;

    uint16_t red_max;
    uint16_t green_max;
    uint16_t blue_max;

    uint8_t red_shift;
    uint8_t green_shift;
    uint8_t blue_shift;

    VNCVerbose verbose;

    KeymapSym keymapSym;

    /** @brief state of the VNC state machine */
    enum VncState {
        DO_INITIAL_CLEAR_SCREEN,
        UP_AND_RUNNING,
        WAIT_SECURITY_TYPES,
        WAIT_SECURITY_TYPES_LEVEL,
        WAIT_SECURITY_TYPES_PASSWORD_AND_SERVER_RANDOM,
        WAIT_SECURITY_TYPES_PASSWORD_AND_SERVER_RANDOM_RESPONSE,
        WAIT_SECURITY_TYPES_MS_LOGON,
        WAIT_SECURITY_TYPES_MS_LOGON_RESPONSE,
        WAIT_SECURITY_TYPES_INVALID_AUTH,
        WAIT_SECURITY_RESULT,
        WAIT_SECURITY_ULTRA_CHALLENGE,
        DO_VENCRYPT_HANDSHAKE,
        SERVER_INIT,
        SERVER_INIT_RESPONSE,
        WAIT_CLIENT_UP_AND_RUNNING
    };

    /** @brief state for the VeNCrypt state machine */
    enum VeNCryptState {
        WAIT_VENCRYPT_VERSION,
        WAIT_VENCRYPT_VERSION_RESPONSE,
        WAIT_VENCRYPT_SUBTYPES,
        WAIT_VENCRYPT_AUTH_ANSWER
    };

    /** @brief status returned in a security reason */
    enum SecurityReasonStatus {
        SECURITY_REASON_OK = 0,
        SECURITY_REASON_FAILED = 1,
        SECURITY_REASON_TOO_MANY_ATTEMPTS = 2,
        SECURITY_REASON_CONTINUE = 0xffffffff // returned by UltraVNC
    };

private:
    std::string encodings;

    VncState state = WAIT_SECURITY_TYPES;
    VeNCryptState vencryptState = WAIT_VENCRYPT_VERSION;

    MonotonicTimePoint::duration session_time_start;
    ClientExecute* rail_client_execute = nullptr;

    Random & rand;
    not_null_ptr<gdi::GraphicApi> gd;
    gdi::GraphicApi & original_gd;
    EventsGuard events_guard;

    SessionLogApi & session_log;

    /** @brief type of VNC authentication */
    enum VncAuthType : int32_t {
        VNC_AUTH_INVALID     = 0,
        VNC_AUTH_NONE         = 1,
        VNC_AUTH_VNC         = 2,
        VNC_AUTH_TIGHT         = 16,
        VNC_AUTH_ULTRA        = 17,
        VNC_AUTH_TLS         = 18,
        VNC_AUTH_VENCRYPT    = 19,
        VNC_AUTH_DIFFIE_HELLMAN = 30,  // 0x1E  unsupported, update name in securityTypeString() when implemented
        VNC_AUTH_APPLE = 33,  // 0x21  unsupported, update name in securityTypeString() when implemented
        VNC_AUTH_ULTRA_MsLogonIAuth = 112,
        VNC_AUTH_ULTRA_MsLogonIIAuth = 113,
        VNC_AUTH_ULTRA_SecureVNCPluginAuth = 114,
        VNC_AUTH_ULTRA_SecureVNCPluginAuth_new = 115,
        VeNCRYPT_TLSNone     = 257,
        VeNCRYPT_TLSVnc     = 258,
        VeNCRYPT_TLSPlain     = 259,
        VeNCRYPT_X509None    = 260,
        VeNCRYPT_X509Vnc    = 261,
        VeNCRYPT_X509Plain    = 262,
        VNC_AUTH_ULTRA_MS_LOGON = -6,
    };

    VncAuthType choosenAuth;
    VncAuthType force_authentication_method = VNC_AUTH_INVALID;

    const bool cursor_pseudo_encoding_supported;

    Zdecompressor<> zd;

    // FT / Cb features
    //@{
    // TODO u8 or u16
    enum class FtFlags : uint32_t;
    REDEMPTION_DECLARE_ENUM_FLAGS_IN_CLASS(FtFlags)

    struct FT;
    friend FT;

    struct dynamic_non_null_cstring
    {
        dynamic_non_null_cstring() = default;

        ~dynamic_non_null_cstring();

        void init(chars_view str);

        char const * c_str() const noexcept
        {
            return m_str;
        }

    private:
        void _free() noexcept;

        char const * m_str = "";
    };


    enum class TransferValidatorStatus : uint8_t;

    enum class FileStorageOption : uint8_t
    {
        OnInvalidFile,
        Always,
    };

    struct TransferedFileCtx
    {
        TransferedFileCtx & operator = (TransferedFileCtx const &) = delete;

        struct TflFile;

        struct Utf8FileName
        {
            Utf8FileName() noexcept {}

            explicit Utf8FileName(VNC::UVncFile::PathView file_name) noexcept
            {
                reset(file_name);
            }

            Utf8FileName(Utf8FileName const &) noexcept;

            Utf8FileName & operator = (Utf8FileName const &) noexcept;

            void reset(VNC::UVncFile::PathView file_name) noexcept;

            void reset() noexcept
            {
                m_len = 0;
            }

            bool is_empty() const noexcept
            {
                return m_len == 0;
            }

            chars_view av() const noexcept
            {
                return {char_ptr_cast(m_buffer), m_len};
            }

        private:
            uint16_t m_len = 0;
            uint8_t m_buffer[VNC::UVncFile::PathView::Bytes::at_most * 3];
        };

        struct Hash
        {
            uint8_t buffer[SslSha256::DIGEST_LENGTH];
        };

        struct File
        {
            File();
            File(File &&) = default;
            File & operator = (File &&) = default;

            ~File();

            std::unique_ptr<TflFile> tfl_file {};
            TransferValidatorStatus validator_status {};
            FileValidatorId validator_id {};
            FileValidatorTargets validator_target = FileValidatorTargets::None;
            Hash hash;
            uint64_t file_size = 0;
            Utf8FileName utf8_file_name {};
        };

        FileValidatorService * file_validator = nullptr;
        FdxCapture * fdx_capture = nullptr;

        FileValidatorTargets validator_targets = FileValidatorTargets::None;
        FileValidatorTargets block_invalid_file = FileValidatorTargets::None;

        FileStorageOption file_storage_option = FileStorageOption::OnInvalidFile;
        bool log_if_accepted = false;

        uint64_t original_max_blocked_file_size_rejected = 0;
        // deal with original value and CB_HUGE_FILE_SUPPORT_ENABLED flag
        uint64_t max_blocked_file_size_rejected_upload = 0;
        uint64_t max_blocked_file_size_rejected_download = 0;

        dynamic_non_null_cstring tmp_dir {};

        std::vector<File> waiting_validator_file_list {};

        SslSha256 sha2 {};

        File current_file {};

        ModVncParams::GetFileValidatorAndStorage get_file_validator_and_storage;
    };

    CHANNELS::ChannelDef const * cliprdr_chann;
    FtFlags ft_flags {};
    bool has_cursor = false;
    VNC::CliprdrAdapter cliprdr;
    VNC::CliprdrFileList cliprdr_file_list;
    VNC::VncFileList uvnc_file_list;
    VNC::FileTransferGui ft_gui;
    UVNCFileTransferReader ft_reader;
    TransferedFileCtx transfered_file_ctx;
    //@}

    struct DoTlsParams
    {
        std::string certif_path;
        ModTlsParams::ServerCertParams server_cert;
        TlsConfig tls_config;

        const bool server_cert_check_using_ca;
        const std::string ca_certificates;
        const std::string target_host;
    };
    DoTlsParams do_tls_params;

public:
    mod_vnc( Transport & t
           , Random & rand
           , gdi::GraphicApi & gd
           , Font const & glyphs
           , EventContainer & events
           , const char * username
           , const char * password
           , FrontAPI & front
           // TODO: front width and front height should be provided through info
           , uint16_t front_width
           , uint16_t front_height
           , ModVncParams params
           , const char * encodings
           , KeyLayout const& layout
           , kbdtypes::KeyLocks locks
           , bool server_is_macos
           , bool server_is_unix
           , bool cursor_pseudo_encoding_supported
           , ClientExecute* rail_client_execute
           , VNCVerbose verbose
           , SessionLogApi& session_log
           , ModTlsParams const& tls_params
           , std::string_view force_authentication_method
           , Translator const& translator
    );

    ~mod_vnc();

    template<std::size_t MaxLen>
    class MessageCtx
    {
        static_assert(MaxLen < 32*1024, "inefficient");

        enum State
        {
            Size,
            Data,
            Strip,
        };

    public:
        void restart()
        {
            this->state = State::Size;
        }

        template<class F>
        bool run(Buf64k & buf, F && f)
        {
            switch (this->state)
            {
                case State::Size:
                    this->state = this->read_size(buf);
                    if (this->state != State::Data) {
                        return false;
                    }
                    [[fallthrough]];
                case State::Data:
                    this->state = this->read_data(buf, f);
                    if (this->state == State::Size) {
                        return true;
                    }
                    break;
                case State::Strip:
                    this->state = this->strip_data(buf);
                    if (this->state == State::Size) {
                        return true;
                    }
                    break;
            }
            return false;
        }

    private:
        State state = State::Size;
        uint32_t len;

        State read_size(Buf64k & buf)
        {
            const size_t sz = 4;

            if (buf.remaining() < sz)
            {
                return State::Size;
            }

            this->len = InStream(buf.av(sz)).in_uint32_be();

            buf.advance(sz);

            return State::Data;
        }

        template<class F>
        State read_data(Buf64k & buf, F & f)
        {
            const size_t sz = std::min<size_t>(MaxLen, this->len);

            if (buf.remaining() < sz)
            {
                return State::Data;
            }

            f(u8_array_view{buf.av().data(), sz});
            buf.advance(sz);

            if (sz == this->len) {
                return State::Size;
            }

            this->len -= MaxLen;
            return State::Strip;
        }

        State strip_data(Buf64k & buf)
        {
            const size_t sz = std::min<size_t>(buf.remaining(), this->len);
            this->len -= sz;
            buf.advance(sz);

            return this->len ? State::Strip : State::Size;
        }
    };

    template<class T>
    struct BasicResult
    {
        static BasicResult fail() noexcept
        {
            return BasicResult{};
        }

        static BasicResult ok(T value) noexcept
        {
            return BasicResult{value};
        }

        bool operator!() const noexcept
        {
            return !this->is_ok;
        }

        explicit operator bool () const noexcept
        {
            return this->is_ok;
        }

        operator T () const noexcept
        {
            return this->value;
        }

    private:
        BasicResult() noexcept = default;

        BasicResult(T value) noexcept
          : is_ok(true)
          , value(value)
        {}

        bool is_ok = false;
        T value;
    };

    class SecurityResult
    {
        enum class State
        {
            Header,
            ReasonFail,
            Finish,
        };

        using Result = BasicResult<State>;

    public:
        void restart()
        {
            this->state = State::Header;
        }

        template<class F> // f(bool status, u8_array_view raison_fail)
        bool run(Buf64k & buf, F && f)
        {
            switch (this->state)
            {
                case State::Header:
                    if (auto r = this->read_header(buf)) {
                        if (r == State::Finish) {
                            f(true, nullptr);
                            return true;
                        }
                        this->reason.restart();
                        this->state = r;
                    }
                    else {
                        return false;
                    }
                    [[fallthrough]];
                case State::ReasonFail:
                    return this->reason.run(buf, [&f](u8_array_view av){ f(false, av); });
                case State::Finish:
                    return true;
            }

            REDEMPTION_UNREACHABLE();
        }

    private:
        using ReasonCtx = MessageCtx<256>;

        State state = State::Header;
        ReasonCtx reason;

        static Result read_header(Buf64k & buf)
        {
            const size_t sz = 4;

            if (buf.remaining() < sz)
            {
                return Result::fail();
            }

            uint32_t const i = InStream(buf.av(sz)).in_uint32_be();

            buf.advance(sz);

            return Result::ok(i ? State::ReasonFail : State::Finish);
        }
    };
    SecurityResult auth_response_ctx;

    class MsLogonCtx {
        enum class State {
            Data,
            Finish,
        };

    public:
        void restart() noexcept
        {
            this->state = State::Data;
        }

        bool run(Buf64k & buf)
        {
            return State::Finish == this->read_data(buf);
        }

        uint64_t gen;
        uint64_t mod;
        uint64_t resp;

    private:
        State state = State::Data;

        State read_data(Buf64k & buf)
        {
            const size_t sz = 24;

            if (buf.remaining() < sz)
            {
                return State::Data;
            }

            InStream stream(buf.av(sz));
            this->gen = stream.in_uint64_be();
            this->mod = stream.in_uint64_be();
            this->resp = stream.in_uint64_be();

            buf.advance(sz);

            return State::Finish;
        }
    };
    MsLogonCtx ms_logon_ctx;

    bool ms_logon(Buf64k & buf);

    MessageCtx<8192> invalid_auth_ctx;



    // 7.3.2   ServerInit
    // ------------------
    // After receiving the ClientInit message, the server sends a
    // ServerInit message. This tells the client the width and
    // height of the server's framebuffer, its pixel format and the
    // name associated with the desktop:

    // framebuffer-width  : 2 bytes
    // framebuffer-height : 2 bytes

    // PIXEL_FORMAT       : 16 bytes
    // VNC pixel_format capabilities
    // -----------------------------
    // Server-pixel-format specifies the server's natural pixel
    // format. This pixel format will be used unless the client
    // requests a different format using the SetPixelFormat message
    // (SetPixelFormat).

    // PIXEL_FORMAT::bits per pixel  : 1 byte
    // PIXEL_FORMAT::color depth     : 1 byte

    // Bits-per-pixel is the number of bits used for each pixel
    // value on the wire. This must be greater than or equal to the
    // depth which is the number of useful bits in the pixel value.
    // Currently bits-per-pixel must be 8, 16 or 32. Less than 8-bit
    // pixels are not yet supported.

    // PIXEL_FORMAT::endianess       : 1 byte (0 = LE, 1 = BE)

    // Big-endian-flag is non-zero (true) if multi-byte pixels are
    // interpreted as big endian. Of course this is meaningless
    // for 8 bits-per-pixel.

    // PIXEL_FORMAT::true color flag : 1 byte
    // PIXEL_FORMAT::red max         : 2 bytes
    // PIXEL_FORMAT::green max       : 2 bytes
    // PIXEL_FORMAT::blue max        : 2 bytes
    // PIXEL_FORMAT::red shift       : 1 bytes
    // PIXEL_FORMAT::green shift     : 1 bytes
    // PIXEL_FORMAT::blue shift      : 1 bytes

    // If true-colour-flag is non-zero (true) then the last six
    // items specify how to extract the red, green and blue
    // intensities from the pixel value. Red-max is the maximum
    // red value (= 2^n - 1 where n is the number of bits used
    // for red). Note this value is always in big endian order.
    // Red-shift is the number of shifts needed to get the red
    // value in a pixel to the least significant bit. Green-max,
    // green-shift and blue-max, blue-shift are similar for green
    // and blue. For example, to find the red value (between 0 and
    // red-max) from a given pixel, do the following:

    // * Swap the pixel value according to big-endian-flag (e.g.
    // if big-endian-flag is zero (false) and host byte order is
    // big endian, then swap).
    // * Shift right by red-shift.
    // * AND with red-max (in host byte order).

    // If true-colour-flag is zero (false) then the server uses
    // pixel values which are not directly composed from the red,
    // green and blue intensities, but which serve as indices into
    // a colour map. Entries in the colour map are set by the
    // server using the SetColourMapEntries message
    // (SetColourMapEntries).

    // PIXEL_FORMAT::padding         : 3 bytes

    // name-length        : 4 bytes
    // name-string        : variable

    // The text encoding used for name-string is historically undefined but it is strongly recommended to use UTF-8 (see String Encodings for more details).

    // TODO not yet supported
    // If the Tight Security Type is activated, the server init
    // message is extended with an interaction capabilities section.


//    7.4.7   EnableContinuousUpdates

//    This message informs the server to switch between only sending FramebufferUpdate messages as a result of a
//    FramebufferUpdateRequest message, or sending FramebufferUpdate messages continuously.

//    Note that there is currently no way to determine if the server supports this message except for using the
//       Tight Security Type authentication.

//    No. of bytes       Type     [Value]     Description
//            1           U8       150       message-type
//            1           U8                 enable-flag
//            2           U16                x-position
//            2           U16                y-position
//            2           U16                width
//            2           U16                height

//    If enable-flag is non-zero, then the server can start sending FramebufferUpdate messages as needed for the area
// specified by x-position, y-position, width, and height. If continuous updates are already active, then they must
// remain active active and the coordinates must be replaced with the last message seen.

//    If enable-flag is zero, then the server must only send FramebufferUpdate messages as a result of receiving
// FramebufferUpdateRequest messages. The server must also immediately send out a EndOfContinuousUpdates message.
// This message must be sent out even if continuous updates were already disabled.

//    The server must ignore all incremental update requests (FramebufferUpdateRequest with incremental set to
// non-zero) as long as continuous updates are active. Non-incremental update requests must however be honored,
// even if the area in such a request does not overlap the area specified for continuous updates.

    class ServerInitCtx
    {
        enum class State
        {
            PixelFormat,
            EncodingName,
        };

    public:
        void restart()
        {
            this->state = State::PixelFormat;
        }

        bool run(Buf64k & buf, mod_vnc & vnc)
        {
            switch (this->state)
            {
            case State::PixelFormat:
                this->state = this->read_pixel_format(buf, vnc);
                if (this->state != State::EncodingName) {
                    break;
                }
                [[fallthrough]];
            case State::EncodingName:
                this->state = this->read_encoding_name(buf, vnc);
                if (this->state == State::PixelFormat) {
                    return true;
                }
            }
            return false;
        }

    private:
        State state = State::PixelFormat;
        uint32_t lg;

        State read_pixel_format(Buf64k & buf, mod_vnc & vnc)
        {
            const size_t sz = 24;

            if (buf.remaining() < sz)
            {
                return State::PixelFormat;
            }

            InStream stream(buf.av(sz));
            vnc.width = stream.in_uint16_be();
            vnc.height = stream.in_uint16_be();
            vnc.bpp    = safe_int(stream.in_uint8());
            vnc.depth  = stream.in_uint8();
            vnc.endianess = stream.in_uint8();
            vnc.true_color_flag = stream.in_uint8();
            vnc.red_max = stream.in_uint16_be();
            vnc.green_max = stream.in_uint16_be();
            vnc.blue_max = stream.in_uint16_be();
            vnc.red_shift = stream.in_uint8();
            vnc.green_shift = stream.in_uint8();
            vnc.blue_shift = stream.in_uint8();
            stream.in_skip_bytes(3); // skip padding

            // LOG(LOG_INFO, "VNC received: width=%d height=%d bpp=%d depth=%d endianess=%d true_color=%d red_max=%d green_max=%d blue_max=%d red_shift=%d green_shift=%d blue_shift=%d", this->width, this->height, this->bpp, this->depth, this->endianess, this->true_color_flag, this->red_max, this->green_max, this->blue_max, this->red_shift, this->green_shift, this->blue_shift);

            this->lg = stream.in_uint32_be();

            if (this->lg > sizeof(vnc.mod_name)-1) {
                LOG(LOG_ERR, "VNC connection error");
                throw Error(ERR_VNC_CONNECTION_ERROR);
            }

            buf.advance(sz);
            return State::EncodingName;
        }

        State read_encoding_name(Buf64k & buf, mod_vnc & vnc)
        {
            if (buf.remaining() < this->lg)
            {
                return State::EncodingName;
            }

            memcpy(vnc.mod_name, buf.av().data(), this->lg);
            vnc.mod_name[this->lg] = 0;

            buf.advance(this->lg);
            return State::PixelFormat;
        }
    };
    ServerInitCtx server_init_ctx;

    void initial_clear_screen();

    // TODO It may be possible to change several mouse buttons at once ? Current code seems to perform several send if that occurs. Is it what we want ?
    void rdp_input_mouse(uint16_t device_flags, uint16_t x, uint16_t y) override;
    void rdp_input_mouse_ex(uint16_t device_flags, uint16_t x, uint16_t y) override;
    void rdp_input_scancode(KbdFlags flags, Scancode scancode, uint32_t event_time, Keymap const& keymap) override;
    void rdp_input_unicode(KbdFlags flag, uint16_t unicode) override;

    void send_keyevents(KeymapSym::Keys keys);

public:
    void rdp_input_synchronize(KeyLocks locks) override;

private:
    void update_screen(Rect r, uint8_t incr);

public:
    void rdp_input_invalidate(Rect r) override;

private:

//   Encoding value |   Mnemonic     | Encoding Description
// -------------------------------------------------------------------------------------
//    0             |                | Raw Encoding                                    |
//    1             |                | CopyRect Encoding                               |
//    2             |                | RRE Encoding                                    |
//    4             |                | CoRRE Encoding                                  |
//    5             |                | Hextile Encoding                                |
//    6             |                | zlib Encoding                                   |
//    7             |                | Tight Encoding                                  |
//    8             |                | zlibhex Encoding                                |
//    16            |                | ZRLE Encoding                                   |
//    -23 to -32    |                | JPEG Quality Level Pseudo-encoding              |
//    -223          |                | DesktopSize Pseudo-encoding                     |
//    -224          |                | LastRect Pseudo-encoding                        |
//    -239          |                | Cursor Pseudo-encoding                          |
//    -240          |                | X Cursor Pseudo-encoding                        |
//    -247 to -256  |                | Compression Level Pseudo-encoding               |
//    -257          |                | QEMU Pointer Motion Change Pseudo-encoding      |
//    -258          |                | QEMU Extended Key Event Pseudo-encoding         |
//    -259          |                | QEMU Audio Pseudo-encoding                      |
//    -261          |                | LED State Pseudo-encoding                       |
//    -305          |                | gii Pseudo-encoding                             |
//    -307          |                | DesktopName Pseudo-encoding                     |
//    -308          |                | ExtendedDesktopSize Pseudo-encoding             |
//    -309          |                | xvp Pseudo-encoding                             |
//    -312          |                | Fence Pseudo-encoding                           |
//    -313          |                | ContinuousUpdates Pseudo-encoding               |
//    -314          |                | Cursor With Alpha Pseudo-encoding               |
//    -412 to -512  |                | JPEG Fine-Grained Quality Level Pseudo-encoding |
//    -763 to -768  |                | JPEG Subsampling Level Pseudo-encoding          |
//    0xc0a1e5ce    |                | Extended Clipboard Pseudo-encoding              |

    enum rfb_encodings {
        RAW_ENCODING      = 0,
        COPYRECT_ENCODING = 1,
        RRE_ENCODING      = 2,
        CORRE_ENCODING    = 4,
        HEXTILE_ENCODING  = 5,
        ZLIB_ENCODING     = 6,
        TIGHT_ENCODING    = 7,
        ZLIBHEX_ENCODING  = 8,
        ZRLE_ENCODING     = 16,
        JPEGQL1_PSEUDO_ENCODING      = -23,
        JPEGQL2_PSEUDO_ENCODING      = -24,
        JPEGQL3_PSEUDO_ENCODING      = -25,
        JPEGQL4_PSEUDO_ENCODING      = -26,
        JPEGQL5_PSEUDO_ENCODING      = -27,
        JPEGQL6_PSEUDO_ENCODING      = -28,
        JPEGQL7_PSEUDO_ENCODING      = -29,
        JPEGQL8_PSEUDO_ENCODING      = -30,
        JPEGQL9_PSEUDO_ENCODING      = -21,
        JPEGQLA_PSEUDO_ENCODING      = -32,
        DESKTOPSIZE_PSEUDO_ENCODING  = -223,
        LASTRECT_PSEUDO_ENCODING     = -224,
        // Required by UltraVNC (< 1.7.4) for CURSOR_PSEUDO_ENCODING...
        // Excepted with "Force cursor shape" option (>= 1.7.4)
        POINTER_POSITION_ENCODING    = -232, // 0xFFFFFF18
        CURSOR_PSEUDO_ENCODING       = -239,
        XCURSOR_PSEUDO_ENCODING      = -240,
        UVNC_FILE_TRANSFER           = UVNCFileTransferReader::encoding_value,
    };

    // VNC Client to Server Messages
    enum VNC_client_to_server_messages {
        VNC_CS_MSG_SET_PIXEL_FORMAT              = 0,
        VNC_CS_MSG_SET_ENCODINGS                 = 2,
        VNC_CS_MSG_FRAME_BUFFER_UPDATE_REQUEST   = 3,
        VNC_CS_MSG_KEY_EVENT                     = 4,
        VNC_CS_MSG_POINTER_EVENT                 = 5,
        VNC_CS_MSG_CLIENT_CUT_TEXT               = 6,
        // Optional Messages
        VNC_CS_MSG_FILE_TRANSFER                  = 7,
        VNC_CS_MSG_SET_SCALE                      = 8,
        VNC_CS_MSG_SET_SERVER_INPUT               = 9,
        VNC_CS_MSG_SET_SW                         = 10,
        VNC_CS_MSG_TEXT_CHAT                      = 11,
        VNC_CS_MSG_KEY_FRAME_REQUEST              = 12,
        VNC_CS_MSG_KEEP_ALIVE                     = 13,
        VNC_CS_MSG_ULTRA_VNC_RESERVED1            = 14,
        VNC_CS_MSG_SET_SCALE_FACTOR               = 15,
        VNC_CS_MSG_ULTRA_VNC_RESERVED2            = 16,
        VNC_CS_MSG_ULTRA_VNC_RESERVED3            = 17,
        VNC_CS_MSG_ULTRA_VNC_RESERVED4            = 18,
        VNC_CS_MSG_ULTRA_VNC_RESERVED5            = 19,
        VNC_CS_MSG_REQUEST_SESSION                = 20,
        VNC_CS_MSG_SET_SESSION                    = 21,
        VNC_CS_MSG_NOTIFY_PLUGIN_STREAMING        = 80,
        VNC_CS_MSG_VMWARE1                        = 127,
        VNC_CS_MSG_CAR_CONNECTIVITY               = 128,
        VNC_CS_MSG_ENABLE_CONTINUOUS_UPDATE       = 150,
        VNC_CS_MSG_CLIENT_FENCE                   = 248,
        VNC_CS_MSG_OLIVE_CALL_CONTROL             = 249,
        VNC_CS_MSG_XVP_CLIENT_MESSAGE             = 250,
        VNC_CS_MSG_SET_DESKTOP_SIZE               = 251,
        VNC_CS_MSG_TIGHT                          = 252,
        VNC_CS_MSG_GII_CLIENT_MESSAGE             = 253,
        VNC_CS_MSG_VMWARE2                        = 254,
        VNC_CS_MSG_QEMU_CLIENT_MESSAGE            = 255,
    };

    // https://github.com/rfbproto/rfbproto/blob/master/rfbproto.rst#75server-to-client-messages
    enum class ServerToClientMessage : uint8_t
    {
        // must support
        FramebufferUpdate = 0,
        SetColourMapEntries = 1,
        Bell = 2,
        ServerCutText = 3,

        // Optional message
        FileTransfer = UVNCFileTransferReader::message_type,
    };

    class PasswordCtx
    {
        enum class State
        {
            RandomKey,
            Finish,
        };

    public:
        writable_u8_array_view server_random;

        void restart() noexcept
        {
            this->state = State::RandomKey;
        }

        bool run(Buf64k & buf) noexcept
        {
            return State::Finish == this->read_random_number(buf);
        }

    private:
        State state = State::RandomKey;

        State read_random_number(Buf64k & buf) noexcept
        {
            const size_t sz = 16;

            if (buf.remaining() < sz)
            {
                return State::RandomKey;
            }

            this->server_random = buf.av(sz);

            buf.advance(sz);
            return State::Finish;
        }
    };

    PasswordCtx password_ctx;
    struct UpAndRunningCtx
    {
        enum class State : uint8_t
        {
            Header,
            FrameBufferUpdate,
            Palette,
            ServerCutText,
            FileTransfer,
        };

        void restart() noexcept
        {
            this->state = State::Header;
        }

        bool run(Buf64k & buf, gdi::GraphicApi & drawable, mod_vnc & vnc)
        {
            switch (this->state)
            {
            case State::Header:
                if (buf.remaining() < 1) {
                    return false;
                }

                this->message_type = safe_int(buf.av()[0]);

                buf.advance(1);

                REDEMPTION_DIAGNOSTIC_PUSH()
                REDEMPTION_DIAGNOSTIC_CLANG_IGNORE("-Wcovered-switch-default")
                switch (this->message_type)
                {
                case ServerToClientMessage::FramebufferUpdate: /* framebuffer update */
                    vnc.frame_buffer_update_ctx.start(vnc.bpp, to_bytes_per_pixel(vnc.bpp));
                    this->state = State::FrameBufferUpdate;
                    return vnc.lib_frame_buffer_update(buf);

                case ServerToClientMessage::SetColourMapEntries:
                    vnc.palette_update_ctx.start();
                    this->state = State::Palette;
                    return vnc.lib_palette_update(drawable, buf);

                case ServerToClientMessage::Bell:
                    return true;

                case ServerToClientMessage::ServerCutText:
                    this->state = State::ServerCutText;
                    return vnc.lib_clip_data(buf);

                case ServerToClientMessage::FileTransfer:
                    this->state = State::FileTransfer;
                    return vnc.consume_file_transfer_packet(buf);

                default:
                    LOG(LOG_ERR, "unknown message type in vnc %u", message_type);
                    throw Error(ERR_VNC);
                }
                REDEMPTION_DIAGNOSTIC_POP()
                break;

            case State::FrameBufferUpdate: return vnc.lib_frame_buffer_update(buf);
            case State::Palette:           return vnc.lib_palette_update(drawable, buf);
            case State::ServerCutText:     return vnc.lib_clip_data(buf);
            case State::FileTransfer:      return vnc.consume_file_transfer_packet(buf);
            }

            return false;
        }

    private:
        State state = State::Header;
        ServerToClientMessage message_type;
    };

    bool doTlsSwitch();

    UpAndRunningCtx up_and_running_ctx;

    VncBuf64k server_data_buf;
    int spokenProtocol;
    bool tlsSwitch;

public:
    void draw_event();

private:
    static const char *securityTypeString(int32_t t);

    static void updatePreferedAuth(int32_t authId, VncAuthType &preferedAuth, size_t &preferedAuthIndex);

    bool readSecurityResult(InStream &s, uint32_t &status, bool &haveReason, std::string &reason, size_t &skipLen) const;

    bool treatVeNCrypt();

    bool draw_event_impl();

private:
    struct FrameBufferUpdateCtx
    {
        enum class State
        {
            Header,
            Encoding,
            Data,
        };

        using Result = BasicResult<State>;

        VNC::Encoder::EncoderState last = VNC::Encoder::EncoderState::Ready;

        FrameBufferUpdateCtx(Zdecompressor<> & zd, VNCVerbose verbose)
          : zd{zd}
          , verbose(verbose)
        {
        }

        void start(BitsPerPixel bpp, BytesPerPixel Bpp)
        {
            this->bpp = bpp;
            this->Bpp = Bpp;
            this->state = State::Header;
        }


//  7.5.1   FramebufferUpdate (part 2 : rectangles)
// ----------------------------------

//  FrameBufferUpdate message is followed by number-of-rectangles
// of pixel data. Each rectangle consists of:

//     No. of bytes | Type   |  Description
// ---------------------------------------------
//         2        |  U16   |  x-position
//         2        |  U16   |  y-position
//         2        |  U16   |  width
//         2        |  U16   |  height
//         4        |  S32   |  encoding-type

//  followed by the pixel data in the specified encoding. See Encodings for the format of the data for each encoding
// and Pseudo-encodings for the meaning of pseudo-encodings.

// Note that a framebuffer update marks a transition from one valid framebuffer state to another. That
// means that a single update handles all received FramebufferUpdateRequest up to the point where th
// e update is sent out.

// However, because there is no strong connection between a FramebufferUpdateRequest and a subsequent
// FramebufferUpdate, a client that has more than one FramebufferUpdateRequest pending at any given
// time cannot be sure that it has received all framebuffer updates.

        size_t last_avail = 0;

        bool run(Buf64k & buf, mod_vnc & vnc)
        {
            Result r = Result::fail();

            for (;;) {
                switch (this->state)
                {
                case State::Header:
                {
                    const size_t sz = 3;

                    if (buf.remaining() < sz)
                    {
                        r = Result::fail();
                        break;
                    }

                    InStream stream(buf.av(sz));
                    stream.in_skip_bytes(1);
                    this->num_recs = stream.in_uint16_be();

                    buf.advance(sz);
                    r = Result::ok(State::Encoding);
                }
                break;
                case State::Encoding:
                {
                    if (0 == this->num_recs) {
                        this->state = State::Header;
                        return true;
                    }
                    const size_t sz = 12;

                    if (buf.remaining() < sz)
                    {
                        r = Result::fail();
                    }
                    else {
                        InStream stream(buf.av(sz));
                        this->x = stream.in_uint16_be();
                        this->y = stream.in_uint16_be();
                        this->cx = stream.in_uint16_be();
                        this->cy = stream.in_uint16_be();
                        this->encoding = stream.in_sint32_be();

                        LOG_IF(bool(this->verbose & VNCVerbose::basic_trace), LOG_INFO,
                            "Encoding: %u (%u, %u, %u, %u) : %d",
                            this->num_recs, this->x, this->y, this->cx, this->cy, this->encoding);

                        --this->num_recs;

                        LOG_IF(bool(this->verbose & VNCVerbose::basic_trace), LOG_INFO,
                            "%s %d (%d, %d, %d, %d)",
                            (this->encoding == HEXTILE_ENCODING)
                            ? "HEXTILE_ENCODING"
                            : (this->encoding == CURSOR_PSEUDO_ENCODING)
                            ? "CURSOR_PSEUDO_ENCODING"
                            : (this->encoding == COPYRECT_ENCODING)
                            ? "COPYRECT_ENCODING"
                            : (this->encoding == RRE_ENCODING)
                            ? "RRE_ENCODING"
                            : (this->encoding == RAW_ENCODING)
                            ? "RAW_ENCODING"
                            : (this->encoding == ZRLE_ENCODING)
                            ? "ZRLE_ENCODING"
                            : (this->encoding == POINTER_POSITION_ENCODING)
                            ? "POINTER_POSITION_ENCODING"
                            : "UNKNOWN_ENCODING",
                            this->encoding , this->x, this->y, this->cx, this->cy);

                        switch (this->encoding){
                        case COPYRECT_ENCODING:  /* raw */
                            this->encoder = VNC::Encoder::copy_rect_encoder(
                                Rect(this->x, this->y, this->cx, this->cy),
                                vnc.width, vnc.height);
                            break;
                        case HEXTILE_ENCODING:  /* hextile */
                            this->encoder = VNC::Encoder::hextile_encoder(
                                this->bpp, this->Bpp, Rect(this->x, this->y, this->cx, this->cy),
                                vnc.verbose);
                            break;
                        case CURSOR_PSEUDO_ENCODING:  /* cursor */
                            this->encoder = VNC::Encoder::cursor_encoder(
                                this->Bpp, Rect(this->x, this->y, this->cx, this->cy),
                                vnc.red_shift, vnc.red_max, vnc.green_shift, vnc.green_max,
                                vnc.blue_shift, vnc.blue_max, vnc.has_cursor, vnc.verbose);
                            break;
                        case RAW_ENCODING:  /* raw */
                            this->encoder = VNC::Encoder::raw_encoder(
                                this->bpp, this->Bpp, Rect(this->x, this->y, this->cx, this->cy));
                            break;
                        case ZRLE_ENCODING: /* ZRLE */
                            this->encoder = VNC::Encoder::zrle_encoder(
                                this->bpp, this->Bpp, Rect(this->x, this->y, this->cx, this->cy),
                                this->zd, vnc.verbose);
                            break;
                        case RRE_ENCODING: /* RRE */
                            this->encoder = VNC::Encoder::rre_encoder(
                                this->bpp, this->Bpp, Rect(this->x, this->y, this->cx, this->cy));
                            break;
                        case POINTER_POSITION_ENCODING:
                            // ignore position sent by server
                            // no data
                            buf.advance(sz);
                            r = Result::ok(State::Header);
                            continue;
                        default:
                            LOG(LOG_ERR, "unexpected VNC encoding %d", encoding);
                            throw Error(ERR_VNC_UNEXPECTED_ENCODING_IN_LIB_FRAME_BUFFER);
                        }
                        buf.advance(sz);
                        // Note: it is important to immediately call State::Data as in some cases there won't be
                        // any trailing data to expect.
                        this->last = VNC::Encoder::EncoderState::Ready;
                        r = Result::ok(State::Data);
                    }
                }
                break;
                case State::Data:
                    {
                        if (this->last == VNC::Encoder::EncoderState::NeedMoreData){
                            // No data. This can happen when TLS is enabled
                            if (this->last_avail == buf.remaining()){
                                return false;
                            }
                        }

                        if (!bool(this->encoder)){
                            LOG(LOG_ERR, "Call to vnc::mod with null encoder");
                            throw Error(ERR_VNC);
                        }

                        // Pre Assertion: we have an encoder
                        switch (encoder(buf, *vnc.gd)){
                            case VNC::Encoder::EncoderState::Ready:
                                r = Result::ok(State::Data);
                                this->last = VNC::Encoder::EncoderState::Ready;
                            break;
                            case VNC::Encoder::EncoderState::NeedMoreData:
                                r = Result::fail();
                                this->last_avail = buf.remaining();
                                this->last = VNC::Encoder::EncoderState::NeedMoreData;
                            break;
                            case VNC::Encoder::EncoderState::Exit:
                                // consume returns true if encoder is finished (ready to be resetted)
                                r = Result::ok(State::Encoding);
                                encoder = nullptr;
                                this->last = VNC::Encoder::EncoderState::NeedMoreData;
                                break;
                        }
                    }
                }

                if (!r) {
                    return false;
                }
                this->state = r;
            }

            return true;
        }

    private:
        BitsPerPixel bpp = BitsPerPixel::BitsPP32;
        BytesPerPixel Bpp = BytesPerPixel(4);

        State state = State::Header;

        uint16_t num_recs = 0;

        uint16_t x = 0;
        uint16_t y = 0;
        uint16_t cx = 0;
        uint16_t cy = 0;
        int32_t encoding = 0;

        VNC::Encoder::Encoder encoder;

        Zdecompressor<> & zd;

        VNCVerbose verbose;
    };
    FrameBufferUpdateCtx frame_buffer_update_ctx;

    bool lib_frame_buffer_update(Buf64k & buf)
    {
        const bool ok = this->frame_buffer_update_ctx.run(buf, *this);
        if (!ok) {
            return false;
        }

        this->update_screen(Rect(0, 0, this->width, this->height), 1);
        return true;
    }

    class PaletteUpdateCtx
    {
        enum class State
        {
            Header,
            Data,
            SkipData,
        };

        using Result = BasicResult<State>;

    public:
        void start()
        {
            this->state = State::Header;
            this->num_colors = 1; // sentinel
        }

        bool run(Buf64k & buf) noexcept
        {
            for (;;) {
                Result r = [this, &buf]{
                    switch (this->state) {
                        case State::Header:   return this->read_header(buf);
                        case State::Data:     return this->read_data(buf);
                        case State::SkipData: return this->skip_data(buf);
                    }
                    REDEMPTION_UNREACHABLE();
                }();

                if (!r) {
                    return false;
                }
                if (0 == this->num_colors) {
                    return true;
                }
                this->state = r;
            }
        }

        [[nodiscard]] BGRPalette const & get_palette() const noexcept
        {
            return this->palette;
        }

    private:
        State state;

        uint16_t first_color;
        uint16_t num_colors;

        BGRPalette palette = BGRPalette::classic_332();

        Result read_header(Buf64k & buf) noexcept
        {
            if (buf.remaining() < 5)
            {
                return Result::fail();
            }

            InStream stream(buf.av(5));
            stream.in_skip_bytes(1);
            this->first_color = stream.in_uint16_be();
            this->num_colors = stream.in_uint16_be();

            buf.advance(5);

            if (this->first_color + this->num_colors > 256) {
                LOG(LOG_ERR, "VNC: number of palette colors too large: %d",
                    this->num_colors);
                return Result::ok(State::SkipData);
            }

            return Result::ok(State::Data);
        }

        Result read_data(Buf64k & buf) noexcept
        {
            if (buf.remaining() < 6)
            {
                return Result::fail();
            }

            InStream stream(buf.av());
            uint16_t const n = std::min<uint16_t>(
                stream.get_capacity() / 6,
                this->num_colors
            );
            this->num_colors -= n;

            uint16_t const max = n + this->first_color;
            for (; this->first_color < max; ++this->first_color) {
                const int b = stream.in_uint16_be() >> 8;
                const int g = stream.in_uint16_be() >> 8;
                const int r = stream.in_uint16_be() >> 8;
                this->palette.set_color(this->first_color, BGRColor(b, g, r));
            }

            buf.advance(n * 6);

            return Result::ok(State::Data);
        }

        Result skip_data(Buf64k & buf) noexcept
        {
            auto const n = std::min(buf.remaining(), uint16_t(this->num_colors * 6));
            this->num_colors -= n / 6;
            buf.advance(n);
            return Result::ok(State::SkipData);
        }
    };
    PaletteUpdateCtx palette_update_ctx;

    bool lib_palette_update(gdi::GraphicApi & drawable, Buf64k & buf)
    {
        if (!this->palette_update_ctx.run(buf)) {
            return false;
        }

        drawable.set_palette(this->palette_update_ctx.get_palette());
        drawable.draw(RDPColCache(0, this->palette_update_ctx.get_palette()));

        return true;
    } // lib_palette_update

    [[nodiscard]]
    const CHANNELS::ChannelDef * get_channel_by_name(CHANNELS::ChannelNameId channel_name) const
    {
        return this->front.get_channel_list().get_by_name(channel_name);
    }

    Rfb::CutTextReader server_cut_text_reader;

    //******************************************************************************
    // Entry point for VNC server clipboard content reception
    // Conversion to RDP behaviour :
    //  - store this content in a buffer, waiting for an explicit request from the front
    //  - send a notification to the front (Format List PDU) that the server clipboard
    //    status has changed
    //******************************************************************************
    bool lib_clip_data(Buf64k & buf);

    bool consume_file_transfer_packet(Buf64k& buf);

public:
    void send_to_mod_channel(CHANNELS::ChannelNameId front_channel_name, InStream & chunk, size_t total_length, uint32_t flags) override;

    // Front calls this member function when it became up and running.
    void rdp_gdi_up_and_running() override;
    void rdp_gdi_down() override {}

    [[nodiscard]] bool is_up_and_running() const override {
        return (UP_AND_RUNNING == this->state);
    }

    bool server_error_encountered() const override { return false; }

    void disconnect() override;

    [[nodiscard]] Dimension get_dim() const override
    { return Dimension(this->width, this->height); }

    void acl_update(AclFieldMask const&/* acl_fields*/) override {}

    void file_validator_receive_event();


private:
    void send_to_cliprdr(bytes_view chunk, size_t total_length, VNC::ChannelFlags flags);
    void send_to_cliprdr(bytes_view pdu);
};

