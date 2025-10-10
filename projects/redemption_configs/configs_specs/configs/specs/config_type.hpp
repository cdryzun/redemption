/*
SPDX-FileCopyrightText: 2025 Wallix Proxies Team

SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "configs/enumeration.hpp"

namespace cfg_specs {

inline void config_type_definition(type_enumerations & e)
{
    using EDescs = type_enumerations::DescriptionsAndInfo;
    using Descs = type_enumeration::Descriptions;

    using Opt = type_enumerations::DisplayNameOption;
    auto withNameWhenDescription = Opt::WithNameWhenDescription;
    auto withoutNameWhenDescription = Opt::WithoutNameWhenDescription;

    e.enumeration_list("ModuleName", withNameWhenDescription)
      .value("UNKNOWN")
      .value("login")
      .value("selector")
      .value("confirm")
      .value("link_confirm")
      .value("challenge")
      .value("valid")
      .value("transitory")
      .value("close")
      .value("close_back")
      .value("interactive_target")
      .value("RDP")
      .value("VNC")
      .value("INTERNAL")
      .value("waitinfo")
      .value("bouncer2")
      .value("autotest")
      .value("widgettest")
      .value("card")
    ;

    e.enumeration_flags("CaptureFlags", withNameWhenDescription, "Specifies the type of data to be captured:")
      .value("none")
      .value("png")
      .value("wrm", "Session recording file.")
      .reserved("video")
      .value("ocr")
    ;

    e.enumeration_list("RdpSecurityEncryptionLevel", withNameWhenDescription)
      .value("none")
      .value("low")
      .value("medium")
      .value("high")
    ;

    e.enumeration_list("Language", withNameWhenDescription)
      .value("en")
      .value("fr")
    ;

    e.enumeration_list("ClipboardEncodingType", withNameWhenDescription)
      .value("utf8").alias("utf-8")
      .value("latin1")
    ;

    e.enumeration_flags("KeyboardLogFlags", withoutNameWhenDescription)
      .value("none", Descs{
          .regular = "",
          .disabled = "Log all keyboard inputs",
        })
      .value("session_log", Descs{
          .regular = "",
          .disabled = "Exclude keyboard inputs from session logs (including SIEM)",
        })
      .value("wrm", Descs{
          .regular = "",
          .disabled = "Exclude keyboard inputs from recorded sessions",
        })
    ;

    e.enumeration_flags("ClipboardLogFlags", withoutNameWhenDescription)
      .value("none")
      .value("wrm", Descs{
          .regular = "",
          .disabled = "disable clipboard log in recorded sessions",
        })
      .value("meta", Descs{
          .regular = "",
          .disabled = "disable clipboard log in recorded meta",
        })
    ;

    e.enumeration_flags("FileSystemLogFlags", withoutNameWhenDescription)
      .value("none")
      .value("wrm", Descs{
          .regular = "",
          .disabled = "disable (redirected) file system log in recorded sessions",
        })
      .value("meta", Descs{
          .regular = "",
          .disabled = "disable (redirected) file system log in recorded meta",
        })
    ;

    e.enumeration_set("ColorDepth", withoutNameWhenDescription, "Specifies the maximum color depth for the client connection session:")
      .value("depth8", 8, "8-bit")
      .value("depth15", 15, "15-bit 555 RGB mask")
      .value("depth16", 16, "16-bit 565 RGB mask")
      .value("depth24", 24, "24-bit RGB mask")
      .value("depth32", 32, "32-bit RGB mask + alpha")
    ;

    e.enumeration_flags("ServerCertNotification", withNameWhenDescription)
      .value("nobody")
      .value("SIEM", "message sent to SIEM")
    ;

    e.enumeration_list("ServerCertCheck", withoutNameWhenDescription, EDescs{
        .desc = "Configure server certificate verification behavior.",
        .info = "Internal errors, such as failure to access a known certificate or decode it, always result in connection rejection."
    })
      .value("fails_if_no_match_or_missing", "Fails if the certificate is missing or does not match the known certificate.")
      .value("fails_if_no_match_and_succeed_if_no_know", "Fails if the certificate does not match the known certificate; succeeds if no certificate exists.")
      .value("succeed_if_exists_and_fails_if_missing", "Succeeds if a certificate exists (verification skipped); fails if no certificate exists.")
      .value("always_succeed", "Always succeeds without performing certificate validation.")
    ;

    e.enumeration_list("TraceType", withoutNameWhenDescription, EDescs{
        .desc = "Session record options.",
        .info = "When session records are encrypted, they can be read only by the WALLIX Bastion where they have been generated."
    })
      .value("localfile", "No encryption (faster).")
      .value("localfile_hashed", "No encryption, with checksum.")
      .value("cryptofile", "Encryption enabled.")
    ;

    e.enumeration_list("KeyboardInputMaskingLevel", withoutNameWhenDescription)
      .value("unmasked", "Log all input in plain text.")
      .value("password_only", "Log all input but mask passwords (without Session Probe, input is not logged).")
      .value("password_and_unidentified", "Log all input but mask passwords and unidentified input (without Session Probe, input is not logged).")
      .value("fully_masked", "Do not log input.")
    ;

    e.enumeration_list("SessionProbeOnLaunchFailure", withNameWhenDescription, "Behavior on failure to launch Session Probe.")
      .value("ignore_and_continue", "Proceed without Session Probe. Metadata collection is considered non-essential, and user experience is prioritized. The session will start in best-effort using 'Launch fallback timeout' instead of 'Launch timeout'.")
      .value("disconnect_user", "(recommended) End the session. A successful launch is expected when all technical prerequisites are met, reliability is prioritized. Adapt 'Launch timeout' value to the target performance.")
      .value("retry_without_session_probe", "Attempt to reconnect without Session Probe. This restores legacy behavior. The session will start using 'Launch fallback timeout' instead of 'Launch timeout'.")
    ;

    e.enumeration_list("VncBogusClipboardInfiniteLoop", withNameWhenDescription)
      .value("delayed", "Clipboard processing is deferred and, if necessary, the token is left with the client.")
      .value("duplicated", "When 2 identical requests are received, the second is ignored. This can block clipboard data reception until a clipboard event is triggered on the target server when the client clipboard is blocked, and vice versa.")
      .value("continued", "No special processing is done, the proxy always responds immediately.")
    ;

    e.enumeration_list("ColorDepthSelectionStrategy", withoutNameWhenDescription, "Color depth for the Session Recording file (.wrm):")
      .value("depth24", "24-bit")
      .value("depth16", "16-bit")
    ;

    e.enumeration_list("WrmCompressionAlgorithm", withoutNameWhenDescription, "Compression method of the Session Recording file (.wrm):")
      .value("no_compression")
      .value("gzip", "GZip: Files are better compressed, but this takes more time and CPU load.")
      .value("snappy", "Snappy: Faster than GZip, but files are less compressed.")
    ;

    e.enumeration_list("RdpCompression", withoutNameWhenDescription, "Specifies the highest RDP compression support available")
      .value("none", "The RDP bulk compression is disabled")
      .value("rdp4", "RDP 4.0 bulk compression")
      .value("rdp5", "RDP 5.0 bulk compression")
      .value("rdp6", "RDP 6.0 bulk compression")
      .value("rdp6_1", "RDP 6.1 bulk compression")
    ;

    e.enumeration_set("OcrVersion", withNameWhenDescription)
      .value("v1", 1)
      .value("v2", 2)
    ;

    e.enumeration_list("OcrLocale", withoutNameWhenDescription)
      .value("latin", "Recognizes Latin characters")
      .value("cyrillic", "Recognizes Latin and Cyrillic characters")
    ;

    e.enumeration_list("SessionProbeOnKeepaliveTimeout", withNameWhenDescription)
      .value("ignore_and_continue", "Proceed without Session Probe. Minimizes impact on user experience if Session Probe is unstable. Not recommended when Session Probe is working well. May be exploited by attackers simulating Session Probe failure to bypass surveillance.")
      .value("disconnect_user", "Legacy behavior. Prioritizes security but impacts user experience. RDP session may close (resulting in the permanent loss of all its unsaved elements) if the 'End disconnected session' parameter (or an equivalent setting at the RDS-level) is enabled.")
      .value("freeze_connection_and_wait", "(recommended) Block user actions until contact with the Session Probe is restored (e.g., reply to KeepAlive). Ensures session integrity during Session Probe outages.")
    ;

    e.enumeration_list("SmartVideoCropping", withNameWhenDescription)
      .value("disable", "When replaying the session video, the content of the RDP viewer matches the size of the client's desktop.")
      .value("v1", "When replaying the session video, the content of the RDP viewer is restricted to the greatest area covered by the application during the session.")
      .value("v2", "When replaying the session video, the content of the RDP viewer is fully covered by the size of the greatest application window during the session.")
    ;

    e.enumeration_list("RdpModeConsole", withoutNameWhenDescription)
      .value("allow", "Forward Console mode request from client to the target.")
      .value("force", "Force Console mode on target regardless of client request.")
      .value("forbid", "Block Console mode request from client.")
    ;

    e.enumeration_flags("SessionProbeDisabledFeature", withoutNameWhenDescription)
      .value("none")
      .value("jab", Descs{
          .regular = "",
          .disabled = "disable Java Access Bridge. General user activity monitoring in the Java applications (including detection of password fields)."
        })
      .value("msaa", Descs{
          .regular = "",
          .disabled = "disable MS Active Accessbility. General user activity monitoring (including detection of password fields). (legacy API)"
        })
      .value("msuia", Descs{
          .regular = "",
          .disabled = "disable MS UI Automation. General user activity monitoring (including detection of password fields). (new API)"
        })
      .invalid_value()
      .value("edge_inspection", Descs{
          .regular = "",
          .disabled = "disable Inspect Edge location URL. Basic web navigation monitoring."
        })
      .value("chrome_inspection", Descs{
          .regular = "",
          .disabled = "disable Inspect Chrome Address/Search bar. Basic web navigation monitoring."
        })
      .value("firefox_inspection", Descs{
          .regular = "",
          .disabled = "disable Inspect Firefox Address/Search bar. Basic web navigation monitoring."
        })
      .value("ie_monitoring", Descs{
          .regular = "",
          .disabled = "disable Monitor Internet Explorer event. Advanced web navigation monitoring."
        })
      .value("group_membership", Descs{
          .regular = "",
          .disabled = "disable Inspect group membership of user. User identity monitoring."
        })
    ;

    e.enumeration_list("RdpStoreFile", withNameWhenDescription)
      .value("never", "Never store transferred files.")
      .value("always", "Always store transferred files.")
      .value("on_invalid_verification", "Store transferred files only if file verification fails. Requires ICAP file verification (in section file_verification).")
    ;

    e.enumeration_list("SessionProbeOnAccountManipulation", withNameWhenDescription, "For targets running WALLIX BestSafe only.")
      .value("allow",  "User action will be accepted.")
      .value("notify", "(Same thing as 'allow'.)")
      .value("deny",   "User action will be rejected.")
    ;

    e.enumeration_list("ClientAddressSent", withoutNameWhenDescription, "Client Address to send to target(in InfoPacket)")
      .value("no_address", "Send 0.0.0.0")
      .value("proxy", "Send proxy client address or target connection.")
      .value("front", "Send user client address of front connection.")
    ;

    e.enumeration_list("SessionProbeLogLevel", withNameWhenDescription)
      .reserved("Off")
      .value("Fatal", "Severe error events that will likely cause the application to abort.")
      .value("Error", "Error events that may allow the application to continue running.")
      .value("Info", "General informational messages about application progress.")
      .value("Warning", "Potentially harmful situations that require attention.")
      .value("Debug", "Detailed informational events intended for debugging.")
      .value("Detail", "More detailed informational events than Debug for in-depth analysis.")
    ;

    e.enumeration_list("ModRdpUseFailureSimulationSocketTransport", withNameWhenDescription)
      .value("Off")
      .value("SimulateErrorRead")
      .value("SimulateErrorWrite")
    ;

    e.enumeration_list("LoginLanguage", withNameWhenDescription)
      .value("Auto", "The language is determined based on the keyboard layout specified by the client.")
      .value("EN")
      .value("FR")
    ;

    e.enumeration_list("VncTunnelingType", withNameWhenDescription)
        .value("pxssh")
        .value("pexpect")
        .value("popen")
    ;

    e.enumeration_list("VncTunnelingCredentialSource", withNameWhenDescription)
        .value("static_login")
        .value("scenario_account")
    ;

    e.enumeration_list("BannerType", withNameWhenDescription)
      .value("info")
      .value("warn")
      .value("alert")
    ;

    e.enumeration_list("SessionProbeCPUUsageAlarmAction", withoutNameWhenDescription)
      .value("Restart", "Restart Session Probe. May result in session disconnection due to loss of KeepAlive messages! Refer to 'On keepalive timeout' parameter of current section and 'Allow multiple handshakes' parameter of 'Configuration options'.")
      .value("Stop", "Stop Session Probe. May result in session disconnection due to loss of KeepAlive messages! Refer to 'On keepalive timeout' parameter of current section.")
    ;

    e.enumeration_list("SessionProbeProcessCommandLineRetrieveMethod", withNameWhenDescription)
      .value("windows_management_instrumentation", "Get command-line of processes via Windows Management Instrumentation. (Legacy method)")
      .value("windows_internals", "Calling internal system APIs to get the process command line. (More efficient but less stable)")
      .value("both", "First use internal system APIs call, if that fails, use Windows Management Instrumentation method.")
    ;

    e.enumeration_list("RdpSaveSessionInfoPDU", withoutNameWhenDescription)
        .value("Supported", "Windows")
        .value("UnsupportedOrUnknown", "Bastion, xrdp, or others")
    ;

    e.enumeration_flags("SessionLogFormat", withNameWhenDescription)
        .value("disabled")
        .value("SIEM")
        .value("ArcSight")
    ;
}

}
