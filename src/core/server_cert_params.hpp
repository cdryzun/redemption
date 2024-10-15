/*
SPDX-FileCopyrightText: 2024 Wallix Proxies Team
SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "configs/autogen/enums.hpp"

struct ServerCertNotifications
{
    ServerCertNotification access_allowed_message = ServerCertNotification::SIEM;
    ServerCertNotification create_message = ServerCertNotification::SIEM;
    ServerCertNotification success_message = ServerCertNotification::SIEM;
    ServerCertNotification failure_message = ServerCertNotification::SIEM;
    ServerCertNotification error_message = ServerCertNotification::SIEM;
};

struct ServerCertParams
{
    bool store = true;
    ServerCertCheck check = ServerCertCheck::fails_if_no_match_or_missing;
    ServerCertNotifications notifications;
};
