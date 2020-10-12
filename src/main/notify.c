/* Copyright (C) 2020 Torge Matthies
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * Author contact info:
 *   E-Mail address: openglfreak@googlemail.com
 *   PGP key fingerprint: 0535 3830 2F11 C888 9032 FAD2 7C95 CD70 C9E8 438D */

#include "misc.h"
#include <winestreamproxy/logger.h>

#include <errno.h>
#include <stddef.h>

#include <fcntl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <tchar.h>
#include <unistd.h>
#include <windef.h>
#include <winbase.h>
#include <winnls.h>

#define NOTIFICATION_SOCKET_ENVVAR_NAME _T("__WINESTREAMPROXY_NOTIFY_SOCKET")

int notify_socket_fd = -1;

BOOL init_socket_notification(logger_instance* const logger)
{
#ifdef _UNICODE
    TCHAR* socket_path;
    size_t socket_path_length;
    int socket_path_length2;
#else
    DWORD socket_path_length;
#endif
    struct sockaddr_un addr;

#ifdef _UNICODE
    if (!get_envvar(NOTIFICATION_SOCKET_ENVVAR_NAME, &socket_path, &socket_path_length) || !socket_path
        || !*socket_path)
        return FALSE;
    socket_path_length2 = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, socket_path, -1, addr.sun_path,
                                              sizeof(addr.sun_path), NULL, NULL);
    if (socket_path_length2 == 0)
        return FALSE;
#else
    socket_path_length = GetEnvironmentVariableA(NOTIFICATION_SOCKET_ENVVAR_NAME, addr.sun_path, sizeof(addr.sun_path));
    if (socket_path_length >= sizeof(addr.sun_path) || socket_path_length <= 0)
        return FALSE;
#endif

    LOG_TRACE(logger, (_T("Opening notification socket")));

    notify_socket_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (notify_socket_fd == -1)
    {
        LOG_ERROR(logger, (_T("Could not create notification socket: Error %d"), errno));
#ifdef _UNICODE
        HeapFree(GetProcessHeap(), 0, socket_path);
#endif
        return FALSE;
    }

    addr.sun_family = AF_UNIX;
    if (connect(notify_socket_fd, (struct sockaddr*)&addr, sizeof(addr)) != 0)
    {
        LOG_ERROR(logger, (_T("Could not connect to notification socket: Error %d"), errno));
        close(notify_socket_fd);
#ifdef _UNICODE
        HeapFree(GetProcessHeap(), 0, socket_path);
#endif
        return FALSE;
    }

#ifdef _UNICODE
    HeapFree(GetProcessHeap(), 0, socket_path);
#endif

    LOG_TRACE(logger, (_T("Opened notification socket")));

    return TRUE;
}

void socket_notify(logger_instance* const logger, unsigned char const code)
{
    if (notify_socket_fd != -1)
    {
        LOG_TRACE(logger, (_T("Sending notification to socket (code %ud)"), (unsigned int)code));

        if (write(notify_socket_fd, &code, 1) != 1)
            LOG_ERROR(logger, (_T("Failed to send notification to socket: Error %d"), errno));
        else
            LOG_TRACE(logger, (_T("Sent notification to socket (code %ud)"), (unsigned int)code));
        close(notify_socket_fd);
        notify_socket_fd = -1;
    }
}

void fini_socket_notification(logger_instance* const logger)
{
    socket_notify(logger, 0);
}
