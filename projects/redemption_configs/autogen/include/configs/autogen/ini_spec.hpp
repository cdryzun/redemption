#include "config_variant.hpp"

R"gen_config_ini(## Python spec file for RDP proxy.


[globals]

# Port of RDP Proxy service.<br/>
# Changing the port number will restart the service and disconnect active sessions. It will also block WALLIX Access Manager connections unless its RDP port is configured.
# Choose a port that is not already in use. Otherwise, the service will not run.
#_iptables
#_advanced
#_logged
port = integer(min=0, default=3389)

# Timeout during RDP connection initialization.
# Increase this value if the connection between workstations and Bastion is slow.<br/>
# (in seconds)
handshake_timeout = integer(min=0, default=10)

# No automatic disconnection due to inactivity, timer is set on primary authentication.
# If the value is between 1 and 30, then 30 is used.
# If the value is set to 0, then inactivity timeout value is unlimited.<br/>
# (in seconds)
base_inactivity_timeout = integer(min=0, default=900)

# Specifies how long the RDP proxy login screen should be displayed before the client window closes (use 0 to deactivate).<br/>
# (in seconds)
#_advanced
authentication_timeout = integer(min=0, default=120)

# Transparent mode allows network traffic interception for a target, even when users directly specify a target's address instead of a proxy address.
#_iptables
enable_transparent_mode = boolean(default=False)

# Displays a reminder box at the top of the session when a session has a time limit (timeframe or approval).
# Reminders appear at 30 minutes, 10 minutes, 5 minutes, and 1 minute before the session ends.
#_advanced
#_display_name=Enable end time warning OSD
enable_end_time_warning_osd = boolean(default=True)

# Allows showing the target device name with F12 during the session.
#_advanced
#_display_name=Enable OSD display remote target
enable_osd_display_remote_target = boolean(default=True)

# Displays the target username in the session when F12 is pressed.
# This option needs "Enable OSD display remote target" option.
show_target_user_in_f12_message = boolean(default=False)

# Prevent Remote Desktop session timeouts due to idle TCP sessions by periodically sending keepalive packets to the client.
# Set to 0 to disable this feature.<br/>
# (in milliseconds)
#_display_name=RDP keepalive connection interval
rdp_keepalive_connection_interval = integer(min=0, default=0)

# ⚠ Manually restart service redemption to take changes into account.<br/>
# Enable primary connection on IPv6.
#_advanced
#_display_name=Enable IPv6
enable_ipv6 = boolean(default=True)

[client]

# If true, ignore the password provided by the RDP client. The user needs to manually log in.
#_advanced
ignore_logon_password = boolean(default=False)

# Sends the client screen count to the target server. Not supported for VNC targets.
# Uncheck to disable multiple monitors.
allow_using_multiple_monitors = boolean(default=True)

# Sends Scale &amp; Layout configuration to the target server.
# On Windows 11, this corresponds to the options "Scale", "Display Resolution" and "Display Orientation" of Settings > System > Display.
# ⚠ Title bar detection via OCR will no longer work.
allow_scale_factor = boolean(default=False)

# Fallback to RDP Legacy Encryption if the client does not support TLS.
# ⚠ Enabling this option is a security risk.
#_display_name=TLS fallback legacy
tls_fallback_legacy = boolean(default=False)

# Minimal incoming TLS level: 0=TLSv1, 1=TLSv1.1, 2=TLSv1.2, 3=TLSv1.3
# ⚠ Lower this value only for compatibility reasons.
#_display_name=TLS min level
tls_min_level = integer(min=0, default=2)

# Maximal incoming TLS level: 0=no restriction, 1=TLSv1.1, 2=TLSv1.2, 3=TLSv1.3
# ⚠ Change this value only for compatibility reasons.
#_display_name=TLS max level
tls_max_level = integer(min=0, default=0)

# [Not configured]: Compatible with more RDP clients (less secure)
# HIGH:!ADH:!3DES: Compatible only with MS Windows 7 client or more recent (moderately secure)
# HIGH:!ADH:!3DES:!SHA: Compatible only with MS Server Windows 2008 R2 client or more recent (more secure)
# The format used is described on this page: https://www.openssl.org/docs/man3.1/man1/openssl-ciphers.html#CIPHER-LIST-FORMAT
#_display_name=SSL cipher list
ssl_cipher_list = string(default="HIGH:!ADH:!3DES:!SHA")

# Configure the available TLSv1.3 ciphersuites.
# Empty to apply system-wide configuration.
# The format used is described in the third paragraph of this page: https://www.openssl.org/docs/man1.1.1/man3/SSL_CTX_set_ciphersuites.html#DESCRIPTION
#_display_name=TLS 1.3 cipher suites
tls_1_3_ciphersuites = string(default="")

# Configure the supported key exchange groups.
# Empty to apply system-wide configuration.
# The format used is described in this page: https://www.openssl.org/docs/man3.2/man3/SSL_CONF_cmd.html#groups-groups
#_display_name=TLS key exchange groups
tls_key_exchange_groups = string(default="")

# Configure the supported server signature algorithms.
# Empty to apply system-wide configuration.
# The format should be a colon separated list of signature algorithms in order of decreasing preference of the form algorithm+hash or signature_scheme.
# algorithm is one of RSA, RSA-PSS or ECDSA.
# hash is one of SHA224, SHA256, SHA384 or SHA512.
# signature_scheme is one of the signature schemes defined in TLSv1.3 (rfc8446#section-4.2.3), specified using the IETF name, e.g., ecdsa_secp384r1_sha384 or rsa_pss_rsae_sha256.
# This list needs at least one signature algorithm compatible with the RDP Proxy certificate.
#_display_name=TLS signature algorithms
tls_signature_algorithms = string(default="RSA+SHA256:RSA+SHA384:RSA+SHA512:RSA-PSS+SHA256:RSA-PSS+SHA384:RSA-PSS+SHA512:ECDSA+SHA256:ECDSA+SHA384:ECDSA+SHA512")

# Show the common cipher list supported by client and target server in the logs.
# ⚠ Only for debugging purposes.
#_advanced
show_common_cipher_list = boolean(default=False)

# Specifies the highest RDP compression support available on the client connection session.
# &nbsp; &nbsp;   0: The RDP bulk compression is disabled
# &nbsp; &nbsp;   1: RDP 4.0 bulk compression
# &nbsp; &nbsp;   2: RDP 5.0 bulk compression
# &nbsp; &nbsp;   3: RDP 6.0 bulk compression
# &nbsp; &nbsp;   4: RDP 6.1 bulk compression
#_advanced
#_display_name=RDP compression
rdp_compression = option(0, 1, 2, 3, 4, default=4)

# Specifies the maximum color depth for the client connection session:
# &nbsp; &nbsp;   8: 8-bit
# &nbsp; &nbsp;   15: 15-bit 555 RGB mask
# &nbsp; &nbsp;   16: 16-bit 565 RGB mask
# &nbsp; &nbsp;   24: 24-bit RGB mask
# &nbsp; &nbsp;   32: 32-bit RGB mask + alpha
#_advanced
max_color_depth = option(8, 15, 16, 24, 32, default=24)

# Persistent Disk Bitmap Cache on the primary connection side. If the RDP client supports it, this setting increases the size of image caches.
#_advanced
persistent_disk_bitmap_cache = boolean(default=True)

# If enabled, the content of Persistent Bitmap Caches are stored on the disk to reuse later (this value is ignored if "Persistent disk bitmap cache" option is disabled).
#_advanced
persist_bitmap_cache_on_disk = boolean(default=False)

# Enable Bitmap Compression when supported by the RDP client.
# Disabling this option increases the network bandwith usage.
#_advanced
bitmap_compression = boolean(default=True)

# Allows the client to request the target server to stop graphical updates. For example, when the RDP client window is minimized to reduce bandwidth.
# ⚠ If changes occur on the target, they will not be visible in the recordings either.
enable_suppress_output = boolean(default=True)

# Same effect as "Transform glyph to bitmap" option, but only for RDP clients on an iOS platform.
#_display_name=Bogus iOS glyph support level
bogus_ios_glyph_support_level = boolean(default=True)

# This option converts glyphs to bitmaps to resolve issues with certain RDP clients.
#_advanced
transform_glyph_to_bitmap = boolean(default=False)

# Informs users with a message when their session is audited.
#_display_name=Enable OSD 4 eyes
enable_osd_4_eyes = boolean(default=True)

# Enable RemoteFX on the client connection.
# Needs - "Max color depth" option set to 32 (32-bit RGB mask + alpha)
# &nbsp; &nbsp;       - "Enable RemoteFX" option (in "rdp" section of "Connection Policy" configuration) set to on
#_advanced
#_display_name=Enable RemoteFX
enable_remotefx = boolean(default=True)

# This option should only be used if the target server or client is showing graphical issues.
# In general, disabling RDP orders has a negative impact on performance.<br/>
# Drawing orders that can be disabled:
# &nbsp; &nbsp;    0: DstBlt
# &nbsp; &nbsp;    1: PatBlt
# &nbsp; &nbsp;    2: ScrBlt
# &nbsp; &nbsp;    3: MemBlt
# &nbsp; &nbsp;    4: Mem3Blt
# &nbsp; &nbsp;    9: LineTo
# &nbsp; &nbsp;   15: MultiDstBlt
# &nbsp; &nbsp;   16: MultiPatBlt
# &nbsp; &nbsp;   17: MultiScrBlt
# &nbsp; &nbsp;   18: MultiOpaqueRect
# &nbsp; &nbsp;   22: Polyline
# &nbsp; &nbsp;   25: EllipseSC
# &nbsp; &nbsp;   27: GlyphIndex<br/>
# (values are comma-separated)
#_advanced
disabled_orders = string(default="25")

# This option fixes a bug in the Remote Desktop client on Windows 11/Windows Server 2025 starting with version 24H2.
# Occasionally, some screen areas may not refresh due to a bug where only the first image in a BitmapUpdate message with multiple images is displayed correctly.
# Enabling this option prevents this issue, but will slightly increase the data sent to the client.
# The option is automatically disabled if the connection is from an Access Manager.
#_advanced
workaround_incomplete_images = boolean(default=False)

[all_target_mod]

# The maximum wait time for the proxy to connect to a target.<br/>
# (in milliseconds)
#_advanced
connection_establishment_timeout = integer(min=1000, max=10000, default=3000)

[remote_program]

# Allows resizing of a desktop session opened in a RemoteApp window.
# This happens when an RDP client opened in RemoteApp accesses a desktop target.
allow_resize_hosted_desktop = boolean(default=True)

[mod_rdp]

# It specifies a list of (comma-separated) RDP server desktop features to enable or disable in the session (with the goal of optimizing bandwidth usage).<br/>
# If a feature is preceded by a "-" sign, it is disabled; if it is preceded by a "+" sign or no sign, it is enabled. Unconfigured features can be controlled by the RDP client.<br/>
# Available features:
# &nbsp; &nbsp;   - wallpaper
# &nbsp; &nbsp;   - full_window_drag
# &nbsp; &nbsp;   - menu_animations
# &nbsp; &nbsp;   - theme
# &nbsp; &nbsp;   - mouse_cursor_shadows
# &nbsp; &nbsp;   - cursor_blinking
# &nbsp; &nbsp;   - font_smoothing
# &nbsp; &nbsp;   - desktop_composition
#_advanced
force_performance_flags = string(default="-mouse_cursor_shadows,-theme,-menu_animations")

# If enabled, the font smoothing desktop feature is automatically disabled in recorded session.
# This allows OCR (when session probe is disabled) to better detect window titles.
# If disabled, it allows font smoothing in recorded sessions. However, OCR will not work when session recording is disabled. In this case, window titles are not detected.
#_advanced
auto_adjust_performance_flags = boolean(default=True)

# Specifies the highest RDP compression support available on the target server connection.
# &nbsp; &nbsp;   0: The RDP bulk compression is disabled
# &nbsp; &nbsp;   1: RDP 4.0 bulk compression
# &nbsp; &nbsp;   2: RDP 5.0 bulk compression
# &nbsp; &nbsp;   3: RDP 6.0 bulk compression
# &nbsp; &nbsp;   4: RDP 6.1 bulk compression
#_advanced
#_display_name=RDP compression
rdp_compression = option(0, 1, 2, 3, 4, default=4)

#_advanced
disconnect_on_logon_user_change = boolean(default=False)

# The maximum wait time for the proxy to log on to an RDP session.
# Value 0 is equivalent to 15 seconds.<br/>
# (in seconds)
#_advanced
open_session_timeout = integer(min=0, default=0)

# Persistent Disk Bitmap Cache on the secondary connection side. If supported by the RDP target server, the size of image caches will be increased.
#_advanced
persistent_disk_bitmap_cache = boolean(default=True)

# Support of Cache Waiting List (this value is ignored if the "Persistent disk bitmap cache" option is disabled).
#_advanced
cache_waiting_list = boolean(default=True)

# If enabled, the contents of Persistent Bitmap Caches are stored on disk for reusing them later (this value is ignored if "Persistent disk bitmap cache" option is disabled).
#_advanced
persist_bitmap_cache_on_disk = boolean(default=False)

# Client Address to send to target (in InfoPacket).
# &nbsp; &nbsp;   0: Send 0.0.0.0
# &nbsp; &nbsp;   1: Send proxy client address or target connexion.
# &nbsp; &nbsp;   2: Send user client address of front connexion.
#_advanced
client_address_sent = option(0, 1, 2, default=0)

# Authentication channel used by Auto IT scripts. May be '*' to use default name. Keep empty to disable virtual channel.
auth_channel = string(max=7, default="*")

# Authentication channel used by other scripts. No default name. Keep empty to disable virtual channel.
checkout_channel = string(max=7, default="")

# Do not transmit the client machine name to the RDP server.
# If Per-Device licensing mode is configured on the RD host, this Bastion will consume a CAL for all of these connections to the RD host.
hide_client_name = boolean(default=False)

# Stores CALs issued by the terminal servers.
#_advanced
use_license_store = boolean(default=True)

# Workaround option to support partial clipboard initialization performed by some versions of FreeRDP.
#_advanced
#_display_name=Bogus FreeRDP clipboard
bogus_freerdp_clipboard = boolean(default=False)

# Workaround option to disable shared disk for RDP client on iOS platform only.
#_advanced
#_display_name=Bogus iOS RDPDR virtual channel
bogus_ios_rdpdr_virtual_channel = boolean(default=True)

# Workaround option to fix some drawing issues with Windows Server 2012.
# Can be disabled when none of the targets are Windows Server 2012.
#_advanced
bogus_refresh_rect = boolean(default=True)

# Delay before automatically bypassing Windows's Legal Notice screen in RemoteApp mode.
# Set to 0 to disable this feature.<br/>
# (in milliseconds)
#_advanced
#_display_name=RemoteApp bypass legal notice delay
remoteapp_bypass_legal_notice_delay = integer(min=0, default=0)

# Time limit to automatically bypass Windows's Legal Notice screen in RemoteApp mode.
# Set to 0 to disable this feature.<br/>
# (in milliseconds)
#_advanced
#_display_name=RemoteApp bypass legal notice timeout
remoteapp_bypass_legal_notice_timeout = integer(min=0, default=20000)

# Some events such as 'Preferred DropEffect' have no particular meaning. This option allows you to exclude these types of events from the logs.
#_advanced
log_only_relevant_clipboard_activities = boolean(default=True)

# Force splitting target domain and username with '@' separator.
#_advanced
split_domain = boolean(default=False)

# Enables Session Shadowing Support.
# Session probe must be enabled on target connection policy.
# Target server must support "Remote Desktop Shadowing" feature.
# When enabled, users can share their RDP sessions with auditors who request it.
#_advanced
session_shadowing_support = boolean(default=True)

[session_probe]

# If enabled, a string of random characters will be added to the name of the Session Probe executable.
# The result could be: SesProbe-5420.exe
# Some other features automatically enable customization of the Session Probe executable name. Application Driver auto-deployment for example.
#_advanced
customize_executable_name = boolean(default=False)

# If enabled, the RDP Proxy accepts to perform the handshake several times during the same RDP session. Otherwise, any new handshake attempt will interrupt the current session with the display of an alert message.
#_advanced
#_display_name=Allow multiple handshakes
allow_multiple_handshake = boolean(default=False)

[mod_vnc]

# Check this option to enable the clipboard upload (from client to target server).
# This only supports text data clipboard (not files).
clipboard_up = boolean(default=False)

# Check this option to enable the clipboard download (from target server to client).
# This only supports text data clipboard (not files).
clipboard_down = boolean(default=False)

# Sets additional graphics encoding types that will be negotiated with the VNC target server:
# &nbsp; &nbsp;   2: RRE
# &nbsp; &nbsp;   5: HEXTILE
# &nbsp; &nbsp;   16: ZRLE<br/>
# (values are comma-separated)
#_advanced
encodings = string(default="")

# VNC target server clipboard text data encoding type.
#_advanced
server_clipboard_encoding_type = option('utf-8', 'latin1', default="latin1")

# The RDP clipboard is based on a token that indicates who owns data between target server and client. However, some RDP clients, such as FreeRDP, always appropriate this token. This conflicts with VNC, which also appropriates this token, causing clipboard data to be sent in loops.
# This option indicates the strategy to adopt in such situations.
# &nbsp; &nbsp;   0: delayed: Clipboard processing is deferred and, if necessary, the token is left with the client.
# &nbsp; &nbsp;   1: duplicated: When 2 identical requests are received, the second is ignored. This can block clipboard data reception until a clipboard event is triggered on the target server when the client clipboard is blocked, and vice versa.
# &nbsp; &nbsp;   2: continued: No special processing is done, the proxy always responds immediately.
#_advanced
bogus_clipboard_infinite_loop = option(0, 1, 2, default=0)

[ocr]

# Selects the OCR (Optical Character Recognition) version used to detect title bars when Session Probe is not running.
# Version 1 is a bit faster, but has a higher failure rate in character recognition.
# &nbsp; &nbsp;   1: v1
# &nbsp; &nbsp;   2: v2
version = option(1, 2, default=2)

# &nbsp; &nbsp;   latin: Recognizes Latin characters
# &nbsp; &nbsp;   cyrillic: Recognizes Latin and Cyrillic characters
locale = option('latin', 'cyrillic', default="latin")

# Time interval between two analyses.
# A value too low will affect session reactivity.<br/>
# (in 1/100 seconds)
#_advanced
interval = integer(min=0, default=100)

# Checks shape and color to determine if the text is on a title bar.
#_advanced
on_title_bar_only = boolean(default=True)

# Expressed in percentage,
# &nbsp; &nbsp;   0   - all of characters need be recognized
# &nbsp; &nbsp;   100 - accept all results
#_advanced
max_unrecog_char_rate = integer(min=0, max=100, default=40)

[capture]

# Specifies the type of data to be captured:
# &nbsp; &nbsp;   0x0: none
# &nbsp; &nbsp;   0x1: png
# &nbsp; &nbsp;   0x2: wrm: Session recording file.
# &nbsp; &nbsp;   0x8: ocr<br/>
# Note: values can be added (enable all: 0x1 + 0x2 + 0x8 = 0xb)
#_advanced
#_hex
capture_flags = integer(min=0, max=15, default=11)

# Disable clipboard log:
# &nbsp; &nbsp;   0x0: none
# &nbsp; &nbsp;   0x1: disable clipboard log in recorded sessions
# &nbsp; &nbsp;   0x2: disable clipboard log in recorded meta<br/>
# Note: values can be added (disable all: 0x1 + 0x2 = 0x3)
#_advanced
#_hex
disable_clipboard_log = integer(min=0, max=3, default=0)

# Disable (redirected) file system log:
# &nbsp; &nbsp;   0x0: none
# &nbsp; &nbsp;   0x1: disable (redirected) file system log in recorded sessions
# &nbsp; &nbsp;   0x2: disable (redirected) file system log in recorded meta<br/>
# Note: values can be added (disable all: 0x1 + 0x2 = 0x3)
#_advanced
#_hex
disable_file_system_log = integer(min=0, max=3, default=0)

# Time between two .wrm recording files.
# ⚠ A value that is too small increases the disk space required for recordings.<br/>
# (in seconds)
#_advanced
wrm_break_interval = integer(min=0, default=600)

# Color depth for the Session Recording file (.wrm):
# &nbsp; &nbsp;   0: 24-bit
# &nbsp; &nbsp;   1: 16-bit
#_advanced
wrm_color_depth_selection_strategy = option(0, 1, default=1)

# Compression method of the Session Recording file (.wrm):
# &nbsp; &nbsp;   0: no compression
# &nbsp; &nbsp;   1: GZip: Files are better compressed, but this takes more time and CPU load.
# &nbsp; &nbsp;   2: Snappy: Faster than GZip, but files are less compressed.
#_advanced
wrm_compression_algorithm = option(0, 1, 2, default=1)

[audit]

# Show keyboard input events in the meta file.
# (See also "Keyboard input masking level" option (in "session_log" section of "Connection Policy" configuration) for RDP and "Disable keyboard log" option (in "capture" section of "Connection Policy" configuration) for VNC and RDP)
#_advanced
enable_keyboard_log = boolean(default=True)

# The maximum time between two videos when no title bar is detected.<br/>
# (in seconds)
#_advanced
video_break_interval = integer(min=0, default=604800)

# Maximum number of images per second for video generation.
# A higher value will produce smoother videos, but the file weight is higher and the generation time longer.
#_advanced
video_frame_rate = integer(min=1, max=120, default=5)

# In the generated video of the session record traces, remove the top left banner with the timestamp.
# Can slightly speed up the video generation.
#_advanced
video_notimestamp = boolean(default=False)

# FFmpeg options for video codec. See https://trac.ffmpeg.org/wiki/Encode/H.264
# ⚠ Some browsers and video decoders don't support crf=0
#_advanced
#_display_name=FFmpeg options
ffmpeg_options = string(default="crf=35 preset=superfast")

# &nbsp; &nbsp;   0: disable: When replaying the session video, the content of the RDP viewer matches the size of the client's desktop.
# &nbsp; &nbsp;   1: v1: When replaying the session video, the content of the RDP viewer is restricted to the greatest area covered by the application during the session.
# &nbsp; &nbsp;   2: v2: When replaying the session video, the content of the RDP viewer is fully covered by the size of the greatest application window during the session.
smart_video_cropping = option(0, 1, 2, default=2)

# Checking this option will allow to play a video with corrupted Bitmap Update.
#_advanced
play_video_with_corrupted_bitmap = boolean(default=False)

# Allow real-time view (4 eyes) without session recording enabled in the authorization.
allow_rt_without_recording = boolean(default=False)

[icap_server_down]

# IP or FQDN of ICAP server.
host = string(default="")

# Port of ICAP server.
port = integer(min=0, default=1344)

# Service name on ICAP server.
service_name = string(default="avscan")

# Activate TLS on ICAP server connection.
tls = boolean(default=False)

# Send X Context (Client-IP, Server-IP, Authenticated-User) to ICAP server.
#_advanced
enable_x_context = boolean(default=True)

# Filename sent to ICAP as percent encoding.
#_advanced
filename_percent_encoding = boolean(default=False)

[icap_server_up]

# IP or FQDN of ICAP server.
host = string(default="")

# Port of ICAP server.
port = integer(min=0, default=1344)

# Service name on ICAP server.
service_name = string(default="avscan")

# Activate TLS on ICAP server connection.
tls = boolean(default=False)

# Send X Context (Client-IP, Server-IP, Authenticated-User) to ICAP server.
#_advanced
enable_x_context = boolean(default=True)

# Filename sent to ICAP as percent encoding.
#_advanced
filename_percent_encoding = boolean(default=False)

[internal_mod]

# Allow separate target and login entries by enabling the edit target field on the login page.
#_advanced
enable_target_field = boolean(default=True)

# List of keyboard layouts available by the internal pages button located at bottom left of some internal pages (login, selector, etc).<br/>
# (values are comma-separated)
# <details><summary>Possible values in alphabetical order:</summary><table><tr><th>Value</th><th>Windows 11 name (informative)</th></tr><tr><td>ar-SA.101</td><td>Arabic (101)</td></tr><tr><td>ar-SA.102</td><td>Arabic (102)</td></tr><tr><td>ar-SA.102-azerty</td><td>Arabic (102) AZERTY</td></tr><tr><td>as</td><td>Assamese - INSCRIPT</td></tr><tr><td>az-Cyrl</td><td>Azerbaijani Cyrillic</td></tr><tr><td>az-Latn</td><td>Azerbaijani Latin</td></tr><tr><td>az-Latn.standard</td><td>Azerbaijani (Standard)</td></tr><tr><td>ba-Cyrl</td><td>Bashkir</td></tr><tr><td>be</td><td>Belarusian</td></tr><tr><td>bg-BG</td><td>Bulgarian</td></tr><tr><td>bg-BG.latin</td><td>Bulgarian (Latin)</td></tr><tr><td>bg.phonetic</td><td>Bulgarian (Phonetic)</td></tr><tr><td>bg.phonetic.trad</td><td>Bulgarian (Phonetic Traditional)</td></tr><tr><td>bg.typewriter</td><td>Bulgarian (Typewriter)</td></tr><tr><td>bn-IN</td><td>Bangla</td></tr><tr><td>bn-IN.inscript</td><td>Bangla - INSCRIPT</td></tr><tr><td>bn-IN.legacy</td><td>Bangla - INSCRIPT (Legacy)</td></tr><tr><td>bo-Tibt</td><td>Tibetan (PRC)</td></tr><tr><td>bo-Tibt.updated</td><td>Tibetan (PRC) - Updated</td></tr><tr><td>bs-Cy</td><td>Bosnian (Cyrillic)</td></tr><tr><td>bug-Bugi</td><td>Buginese</td></tr><tr><td>bépo</td><td>French (Standard, BÉPO)</td></tr><tr><td>chr-Cher</td><td>Cherokee Nation</td></tr><tr><td>chr-Cher.phonetic</td><td>Cherokee Phonetic</td></tr><tr><td>cs-CZ</td><td>Czech</td></tr><tr><td>cs-CZ.programmers</td><td>Czech Programmers</td></tr><tr><td>cs-CZ.qwerty</td><td>Czech (QWERTY)</td></tr><tr><td>cy-GB</td><td>United Kingdom Extended</td></tr><tr><td>da-DK</td><td>Danish</td></tr><tr><td>de-CH</td><td>Swiss German</td></tr><tr><td>de-DE</td><td>German</td></tr><tr><td>de-DE.ex1</td><td>German Extended (E1)</td></tr><tr><td>de-DE.ex2</td><td>German Extended (E2)</td></tr><tr><td>de-DE.ibm</td><td>German (IBM)</td></tr><tr><td>dv.phonetic</td><td>Divehi Phonetic</td></tr><tr><td>dv.typewriter</td><td>Divehi Typewriter</td></tr><tr><td>dz</td><td>Dzongkha</td></tr><tr><td>el-GR</td><td>Greek</td></tr><tr><td>el-GR.220</td><td>Greek (220)</td></tr><tr><td>el-GR.220_latin</td><td>Greek (220) Latin</td></tr><tr><td>el-GR.319</td><td>Greek (319)</td></tr><tr><td>el-GR.319_latin</td><td>Greek (319) Latin</td></tr><tr><td>el-GR.latin</td><td>Greek Latin</td></tr><tr><td>el-GR.polytonic</td><td>Greek Polytonic</td></tr><tr><td>en-CA.fr</td><td>Canadian French</td></tr><tr><td>en-CA.multilingual</td><td>Canadian Multilingual Standard</td></tr><tr><td>en-GB</td><td>United Kingdom</td></tr><tr><td>en-IE</td><td>Scottish Gaelic</td></tr><tr><td>en-IE.irish</td><td>Irish</td></tr><tr><td>en-IN</td><td>English (India)</td></tr><tr><td>en-NZ</td><td>NZ Aotearoa</td></tr><tr><td>en-US</td><td>US</td></tr><tr><td>en-US.arabic.238L</td><td>US English Table for IBM Arabic 238_L</td></tr><tr><td>en-US.colemak</td><td>Colemak</td></tr><tr><td>en-US.dvorak</td><td>United States-Dvorak</td></tr><tr><td>en-US.dvorak_left</td><td>United States-Dvorak for left hand</td></tr><tr><td>en-US.dvorak_right</td><td>United States-Dvorak for right hand</td></tr><tr><td>en-US.international</td><td>United States-International</td></tr><tr><td>es-ES</td><td>Spanish</td></tr><tr><td>es-ES.variation</td><td>Spanish Variation</td></tr><tr><td>es-MX</td><td>Latin American</td></tr><tr><td>et-EE</td><td>Estonian</td></tr><tr><td>ett-Ital</td><td>Old Italic</td></tr><tr><td>fa</td><td>Persian</td></tr><tr><td>fa.standard</td><td>Persian (Standard)</td></tr><tr><td>ff-Adlm</td><td>ADLaM</td></tr><tr><td>fi-FI.finnish</td><td>Finnish</td></tr><tr><td>fi-SE</td><td>Finnish with Sami</td></tr><tr><td>fo-FO</td><td>Faeroese</td></tr><tr><td>fr-BE</td><td>Belgian (Comma)</td></tr><tr><td>fr-BE.fr</td><td>Belgian French</td></tr><tr><td>fr-CA</td><td>Canadian French (Legacy)</td></tr><tr><td>fr-CH</td><td>Swiss French</td></tr><tr><td>fr-FR</td><td>French (Legacy, AZERTY)</td></tr><tr><td>fr-FR.standard</td><td>French (Standard, AZERTY)</td></tr><tr><td>gem-Runr</td><td>Futhark</td></tr><tr><td>gn</td><td>Guarani</td></tr><tr><td>got-Goth</td><td>Gothic</td></tr><tr><td>gu</td><td>Gujarati</td></tr><tr><td>ha-Latn</td><td>Hausa</td></tr><tr><td>haw-Latn</td><td>Hawaiian</td></tr><tr><td>he</td><td>Hebrew</td></tr><tr><td>he-IL</td><td>Hebrew (Standard, 2018)</td></tr><tr><td>he.standard</td><td>Hebrew (Standard)</td></tr><tr><td>hi</td><td>Hindi Traditional</td></tr><tr><td>hi.inscript</td><td>Devanagari - INSCRIPT</td></tr><tr><td>hr-HR</td><td>Croatian</td></tr><tr><td>hsb</td><td>Sorbian Standard</td></tr><tr><td>hsb.ex</td><td>Sorbian Extended</td></tr><tr><td>hsb.legacy</td><td>Sorbian Standard (Legacy)</td></tr><tr><td>hu-HU</td><td>Hungarian</td></tr><tr><td>hu-HU.101-key</td><td>Hungarian 101-key</td></tr><tr><td>hy.east</td><td>Armenian Eastern (Legacy)</td></tr><tr><td>hy.phonetic</td><td>Armenian Phonetic</td></tr><tr><td>hy.typewriter</td><td>Armenian Typewriter</td></tr><tr><td>hy.west</td><td>Armenian Western (Legacy)</td></tr><tr><td>ig-Latn</td><td>Igbo</td></tr><tr><td>is-IS</td><td>Icelandic</td></tr><tr><td>it-IT</td><td>Italian</td></tr><tr><td>it-IT.142</td><td>Italian (142)</td></tr><tr><td>iu-Cans</td><td>Inuktitut - Naqittaut</td></tr><tr><td>iu-Cans-CA</td><td>Inuktitut - Nattilik</td></tr><tr><td>iu-La</td><td>Inuktitut - Latin</td></tr><tr><td>ja</td><td>Japanese</td></tr><tr><td>jv-Java</td><td>Javanese</td></tr><tr><td>ka.ergo</td><td>Georgian (Ergonomic)</td></tr><tr><td>ka.legacy</td><td>Georgian (Legacy)</td></tr><tr><td>ka.mes</td><td>Georgian (MES)</td></tr><tr><td>ka.oldalpha</td><td>Georgian (Old Alphabets)</td></tr><tr><td>ka.qwerty</td><td>Georgian (QWERTY)</td></tr><tr><td>khb-Talu</td><td>New Tai Lue</td></tr><tr><td>kk-KZ</td><td>Kazakh</td></tr><tr><td>kl</td><td>Greenlandic</td></tr><tr><td>km</td><td>Khmer</td></tr><tr><td>km.nida</td><td>Khmer (NIDA)</td></tr><tr><td>kn</td><td>Kannada</td></tr><tr><td>ko</td><td>Korean</td></tr><tr><td>ku-Arab</td><td>Central Kurdish</td></tr><tr><td>ky-KG</td><td>Kyrgyz Cyrillic</td></tr><tr><td>lb-LU</td><td>Luxembourgish</td></tr><tr><td>lis-Lisu</td><td>Lisu (Standard)</td></tr><tr><td>lis-Lisu.basic</td><td>Lisu (Basic)</td></tr><tr><td>lo</td><td>Lao</td></tr><tr><td>lt</td><td>Lithuanian Standard</td></tr><tr><td>lt-LT</td><td>Lithuanian</td></tr><tr><td>lt-LT.ibm</td><td>Lithuanian IBM</td></tr><tr><td>lv</td><td>Latvian (Standard)</td></tr><tr><td>lv-LV</td><td>Latvian</td></tr><tr><td>lv-LV.qwerty</td><td>Latvian (QWERTY)</td></tr><tr><td>mi-NZ</td><td>Maori</td></tr><tr><td>mk</td><td>Macedonian - Standard</td></tr><tr><td>mk-MK</td><td>Macedonian</td></tr><tr><td>ml</td><td>Malayalam</td></tr><tr><td>mn-MN</td><td>Mongolian Cyrillic</td></tr><tr><td>mn-Mong</td><td>Traditional Mongolian (Standard)</td></tr><tr><td>mn-Mong-CN</td><td>Traditional Mongolian (MNS)</td></tr><tr><td>mn-Mong.script</td><td>Mongolian (Mongolian Script)</td></tr><tr><td>mn-Phag</td><td>Phags-pa</td></tr><tr><td>mr</td><td>Marathi</td></tr><tr><td>mt-MT.47</td><td>Maltese 47-Key</td></tr><tr><td>mt-MT.48</td><td>Maltese 48-Key</td></tr><tr><td>my.phonetic</td><td>Myanmar (Phonetic order)</td></tr><tr><td>my.visual</td><td>Myanmar (Visual order)</td></tr><tr><td>nb-NO</td><td>Norwegian</td></tr><tr><td>ne-NP</td><td>Nepali</td></tr><tr><td>nl-BE</td><td>Belgian (Period)</td></tr><tr><td>nl-NL</td><td>Dutch</td></tr><tr><td>nqo</td><td>N’Ko</td></tr><tr><td>nso</td><td>Sesotho sa Leboa</td></tr><tr><td>or</td><td>Odia</td></tr><tr><td>osa-Osge</td><td>Osage</td></tr><tr><td>pa</td><td>Punjabi</td></tr><tr><td>pl-PL</td><td>Polish (214)</td></tr><tr><td>pl-PL.programmers</td><td>Polish (Programmers)</td></tr><tr><td>ps</td><td>Pashto (Afghanistan)</td></tr><tr><td>pt-BR.abnt</td><td>Portuguese (Brazil ABNT)</td></tr><tr><td>pt-BR.abnt2</td><td>Portuguese (Brazil ABNT2)</td></tr><tr><td>pt-PT</td><td>Portuguese</td></tr><tr><td>ro-RO</td><td>Romanian (Standard)</td></tr><tr><td>ro-RO.legacy</td><td>Romanian (Legacy)</td></tr><tr><td>ro-RO.programmers</td><td>Romanian (Programmers)</td></tr><tr><td>ru</td><td>Russian - Mnemonic</td></tr><tr><td>ru-RU</td><td>Russian</td></tr><tr><td>ru-RU.typewriter</td><td>Russian (Typewriter)</td></tr><tr><td>sah-Cyrl</td><td>Sakha</td></tr><tr><td>sat-Olck</td><td>Ol Chiki</td></tr><tr><td>se-NO</td><td>Norwegian with Sami</td></tr><tr><td>se-NO.ext_norway</td><td>Sami Extended Norway</td></tr><tr><td>se-SE</td><td>Swedish with Sami</td></tr><tr><td>se-SE.ext_finland_sweden</td><td>Sami Extended Finland-Sweden</td></tr><tr><td>sga-Ogam</td><td>Ogham</td></tr><tr><td>si</td><td>Sinhala</td></tr><tr><td>si.wij9</td><td>Sinhala - Wij 9</td></tr><tr><td>sk-SK</td><td>Slovak</td></tr><tr><td>sk-SK.qwerty</td><td>Slovak (QWERTY)</td></tr><tr><td>sl-SI</td><td>Slovenian</td></tr><tr><td>so-Osma</td><td>Osmanya</td></tr><tr><td>sq</td><td>Albanian</td></tr><tr><td>sr-Cy</td><td>Serbian (Cyrillic)</td></tr><tr><td>sr-La</td><td>Serbian (Latin)</td></tr><tr><td>srb-Sora</td><td>Sora</td></tr><tr><td>sv-SE</td><td>Swedish</td></tr><tr><td>syr-Syrc</td><td>Syriac</td></tr><tr><td>syr-Syrc.phonetic</td><td>Syriac Phonetic</td></tr><tr><td>ta-IN</td><td>Tamil</td></tr><tr><td>ta-IN.99</td><td>Tamil 99</td></tr><tr><td>ta-IN.anjal</td><td>Tamil Anjal</td></tr><tr><td>tdd-Tale</td><td>Tai Le</td></tr><tr><td>te</td><td>Telugu</td></tr><tr><td>tg-Cyrl</td><td>Tajik</td></tr><tr><td>th</td><td>Thai Kedmanee</td></tr><tr><td>th.non-shiftlock</td><td>Thai Kedmanee (non-ShiftLock)</td></tr><tr><td>th.pattachote</td><td>Thai Pattachote</td></tr><tr><td>th.pattachote.non-shiftlock</td><td>Thai Pattachote (non-ShiftLock)</td></tr><tr><td>tk-Latn</td><td>Turkmen</td></tr><tr><td>tn-ZA</td><td>Setswana</td></tr><tr><td>tr-TR.f</td><td>Turkish F</td></tr><tr><td>tr-TR.q</td><td>Turkish Q</td></tr><tr><td>tt-Cyrl</td><td>Tatar</td></tr><tr><td>tt-RU</td><td>Tatar (Legacy)</td></tr><tr><td>tzm-Latn</td><td>Central Atlas Tamazight</td></tr><tr><td>tzm-Tfng</td><td>Tifinagh (Basic)</td></tr><tr><td>tzm-Tfng.ex</td><td>Tifinagh (Extended)</td></tr><tr><td>ug-Arab</td><td>Uyghur</td></tr><tr><td>ug-Arab.legacy</td><td>Uyghur (Legacy)</td></tr><tr><td>uk</td><td>Ukrainian (Enhanced)</td></tr><tr><td>uk-UA</td><td>Ukrainian</td></tr><tr><td>ur-PK</td><td>Urdu</td></tr><tr><td>uz-Cy</td><td>Uzbek Cyrillic</td></tr><tr><td>vi</td><td>Vietnamese</td></tr><tr><td>wo-Latn</td><td>Wolof</td></tr><tr><td>yo-Latn</td><td>Yoruba</td></tr><tr><td>zh-Hans-CN</td><td>Chinese (Simplified) - US</td></tr><tr><td>zh-Hans-SG</td><td>Chinese (Simplified, Singapore) - US</td></tr><tr><td>zh-Hant-HK</td><td>Chinese (Traditional, Hong Kong S.A.R.) - US</td></tr><tr><td>zh-Hant-MO</td><td>Chinese (Traditional, Macao S.A.R.) - US</td></tr><tr><td>zh-Hant-TW</td><td>Chinese (Traditional) - US</td></tr></table></details>
#_advanced
keyboard_layout_proposals = string(default="en-US, fr-FR, de-DE")

# Display the close screen.
# Displays secondary connection errors and closes automatically after the specified "Close box timeout" option value or on user request.
enable_close_box = boolean(default=True)

# Specifies the time to spend on the close box of the RDP proxy before closing client window.
# ⚠ Value 0 deactivates the timer and the connection remains open until the client disconnects.<br/>
# (in seconds)
#_advanced
close_box_timeout = integer(min=0, default=600)

[translation]

# The login page shows this language to all users. Once logged in, users see their preferred language.
# &nbsp; &nbsp;   Auto: The language is determined based on the keyboard layout specified by the client.
# &nbsp; &nbsp;   EN: 
# &nbsp; &nbsp;   FR: 
#_advanced
login_language = option('Auto', 'EN', 'FR', default="Auto")

[theme]

# Enable custom theme color configuration.
enable_theme = boolean(default=False)

# Logo displayed when theme is enabled.
#_image=/var/wab/images/rdp-oem-logo.png
logo = string(default=")gen_config_ini" << (REDEMPTION_CONFIG_THEME_LOGO) << R"gen_config_ini(")

# Background color for window, label and button.<br/>
# (in rgb format: hexadecimal (0x21AF21), #rgb (#2fa), #rrggbb (#22ffaa) or a <a href="https://en.wikipedia.org/wiki/Web_colors#Extended_colors">named color</a> case insensitive (red, skyBlue, etc))
bgcolor = string(default="#081F60")

# Foreground color for window, label and button.<br/>
# (in rgb format: hexadecimal (0x21AF21), #rgb (#2fa), #rrggbb (#22ffaa) or a <a href="https://en.wikipedia.org/wiki/Web_colors#Extended_colors">named color</a> case insensitive (red, skyBlue, etc))
fgcolor = string(default="#FFFFFF")

# Separator line color used with some widgets.<br/>
# (in rgb format: hexadecimal (0x21AF21), #rgb (#2fa), #rrggbb (#22ffaa) or a <a href="https://en.wikipedia.org/wiki/Web_colors#Extended_colors">named color</a> case insensitive (red, skyBlue, etc))
separator_color = string(default="#CFD5EB")

# Background color used by buttons when they have focus.<br/>
# (in rgb format: hexadecimal (0x21AF21), #rgb (#2fa), #rrggbb (#22ffaa) or a <a href="https://en.wikipedia.org/wiki/Web_colors#Extended_colors">named color</a> case insensitive (red, skyBlue, etc))
focus_color = string(default="#004D9C")

# Text color for error messages. For example, an authentication error in the login.<br/>
# (in rgb format: hexadecimal (0x21AF21), #rgb (#2fa), #rrggbb (#22ffaa) or a <a href="https://en.wikipedia.org/wiki/Web_colors#Extended_colors">named color</a> case insensitive (red, skyBlue, etc))
error_color = string(default="#FFFF00")

# Background color for editing field.<br/>
# (in rgb format: hexadecimal (0x21AF21), #rgb (#2fa), #rrggbb (#22ffaa) or a <a href="https://en.wikipedia.org/wiki/Web_colors#Extended_colors">named color</a> case insensitive (red, skyBlue, etc))
edit_bgcolor = string(default="#FFFFFF")

# Foreground color for editing field.<br/>
# (in rgb format: hexadecimal (0x21AF21), #rgb (#2fa), #rrggbb (#22ffaa) or a <a href="https://en.wikipedia.org/wiki/Web_colors#Extended_colors">named color</a> case insensitive (red, skyBlue, etc))
edit_fgcolor = string(default="#000000")

# Outline color for editing field.<br/>
# (in rgb format: hexadecimal (0x21AF21), #rgb (#2fa), #rrggbb (#22ffaa) or a <a href="https://en.wikipedia.org/wiki/Web_colors#Extended_colors">named color</a> case insensitive (red, skyBlue, etc))
edit_border_color = string(default="#081F60")

# Outline color for editing field that has focus.<br/>
# (in rgb format: hexadecimal (0x21AF21), #rgb (#2fa), #rrggbb (#22ffaa) or a <a href="https://en.wikipedia.org/wiki/Web_colors#Extended_colors">named color</a> case insensitive (red, skyBlue, etc))
edit_focus_border_color = string(default="#004D9C")

# Cursor color for editing field.<br/>
# (in rgb format: hexadecimal (0x21AF21), #rgb (#2fa), #rrggbb (#22ffaa) or a <a href="https://en.wikipedia.org/wiki/Web_colors#Extended_colors">named color</a> case insensitive (red, skyBlue, etc))
edit_cursor_color = string(default="#888888")

# Placeholder text color for editing field with a little resolution.<br/>
# (in rgb format: hexadecimal (0x21AF21), #rgb (#2fa), #rrggbb (#22ffaa) or a <a href="https://en.wikipedia.org/wiki/Web_colors#Extended_colors">named color</a> case insensitive (red, skyBlue, etc))
edit_placeholder_color = string(default="#A0A0A0")

# Foreground color for toggle button of password field.<br/>
# (in rgb format: hexadecimal (0x21AF21), #rgb (#2fa), #rrggbb (#22ffaa) or a <a href="https://en.wikipedia.org/wiki/Web_colors#Extended_colors">named color</a> case insensitive (red, skyBlue, etc))
password_toggle_color = string(default="#A0A0A0")

# Background color for tooltip.<br/>
# (in rgb format: hexadecimal (0x21AF21), #rgb (#2fa), #rrggbb (#22ffaa) or a <a href="https://en.wikipedia.org/wiki/Web_colors#Extended_colors">named color</a> case insensitive (red, skyBlue, etc))
tooltip_bgcolor = string(default="#FFFF9F")

# Foreground color for tooltip.<br/>
# (in rgb format: hexadecimal (0x21AF21), #rgb (#2fa), #rrggbb (#22ffaa) or a <a href="https://en.wikipedia.org/wiki/Web_colors#Extended_colors">named color</a> case insensitive (red, skyBlue, etc))
tooltip_fgcolor = string(default="#000000")

# Border color for tooltip.<br/>
# (in rgb format: hexadecimal (0x21AF21), #rgb (#2fa), #rrggbb (#22ffaa) or a <a href="https://en.wikipedia.org/wiki/Web_colors#Extended_colors">named color</a> case insensitive (red, skyBlue, etc))
tooltip_border_color = string(default="#000000")

# Background color for even rows in the selector widget.<br/>
# (in rgb format: hexadecimal (0x21AF21), #rgb (#2fa), #rrggbb (#22ffaa) or a <a href="https://en.wikipedia.org/wiki/Web_colors#Extended_colors">named color</a> case insensitive (red, skyBlue, etc))
selector_line1_bgcolor = string(default="#E9ECF6")

# Foreground color for even rows in the selector widget.<br/>
# (in rgb format: hexadecimal (0x21AF21), #rgb (#2fa), #rrggbb (#22ffaa) or a <a href="https://en.wikipedia.org/wiki/Web_colors#Extended_colors">named color</a> case insensitive (red, skyBlue, etc))
selector_line1_fgcolor = string(default="#000000")

# Background color for odd rows in the selector widget.<br/>
# (in rgb format: hexadecimal (0x21AF21), #rgb (#2fa), #rrggbb (#22ffaa) or a <a href="https://en.wikipedia.org/wiki/Web_colors#Extended_colors">named color</a> case insensitive (red, skyBlue, etc))
selector_line2_bgcolor = string(default="#CFD5EB")

# Foreground color for odd rows in the selector widget.<br/>
# (in rgb format: hexadecimal (0x21AF21), #rgb (#2fa), #rrggbb (#22ffaa) or a <a href="https://en.wikipedia.org/wiki/Web_colors#Extended_colors">named color</a> case insensitive (red, skyBlue, etc))
selector_line2_fgcolor = string(default="#000000")

# Background color for the row that has focus in the selector widget.<br/>
# (in rgb format: hexadecimal (0x21AF21), #rgb (#2fa), #rrggbb (#22ffaa) or a <a href="https://en.wikipedia.org/wiki/Web_colors#Extended_colors">named color</a> case insensitive (red, skyBlue, etc))
selector_focus_bgcolor = string(default="#004D9C")

# Foreground color for the row that has focus in the selector widget.<br/>
# (in rgb format: hexadecimal (0x21AF21), #rgb (#2fa), #rrggbb (#22ffaa) or a <a href="https://en.wikipedia.org/wiki/Web_colors#Extended_colors">named color</a> case insensitive (red, skyBlue, etc))
selector_focus_fgcolor = string(default="#FFFFFF")

# Background color for the row that is selected in the selector widget but does not have focus.<br/>
# (in rgb format: hexadecimal (0x21AF21), #rgb (#2fa), #rrggbb (#22ffaa) or a <a href="https://en.wikipedia.org/wiki/Web_colors#Extended_colors">named color</a> case insensitive (red, skyBlue, etc))
selector_selected_bgcolor = string(default="#4472C4")

# Foreground color for the row that is selected in the selector widget but does not have focus.<br/>
# (in rgb format: hexadecimal (0x21AF21), #rgb (#2fa), #rrggbb (#22ffaa) or a <a href="https://en.wikipedia.org/wiki/Web_colors#Extended_colors">named color</a> case insensitive (red, skyBlue, etc))
selector_selected_fgcolor = string(default="#FFFFFF")

# Background color for name of filter fields in the selector widget.<br/>
# (in rgb format: hexadecimal (0x21AF21), #rgb (#2fa), #rrggbb (#22ffaa) or a <a href="https://en.wikipedia.org/wiki/Web_colors#Extended_colors">named color</a> case insensitive (red, skyBlue, etc))
selector_label_bgcolor = string(default="#4472C4")

# Foreground color for name of filter fields in the selector widget.<br/>
# (in rgb format: hexadecimal (0x21AF21), #rgb (#2fa), #rrggbb (#22ffaa) or a <a href="https://en.wikipedia.org/wiki/Web_colors#Extended_colors">named color</a> case insensitive (red, skyBlue, etc))
selector_label_fgcolor = string(default="#FFFFFF")

[debug]

# Restrict target debugging to a specific primary user.
#_advanced
primary_user = string(default="")

# - kbd / ocr when != 0<br/>
# (Wrm)
# - pointer             = 0x0004
# - primary_orders      = 0x0020
# - secondary_orders    = 0x0040
# - bitmap_update       = 0x0080
# - surface_commands    = 0x0100
# - bmp_cache           = 0x0200
# - internal_buffer     = 0x0400
# - sec_decrypted       = 0x1000
#_advanced
#_hex
capture = integer(min=0, default=0)

# - variable = 0x0002
# - buffer   = 0x0040
# - dump     = 0x1000
#_advanced
#_hex
auth = integer(min=0, default=0)

# - Log   = 0x01
# - Event = 0x02
# - Acl   = 0x04
# - Trace = 0x08
#_advanced
#_hex
session = integer(min=0, default=0)

# - basic_trace     = 0x00000001
# - basic_trace2    = 0x00000002
# - basic_trace3    = 0x00000004
# - basic_trace4    = 0x00000008
# - basic_trace5    = 0x00000020
# - graphic         = 0x00000040
# - channel         = 0x00000080
# - cache_from_disk = 0x00000400
# - bmp_info        = 0x00000800
# - global_channel  = 0x00002000
# - sec_decrypted   = 0x00004000
# - keymap          = 0x00008000
# - nla             = 0x00010000
# - nla_dump        = 0x00020000<br/>
# (Serializer)
# - pointer             = 0x00040000
# - primary_orders      = 0x00200000
# - secondary_orders    = 0x00400000
# - bitmap_update       = 0x00800000
# - surface_commands    = 0x01000000
# - bmp_cache           = 0x02000000
# - internal_buffer     = 0x04000000
# - sec_decrypted       = 0x10000000
#_advanced
#_hex
front = integer(min=0, default=0)

# - basic_trace         = 0x00000001
# - connection          = 0x00000002
# - security            = 0x00000004
# - capabilities        = 0x00000008
# - license             = 0x00000010
# - asynchronous_task   = 0x00000020
# - graphics_pointer    = 0x00000040
# - graphics            = 0x00000080
# - input               = 0x00000100
# - rail_order          = 0x00000200
# - credssp             = 0x00000400
# - negotiation         = 0x00000800
# - cache_persister     = 0x00001000
# - fsdrvmgr            = 0x00002000
# - sesprobe_launcher   = 0x00004000
# - sesprobe_repetitive = 0x00008000
# - drdynvc             = 0x00010000
# - surfaceCmd          = 0x00020000
# - cache_from_disk     = 0x00040000
# - bmp_info            = 0x00080000
# - drdynvc_dump        = 0x00100000
# - printer             = 0x00200000
# - rdpsnd              = 0x00400000
# - channels            = 0x00800000
# - rail                = 0x01000000
# - sesprobe            = 0x02000000
# - cliprdr             = 0x04000000
# - rdpdr               = 0x08000000
# - rail_dump           = 0x10000000
# - sesprobe_dump       = 0x20000000
# - cliprdr_dump        = 0x40000000
# - rdpdr_dump          = 0x80000000
#_advanced
#_hex
#_display_name=Mod RDP
mod_rdp = integer(min=0, default=0)

# - basic_trace     = 0x00000001
# - keymap_stack    = 0x00000002
# - draw_event      = 0x00000004
# - input           = 0x00000008
# - connection      = 0x00000010
# - hextile_encoder = 0x00000020
# - cursor_encoder  = 0x00000040
# - clipboard       = 0x00000080
# - zrle_encoder    = 0x00000100
# - zrle_trace      = 0x00000200
# - hextile_trace   = 0x00000400
# - cursor_trace    = 0x00001000
# - rre_encoder     = 0x00002000
# - rre_trace       = 0x00004000
# - raw_encoder     = 0x00008000
# - raw_trace       = 0x00010000
# - copyrect_encoder= 0x00020000
# - copyrect_trace  = 0x00040000
# - keymap          = 0x00080000
#_advanced
#_hex
#_display_name=Mod VNC
mod_vnc = integer(min=0, default=0)

# - copy_paste != 0
# - client_execute = 0x01
#_advanced
#_hex
mod_internal = integer(min=0, default=0)

# - basic    = 0x0001
# - dump     = 0x0002
# - watchdog = 0x0004
# - meta     = 0x0008
#_advanced
#_hex
sck_mod = integer(min=0, default=0)

# - basic    = 0x0001
# - dump     = 0x0002
# - watchdog = 0x0004
# - meta     = 0x0008
#_advanced
#_hex
sck_front = integer(min=0, default=0)

# - when != 0
#_advanced
#_hex
compression = integer(min=0, default=0)

# - life       = 0x0001
# - persistent = 0x0200
#_advanced
#_hex
cache = integer(min=0, default=0)

# - when != 0
#_advanced
#_hex
ocr = integer(min=0, default=0)

# Value passed to function av_log_set_level()
# See https://www.ffmpeg.org/doxygen/2.3/group__lavu__log__constants.html
#_advanced
#_hex
#_display_name=FFmpeg
ffmpeg = integer(min=0, default=0)

# Log unknown members or sections.
#_advanced
config = boolean(default=True)

# List of client probe IP addresses (e.g. ip1, ip2, etc.) to prevent some continuous logs.<br/>
# (values are comma-separated)
#_advanced
probe_client_addresses = string(default="")

)gen_config_ini"
