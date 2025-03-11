[general]

# Secondary login Transformation rule
# ${LOGIN} will be replaced by login
# ${DOMAIN} (optional) will be replaced by domain if it exists.
# Empty value means no transformation rule.
transformation_rule = string(default="")

# Account Mapping password retriever
# Transformation to apply to find the correct account.
# ${USER} will be replaced by the user's login.
# ${DOMAIN} will be replaced by the user's domain (in case of LDAP mapping).
# ${USER_DOMAIN} will be replaced by the user's login + "@" + user's domain (or just user's login if there's no domain).
# ${GROUP} will be replaced by the authorization's user group.
# ${DEVICE} will be replaced by the device's name.
# A regular expression is allowed to transform a variable, with the syntax: ${USER:/regex/replacement}, groups can be captured with parentheses and used with \1, \2, ...
# For example to replace leading "A" by "B" in the username: ${USER:/^A/B}
# Empty value means no transformation rule.
vault_transformation_rule = string(default="")

[session]

# Set how long a user can stay inactive before being disconnected from a target session. The timer starts immediately after the secondary authentication.
# Values between 1 and 30 default to a 30-second timeout.
# If set to 0, the timeout value from "Base inactivity timeout" option (in "globals" section of "RDP Proxy" configuration option) is used.<br/>
# (in seconds)
inactivity_timeout = integer(min=0, default=0)

[all_target_mod]

# Specify max timeout before a TCP connection is aborted. If set to 0, the system default TCP timeout is used.<br/>
# (in milliseconds)
#_advanced
#_display_name=TCP user timeout
tcp_user_timeout = integer(min=0, max=3600000, default=0)

[server_cert]

# Keep known target server certificates on Bastion
server_cert_store = boolean(default=True)

# Configure server certificate verification behavior.
# &nbsp; &nbsp;   0: Fails if the certificate is missing or does not match the known certificate.
# &nbsp; &nbsp;   1: Fails if the certificate does not match the known certificate; succeeds if no certificate exists.
# &nbsp; &nbsp;   2: Succeeds if a certificate exists (verification skipped); fails if no certificate exists.
# &nbsp; &nbsp;   3: Always succeeds without performing certificate validation.
# Internal errors, such as failure to access a known certificate or decode it, always result in connection rejection.
server_cert_check = option(0, 1, 2, 3, default=1)

# Verify server certificate by using internal X509 Certificate Authority configured in Configuration > Certificate authorities.
# When enabled, "Server cert check" option is ignored.
#_display_name=Server cert check CA
server_cert_check_using_ca = boolean(default=False)

# Warn if check allow connection to target server.
# &nbsp; &nbsp;   0x0: nobody
# &nbsp; &nbsp;   0x1: SIEM: message sent to SIEM<br/>
# Note: values can be added (enable all: 0x1 = 0x1)
#_advanced
#_hex
server_access_allowed_message = integer(min=0, max=1, default=0)

# Warn that new target server certificate file was created.
# &nbsp; &nbsp;   0x0: nobody
# &nbsp; &nbsp;   0x1: SIEM: message sent to SIEM<br/>
# Note: values can be added (enable all: 0x1 = 0x1)
#_advanced
#_hex
server_cert_create_message = integer(min=0, max=1, default=1)

# Warn that target server certificate file was successfully checked.
# &nbsp; &nbsp;   0x0: nobody
# &nbsp; &nbsp;   0x1: SIEM: message sent to SIEM<br/>
# Note: values can be added (enable all: 0x1 = 0x1)
#_advanced
#_hex
server_cert_success_message = integer(min=0, max=1, default=0)

# Warn that target server certificate file checking failed.
# &nbsp; &nbsp;   0x0: nobody
# &nbsp; &nbsp;   0x1: SIEM: message sent to SIEM<br/>
# Note: values can be added (enable all: 0x1 = 0x1)
#_advanced
#_hex
server_cert_failure_message = integer(min=0, max=1, default=1)

[vnc]

support_cursor_pseudo_encoding = boolean(default=True)

#_display_name=Server is MacOS
server_is_macos = boolean(default=False)

# When disabled, Ctrl + Alt becomes AltGr (Windows behavior).
#_display_name=Server Unix alt
server_unix_alt = boolean(default=False)

# Enable target connection on IPv6.
#_display_name=Enable IPv6
enable_ipv6 = boolean(default=True)

# Minimal incoming TLS level 0=TLSv1, 1=TLSv1.1, 2=TLSv1.2, 3=TLSv1.3
#_display_name=TLS min level
tls_min_level = integer(min=0, default=2)

# Maximal incoming TLS level 0=no restriction, 1=TLSv1.1, 2=TLSv1.2, 3=TLSv1.3
#_display_name=TLS max level
tls_max_level = integer(min=0, default=0)

# TLSv1.2 and below additional ciphers supported.
# Empty to apply system-wide configuration (SSL security level 2), ALL for support of all ciphers to ensure highest compatibility with target servers.
# For details on the format, refer to this page: https://www.openssl.org/docs/man3.1/man1/openssl-ciphers.html#CIPHER-LIST-FORMAT
cipher_string = string(default="ECDHE-ECDSA-AES256-GCM-SHA384:ECDHE-RSA-AES256-GCM-SHA384:DHE-RSA-AES256-GCM-SHA384:ECDHE-ECDSA-AES128-GCM-SHA256:ECDHE-RSA-AES128-GCM-SHA256:DHE-RSA-AES128-GCM-SHA256")

# Allow TLS legacy insecure renegotiation to unpatched target servers.
#_advanced
#_display_name=TLS enable legacy server
tls_enable_legacy_server = boolean(default=False)

# Configure the available TLSv1.3 cipher suites.
# Empty to apply system-wide configuration.
# For details on the format, refer to the third paragraph on this page: https://www.openssl.org/docs/man1.1.1/man3/SSL_CTX_set_ciphersuites.html#DESCRIPTION
#_display_name=TLS 1.3 cipher suites
tls_1_3_ciphersuites = string(default="")

# Configure the supported key exchange groups.
# Empty to apply system-wide configuration.
# For details on the format, refer to this page: https://www.openssl.org/docs/man3.2/man3/SSL_CONF_cmd.html#groups-groups
#_display_name=TLS key exchange groups
tls_key_exchange_groups = string(default="P-256:P-384:P-521:ffdhe3072:ffdhe4096:ffdhe6144:ffdhe8192")

# Configure the supported client signature algorithms.
# Empty to apply system-wide configuration.
# The format should be a colon-separated list of signature algorithms, ordered by decreasing preference. Each entry should follow one of these forms: algorithm+hash or signature_scheme.
# algorithm options: RSA, RSA-PSS or ECDSA.
# hash options: SHA224, SHA256, SHA384 or SHA512.
# signature_scheme options: TLSv1.3 signature schemes (rfc8446#section-4.2.3) identified by their IETF names (e.g., ecdsa_secp384r1_sha384 or rsa_pss_rsae_sha256).
#_display_name=TLS signature algorithms
tls_signature_algorithms = string(default="")

# Show in the logs the common cipher list supported by client and target server.
# ⚠ Only for debugging purposes.
#_advanced
show_common_cipher_list = boolean(default=False)

# ⚠ The use of this feature is not recommended!<br/>
# When specified, forces the proxy to use a specific authentication method. If this method is not supported by the target server, the connection will not be made.
# &nbsp; &nbsp;   - noauth
# &nbsp; &nbsp;   - vncauth
# &nbsp; &nbsp;   - mslogon
# &nbsp; &nbsp;   - mslogoniiauth
# &nbsp; &nbsp;   - ultravnc_dsm_old
# &nbsp; &nbsp;   - ultravnc_dsm_new
# &nbsp; &nbsp;   - tlsnone
# &nbsp; &nbsp;   - tlsvnc
# &nbsp; &nbsp;   - tlsplain
# &nbsp; &nbsp;   - x509none
# &nbsp; &nbsp;   - x509vnc
# &nbsp; &nbsp;   - x509plain
#_advanced
force_authentication_method = string(default="")

[clipboard]

# VNC target server clipboard text data encoding type.
#_advanced
clipboard_encoding = option('utf-8', 'latin1', default="latin1")

# The RDP clipboard is based on a token that indicates who owns data between target server and client. However, some RDP clients, such as FreeRDP, always appropriate this token. This conflicts with VNC, which also appropriates this token, causing clipboard data to be sent in loops.
# This option indicates the strategy to adopt in such situations.
# &nbsp; &nbsp;   0: delayed: Clipboard processing is deferred and, if necessary, the token is left with the client.
# &nbsp; &nbsp;   1: duplicated: When 2 identical requests are received, the second is ignored. This can block clipboard data reception until a clipboard event is triggered on the target server when the client clipboard is blocked, and vice versa.
# &nbsp; &nbsp;   2: continued: No special processing is done, the proxy always responds immediately.
#_advanced
bogus_infinite_loop_strategy = option(0, 1, 2, default=0)

[file_transfer]

# Maximum item in folder showed by the GUI.
#_advanced
#_display_name=Max item in GUI
max_item_in_gui = integer(min=0, default=100000)

# Maximum file number authorized for one upload or download.
#_advanced
max_file_transfer_list = integer(min=0, default=10000)

# Maximum file size authorized for an upload or a download.
#_advanced
max_file_size = integer(min=0, default=268435456)

[capture]

# ⚠ Logs may contain passwords.<br/>
# Configure how keyboard inputs are logged:
# &nbsp; &nbsp;   0x0: Log all keyboard inputs
# &nbsp; &nbsp;   0x1: Exclude keyboard inputs from session logs (including SIEM)
# &nbsp; &nbsp;   0x2: Exclude keyboard inputs from recorded sessions<br/>
# Note: values can be added (disable all: 0x1 + 0x2 = 0x3)
#_advanced
#_hex
disable_keyboard_log = integer(min=0, max=3, default=3)

[file_verification]

# Enable use of ICAP service for file verification on upload.
enable_up = boolean(default=False)

# Enable use of ICAP service for file verification on download.
enable_down = boolean(default=False)

# Log the files and clipboard texts that are verified and accepted. If deactivated, only those rejected are logged.
#_advanced
log_if_accepted = boolean(default=True)

# ⚠ This value affects the RAM used by the session.<br/>
# If option Block invalid file (up or down) is enabled, automatically reject file with greater filesize.<br/>
# (in megabytes)
#_advanced
max_file_size_rejected = integer(min=0, default=256)

[file_storage]

# Enable storage of transferred files (via RDP Clipboard).
# ⚠ Saving files can take up a lot of disk space.
# &nbsp; &nbsp;   never: Never store transferred files.
# &nbsp; &nbsp;   always: Always store transferred files.
# &nbsp; &nbsp;   on_invalid_verification: Store transferred files only if file verification fails. Requires ICAP file verification (in section file_verification).
store_file = option('never', 'always', 'on_invalid_verification', default="never")

[vnc_over_ssh]

enable = boolean(default=False)

# Port to be used for SSH tunneling
#_display_name=SSH port
ssh_port = integer(min=0, default=22)

# static_login: Static values provided in "SSH login" option &amp; "SSH password" option fields will be used to establish the SSH tunnel.
# scenario_account: Scenario account provided in "Scenario account name" option field will be used to establish the SSH tunnel. (Recommended)
tunneling_credential_source = option('static_login', 'scenario_account', default="scenario_account")

# Login to be used for SSH tunneling.
#_display_name=SSH login
ssh_login = string(default="")

# Password to be used for SSH tunneling.
#_display_name=SSH password
ssh_password = string(default="")

# With the following syntax: "account_name@domain_name[@[device_name]]".<br/>
# Syntax for using global domain scenario account:
# &nbsp; &nbsp;   "account_name@global_domain_name"<br/>
# Syntax for using local domain scenario account (with automatic device name deduction):
# &nbsp; &nbsp;   "account_name@local_domain_name@"
scenario_account_name = string(default="")

# Only for debugging purposes.
#_advanced
tunneling_type = option('pxssh', 'pexpect', 'popen', default="pxssh")

