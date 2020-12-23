/* Copyright (C) 2020 Torge Matthies
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * Author contact info:
 *   E-Mail address: openglfreak@googlemail.com
 *   PGP key fingerprint: 0535 3830 2F11 C888 9032 FAD2 7C95 CD70 C9E8 438D */

#pragma once
#ifndef __WINESTREAMPROXY_PROXY_UNIXLIB_SOCKET_H__
#define __WINESTREAMPROXY_PROXY_UNIXLIB_SOCKET_H__

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif /* defined(__cplusplus) */

typedef struct thread_exit_signal {
    int fds[2];
} thread_exit_signal;

typedef enum poll_status {
    POLL_STATUS_SUCCESS,
    POLL_STATUS_EXIT_SIGNAL,
    POLL_STATUS_CLOSED_CONNECTION,
    POLL_STATUS_INVALID_MESSAGE_FLAGS
} poll_status;

typedef enum recv_status {
    RECV_STATUS_SUCCESS,
    RECV_STATUS_INSUFFICIENT_BUFFER,
    RECV_STATUS_DISCARDED_DATA
} recv_status;

#if __WINE__
#define SOCKET_DLLIMPORT /*__declspec(dllexport)*/
#else
#define SOCKET_DLLIMPORT __declspec(dllimport)
#endif

extern size_t SOCKET_DLLIMPORT socket_get_address_struct_size(void);
extern size_t SOCKET_DLLIMPORT socket_get_max_path_length(void);
extern void SOCKET_DLLIMPORT socket_init_address(void* address_struct, char const* path, size_t path_len);
extern int SOCKET_DLLIMPORT socket_create(int* out_socket);
extern void SOCKET_DLLIMPORT socket_close(int socket);
extern int SOCKET_DLLIMPORT socket_create_thread_exit_signal(thread_exit_signal* out_signal);
extern void SOCKET_DLLIMPORT socket_close_thread_exit_signal(thread_exit_signal signal);
extern int SOCKET_DLLIMPORT socket_connect_unix(int socket, void const* address_struct);
extern int SOCKET_DLLIMPORT socket_set_thread_exit_signal(thread_exit_signal signal);
extern int SOCKET_DLLIMPORT socket_poll(int socket, thread_exit_signal signal, poll_status* out_status);
extern int SOCKET_DLLIMPORT socket_recv(int socket, unsigned char* buffer, size_t buffer_size, recv_status* out_status,
                                        size_t* out_message_length);
extern int SOCKET_DLLIMPORT socket_write(int socket, unsigned char const* message, size_t message_length,
                                         size_t* written);

#ifdef __cplusplus
}
#endif /* defined(__cplusplus) */

#endif /* !defined(__WINESTREAMPROXY_PROXY_UNIXLIB_SOCKET_H__) */
