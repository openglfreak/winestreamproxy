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

typedef struct thread_exit_event {
    int fds[2];
} thread_exit_event;

typedef enum poll_status {
    POLL_STATUS_SUCCESS,
    POLL_STATUS_EXIT_SIGNALED,
    POLL_STATUS_CLOSED_CONNECTION,
    POLL_STATUS_INVALID_MESSAGE_FLAGS
} poll_status;

typedef enum recv_status {
    RECV_STATUS_SUCCESS,
    RECV_STATUS_INSUFFICIENT_BUFFER,
    RECV_STATUS_DISCARDED_DATA
} recv_status;

#define SOCKUNIXAPI __stdcall

typedef struct socket_unix_funcs {
    size_t SOCKUNIXAPI (*get_address_struct_size)(void);
    size_t SOCKUNIXAPI (*get_max_path_length)(void);
    void SOCKUNIXAPI (*init_address)(void* address_struct, char const* path, size_t path_len);

    int SOCKUNIXAPI (*create)(int* out_socket);
    void SOCKUNIXAPI (*close)(int socket);

    int SOCKUNIXAPI (*create_thread_exit_event)(thread_exit_event* out_event);
    void SOCKUNIXAPI (*close_thread_exit_event)(thread_exit_event event);
    int SOCKUNIXAPI (*send_thread_exit_event)(thread_exit_event event);

    int SOCKUNIXAPI (*connect)(int socket, void const* address_struct);
    int SOCKUNIXAPI (*poll)(int socket, thread_exit_event event, poll_status* out_status);
    int SOCKUNIXAPI (*recv)(int socket, unsigned char* buffer, size_t buffer_size, recv_status* out_status,
                            size_t* out_message_length);
    int SOCKUNIXAPI (*send)(int socket, unsigned char const* message, size_t message_length, size_t* written);
} socket_unix_funcs;

typedef int SOCKUNIXAPI (*socket_unix_init_t)(socket_unix_funcs* out_funcs);

#ifdef __cplusplus
}
#endif /* defined(__cplusplus) */

#endif /* !defined(__WINESTREAMPROXY_PROXY_UNIXLIB_SOCKET_H__) */
