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
#include <winestreamproxy/logger.h>

#include <assert.h>

#include <tchar.h>
#include <windef.h>
#include <winbase.h>
#include <winnt.h>

/* This implementation is not 100% thread-safe, but it suffices for our use-case. */

BOOL connection_list_allocate_entry(logger_instance* const logger, connection_list* const connection_list,
                                    connection_data** const out_connection)
{
    connection_list_entry* entry;

    LOG_TRACE(logger, (_T("Allocating connection object")));

    entry = (connection_list_entry*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(connection_list_entry));
    if (!entry)
    {
        LOG_CRITICAL(logger, (_T("Could not allocate connection data (%lu bytes)"), sizeof(connection_list_entry)));
        return FALSE;
    }

    while (TRUE)
    {
        connection_list_entry* end;

        end = ((struct connection_list volatile*)connection_list)->end;

        if (!end)
        {
            if (InterlockedCompareExchangePointer((PVOID volatile*)&connection_list->start, entry, 0))
                continue;
            if (InterlockedCompareExchangePointer((PVOID volatile*)&connection_list->end, entry, /*end*/ 0)/* != end*/)
            {
                InterlockedCompareExchangePointer((PVOID volatile*)&connection_list->start, 0, entry);
                assert(!entry->previous && !entry->next); /* If entry has been modified, all hope is lost. */
                continue;
            }
        }
        else
        {
            entry->previous = end;

            if (InterlockedCompareExchangePointer((PVOID volatile*)&end->next, entry, 0))
                continue;
            if (InterlockedCompareExchangePointer((PVOID volatile*)&connection_list->end, entry, end) != end)
            {
                InterlockedCompareExchangePointer((PVOID volatile*)&end->next, 0, entry);
                assert(!entry->next); /* If entry has been modified, all hope is lost. */
                continue;
            }
        }
        break;
    }

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

    if (connection_list->start == entry)
        InterlockedCompareExchangePointer((PVOID volatile*)&connection_list->start, entry->next, entry);
    if (connection_list->end == entry)
        InterlockedCompareExchangePointer((PVOID volatile*)&connection_list->end, entry->previous, entry);

    if (entry->previous)
        InterlockedCompareExchangePointer((PVOID volatile*)&entry->previous->next, entry->next, entry);
    if (entry->next)
        InterlockedCompareExchangePointer((PVOID volatile*)&entry->next->previous, entry->previous, entry);

    HeapFree(GetProcessHeap(), 0, entry);

    LOG_TRACE(logger, (_T("Deallocated connection object")));
}

void connection_list_deallocate(logger_instance* logger, connection_list* const connection_list)
{
    connection_list_entry* start, * end, * entry;

    LOG_TRACE(logger, (_T("Deallocating connection list")));

    do {
        start = ((struct connection_list volatile*)connection_list)->start;
    } while(InterlockedCompareExchangePointer((PVOID volatile*)&connection_list->start, 0, start) != start);
    do {
        end = ((struct connection_list volatile*)connection_list)->end;
    } while (InterlockedCompareExchangePointer((PVOID volatile*)&connection_list->end, 0, end) != end);

    assert(!start || !start->previous);
    assert(!end || !end->next);

    entry = start;
    while (entry)
    {
        connection_list_entry* next;

        next = entry->next;
        if (next)
        {
            connection_list_entry* next_prev;

            next_prev =
                (connection_list_entry*)InterlockedCompareExchangePointer((PVOID volatile*)&next->previous, 0, entry);
            (void)next_prev;
            assert(next_prev == entry);
        }
        HeapFree(GetProcessHeap(), 0, entry);
        entry = next;
    }

    LOG_TRACE(logger, (_T("Deallocated connection list")));
}
