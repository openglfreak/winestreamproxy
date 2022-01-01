/* Copyright (C) 2020 Torge Matthies
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * Author contact info:
 *   E-Mail address: openglfreak@googlemail.com
 *   PGP key fingerprint: 0535 3830 2F11 C888 9032 FAD2 7C95 CD70 C9E8 438D */

#include "connection_list.h"
#include "misc.h"
#include "../bool.h"
#include <winestreamproxy/logger.h>

#include <assert.h>

#include <tchar.h>
#include <windef.h>
#include <winbase.h>
#include <winnt.h>

/* This implementation is not 100% thread-safe, but it suffices for our use-case. */

bool connection_list_initialize(logger_instance* const logger, connection_list* const connection_list)
{
    LOG_TRACE(logger, (_T("Initializing connection list")));

    InitializeCriticalSection(&connection_list->lock);

    LOG_TRACE(logger, (_T("Initialized connection list")));

    return true;
}

bool connection_list_allocate_entry(logger_instance* const logger, connection_list* const connection_list,
                                    connection_data** const out_connection)
{
    connection_list_entry* entry, * end;

    LOG_TRACE(logger, (_T("Allocating connection object")));

    EnterCriticalSection(&connection_list->lock);

    entry = (connection_list_entry*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(connection_list_entry));
    if (!entry)
    {
        LOG_CRITICAL(logger, (_T("Could not allocate connection data (%lu bytes)"), sizeof(connection_list_entry)));
        return FALSE;
    }

    end = connection_list->end;
    entry->previous = end;
    if (!end)
        connection_list->start = entry;
    else
        end->next = entry;
    connection_list->end = entry;

    LeaveCriticalSection(&connection_list->lock);

    LOG_TRACE(logger, (_T("Allocated connection object")));

    *out_connection = &entry->connection;
    return TRUE;
}

void connection_list_deallocate_entry(logger_instance* const logger, connection_list* const connection_list,
                                      connection_data* const connection)
{
    connection_list_entry* entry =
        (connection_list_entry*)((char*)connection - offsetof(connection_list_entry, connection));

    LOG_TRACE(logger, (_T("Deallocating connection object")));

    EnterCriticalSection(&connection_list->lock);

    if (connection_list->start == entry)
        connection_list->start = entry->next;
    if (connection_list->end == entry)
        connection_list->end = entry->previous;

    if (entry->previous)
        entry->previous->next = entry->next;
    if (entry->next)
        entry->next->previous = entry->previous;

    LeaveCriticalSection(&connection_list->lock);

    HeapFree(GetProcessHeap(), 0, entry);

    LOG_TRACE(logger, (_T("Deallocated connection object")));
}

void connection_list_deallocate(logger_instance* logger, connection_list* const connection_list)
{
    connection_list_entry* start;

    LOG_TRACE(logger, (_T("Deallocating connection list")));

    EnterCriticalSection(&connection_list->lock);

    start = connection_list->start;
    connection_list->start = 0;
    connection_list->end = 0;

    LeaveCriticalSection(&connection_list->lock);

    assert(!start || !start->previous);

    while (start)
    {
        connection_list_entry* next = start->next;
        HeapFree(GetProcessHeap(), 0, start);
        start = next;
    }

    LOG_TRACE(logger, (_T("Deallocated connection list")));
}

void connection_list_finalize(logger_instance* const logger, connection_list* const connection_list)
{
    LOG_TRACE(logger, (_T("Finalizing connection list")));

    DeleteCriticalSection(&connection_list->lock);

    LOG_TRACE(logger, (_T("Finalized connection list")));
}
