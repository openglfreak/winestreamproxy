/* Copyright (C) 2020 Torge Matthies
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * Author contact info:
 *   E-Mail address: openglfreak@googlemail.com
 *   PGP key fingerprint: 0535 3830 2F11 C888 9032 FAD2 7C95 CD70 C9E8 438D */

#include "proxy.h"
#include <winestreamproxy/logger.h>

#include <assert.h>
#include <stddef.h>

#include <tchar.h>
#include <windef.h>
#include <winbase.h>
#include <winnt.h>

BOOL create_proxy(logger_instance* const logger, connection_paths const paths, HANDLE const exit_event,
                  proxy_running_callback const running_callback, proxy_data** const out_proxy)
{
    proxy_data* proxy;

    LOG_TRACE(logger, (_T("Creating proxy object")));

    proxy = (proxy_data*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(proxy_data));
    if (!proxy)
    {
        LOG_CRITICAL(logger, (_T("Could not allocate proxy object (%lu bytes)"), sizeof(proxy_data)));
        return FALSE;
    }

    proxy->logger = logger;
    proxy->paths = paths;
    proxy->exit_event = exit_event;
    proxy->running_callback = running_callback;

    proxy->connect_overlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (!proxy->connect_overlapped.hEvent)
    {
        LOG_CRITICAL(logger, (_T("Could not create an event for asynchronous connecting")));
        HeapFree(GetProcessHeap(), 0, proxy);
        return FALSE;
    }

    LOG_TRACE(logger, (_T("Created proxy object")));

    *out_proxy = proxy;
    return TRUE;
}

void destroy_proxy(proxy_data* const proxy)
{
    logger_instance* logger;
    connection_list_entry* entry;

    logger = proxy->logger;

    LOG_TRACE(logger, (_T("Destroying proxy object")));

    assert(!proxy->connections_start || !proxy->connections_start->previous);
    assert(!proxy->connections_end || !proxy->connections_end->next);

    entry = proxy->connections_start;
    while (entry)
    {
        connection_list_entry* next;

        next = entry->next;
        HeapFree(GetProcessHeap(), 0, entry);
        entry = next;
    }

    CloseHandle(proxy->connect_overlapped.hEvent);

    HeapFree(GetProcessHeap(), 0, proxy);

    LOG_TRACE(logger, (_T("Destroyed proxy object")));
}

/* Not thread-safe, but doesn't need to be. */
BOOL allocate_connection(proxy_data* const proxy, connection_data** const out_connection)
{
    connection_list_entry* entry;

    LOG_TRACE(proxy->logger, (_T("Allocating connection object")));

    entry = (connection_list_entry*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(connection_list_entry));
    if (!entry)
    {
        LOG_CRITICAL(proxy->logger, (
            _T("Could not allocate connection data (%lu bytes)"),
            sizeof(connection_list_entry)
        ));
        return FALSE;
    }

    if (!proxy->connections_end)
    {
        assert(!proxy->connections_start);
        proxy->connections_start = entry;
        proxy->connections_end = entry;
    }
    else
    {
        assert(proxy->connections_start);
        assert(!proxy->connections_end->next);
        entry->previous = proxy->connections_end;
        proxy->connections_end->next = entry;
    }

    LOG_TRACE(proxy->logger, (_T("Allocated connection object")));

    *out_connection = &entry->connection;
    return TRUE;
}

/* Also not thread-safe. */
void deallocate_connection(proxy_data* const proxy, connection_data* const connection)
{
#   define entry ((connection_list_entry*)((char*)connection - offsetof(connection_list_entry, connection)))

    LOG_TRACE(proxy->logger, (_T("Deallocating connection object")));

    if (proxy->connections_start == entry)
        proxy->connections_start = entry->next;
    if (proxy->connections_end == entry)
        proxy->connections_end = entry->previous;

    if (entry->previous)
        entry->previous->next = entry->next;
    if (entry->next)
        entry->next->previous = entry->previous;

    HeapFree(GetProcessHeap(), 0, entry);

    LOG_TRACE(proxy->logger, (_T("Deallocated connection object")));

#   undef entry
}
