/* Copyright (C) 2020 Torge Matthies
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * Author contact info:
 *   E-Mail address: openglfreak@googlemail.com
 *   PGP key fingerprint: 0535 3830 2F11 C888 9032 FAD2 7C95 CD70 C9E8 438D */

#ifdef __linux__
#define socket_use_eventfd
#endif

#include "socket.h"

#include <assert.h>
#include <errno.h>
#include <string.h>

#include <poll.h>
#ifdef socket_use_eventfd
#include <sys/eventfd.h>
#else
#include <fcntl.h>
#endif
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

size_t socket_get_address_struct_size(void) { return sizeof(struct sockaddr_un); }
size_t socket_get_max_path_length(void) { return sizeof(((struct sockaddr_un*)0)->sun_path) - 1; }
void socket_init_address(void* const address_struct, char const* const path, size_t const path_len)
{
    struct sockaddr_un* const addr = (struct sockaddr_un*)address_struct;

    memset(addr, 0, sizeof(*addr));
    addr->sun_family = AF_UNIX;
    assert(path_len < sizeof(addr->sun_path));
    memcpy(addr->sun_path, path, path_len);
}

int socket_create(int* const out_socket)
{
    int _socket;

    _socket = socket(AF_UNIX, SOCK_STREAM, 0);
    if (_socket == -1)
        return errno ? errno : -1;
    *out_socket = _socket;
    return 0;
}
void socket_close(int const socket) { close(socket); }

int socket_create_thread_exit_signal(thread_exit_signal* const out_signal)
{
#ifdef socket_use_eventfd
    if ((out_signal->fds[0] = eventfd(0, EFD_CLOEXEC)) == -1)
        return errno ? errno : -1;
#else
    if (pipe(out_signal->fds) == -1)
        return errno ? errno : -1;
    fcntl(out_signal->fds[0], F_SETFD, FD_CLOEXEC);
    fcntl(out_signal->fds[1], F_SETFD, FD_CLOEXEC);
#endif
    return 0;
}
void socket_close_thread_exit_signal(thread_exit_signal const signal)
{
#ifdef socket_use_eventfd
    close(signal.fds[0]);
#else
    close(signal.fds[0]);
    close(signal.fds[1]);
#endif
}

int socket_connect_unix(int const socket, void const* const address_struct)
{
    if (connect(socket, (struct sockaddr*)address_struct, sizeof(struct sockaddr_un)) != 0)
        return errno ? errno : -1;
    return 0;
}

int socket_set_thread_exit_signal(thread_exit_signal const signal)
{
#ifdef socket_use_eventfd
    static char one[8] = { 0, 0, 0, 0, 0, 0, 0, 1 };
    int const fd = signal.fds[0];
#else
    static char one[1] = { 1 };
    int const fd = signal.fds[1];
#endif

    if (write(fd, one, sizeof(one)) != sizeof(one))
        return errno ? errno : -1;
    return 0;
}

int socket_poll(int const socket, thread_exit_signal const signal, poll_status* const out_status)
{
    struct pollfd fds[2];
    int nfds;

    fds[0].fd = socket;
    fds[0].events = POLLIN | POLLPRI | POLLHUP;
    fds[0].revents = 0;
    fds[1].fd = signal.fds[0];
    fds[1].events = POLLIN | POLLPRI | POLLHUP;
    fds[1].revents = 0;
    while ((nfds = poll(fds, sizeof(fds) / sizeof(fds[0]), -1)) == 0 || errno == EAGAIN || errno == EINTR);
    if (nfds == -1)
        return errno ? errno : -1;

    if (fds[1].revents)
        *out_status = POLL_STATUS_EXIT_SIGNAL;
    else if (fds[0].revents)
    {
        if (fds[0].revents & POLLHUP)
            *out_status = POLL_STATUS_CLOSED_CONNECTION;
        if (!(fds[0].revents & (POLLIN | POLLPRI)))
            *out_status = POLL_STATUS_INVALID_MESSAGE_FLAGS;
    }
    else
        *out_status = POLL_STATUS_SUCCESS;

    return 0;
}

int socket_recv(int const socket, unsigned char* const buffer, size_t const buffer_size, recv_status* const out_status,
                size_t* const out_message_length)
{
    ssize_t recv_ret;

    recv_ret = recv(socket, buffer, buffer_size, MSG_PEEK);
    if (recv_ret == -1)
        return errno ? errno : -1;

    if ((size_t)recv_ret >= buffer_size)
    {
        *out_status = RECV_STATUS_INSUFFICIENT_BUFFER;
        return 0;
    }

    recv_ret = recv(socket, buffer, buffer_size, 0);
    if (recv_ret == -1)
        return errno ? errno : -1;

    *out_status = (size_t)recv_ret >= buffer_size ? RECV_STATUS_DISCARDED_DATA : RECV_STATUS_SUCCESS;
    *out_message_length = (size_t)recv_ret;
    return 0;
}

int socket_write(int const socket, unsigned char const* const message, size_t const message_length, size_t* const written)
{
    ssize_t bytes_written;

    bytes_written = write(socket, message, message_length);
    if (bytes_written == -1)
        return errno ? errno : -1;

    *written = (size_t)bytes_written;
    return 0;
}
