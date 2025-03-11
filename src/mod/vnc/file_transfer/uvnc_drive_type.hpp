/*
SPDX-FileCopyrightText: 2025 Wallix Proxies Team
SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <cstdint>


namespace UVNC::FileTransfer
{

// UltraVNC drive type name.
// See GetDriveType (Windows API) for type of drive.
enum class DriveType : uint8_t
{
    // The drive has fixed media; for example, a hard disk drive or flash drive.
    LocalDisk = 'l',
    // The drive has removable media;
    // for example, a floppy drive, thumb drive, or flash card reader.
    MediaDisk = 'f',
    // The drive is a CD-ROM drive.
    CDRom = 'c',
    // The drive is a remote (network) drive.
    NetworkDisk = 'n',
    // Unknown, NoRootDir or RAM disk
    Other = '\\',
};

} // namespace UVNC::FileTransfer
