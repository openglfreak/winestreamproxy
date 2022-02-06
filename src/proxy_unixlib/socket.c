/* Copyright (C) 2021 Torge Matthies
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
#endif
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>
#include <fcntl.h>

size_t SOCKUNIXAPI socket_get_address_struct_size(void)
{
    return sizeof(struct sockaddr_un);
}

size_t SOCKUNIXAPI socket_get_max_path_length(void)
{
    return sizeof(((struct sockaddr_un*)0)->sun_path) - 1;
}

void SOCKUNIXAPI socket_init_address(void* const address_struct, char const* const path, size_t const path_len)
{
    struct sockaddr_un* const addr = (struct sockaddr_un*)address_struct;
    memset(addr, 0, sizeof(*addr));
    addr->sun_family = AF_UNIX;
    assert(path_len < sizeof(addr->sun_path));
    memcpy(addr->sun_path, path, path_len);
}

int SOCKUNIXAPI socket_create(int* const out_socket)
{
    int s = socket(AF_UNIX, SOCK_STREAM, 0);
    if (s == -1)
        return errno ? errno : -1;
    *out_socket = s;
    return 0;
}

void SOCKUNIXAPI socket_close(int const socket)
{
    close(socket);
}

int SOCKUNIXAPI socket_create_thread_exit_event(thread_exit_event* const out_event)
{
    int pfds[2];
#ifdef socket_use_eventfd
    int const efd = eventfd(0, EFD_CLOEXEC);
    if (efd != -1)
    {
        out_event->fds[0] = efd;
        out_event->fds[1] = efd;
        return 0;
    }
#endif
    if (pipe(pfds) != -1)
    {
        fcntl(pfds[0], F_SETFD, FD_CLOEXEC);
        fcntl(pfds[0], F_SETFD, FD_CLOEXEC);
        out_event->fds[0] = pfds[0];
        out_event->fds[1] = pfds[1];
        return 0;
    }
    return errno ? errno : -1;
}

void SOCKUNIXAPI socket_close_thread_exit_event(thread_exit_event const event)
{
    close(event.fds[0]);
    if (event.fds[1] != event.fds[0])
        close(event.fds[1]);
}

int SOCKUNIXAPI socket_send_thread_exit_event(thread_exit_event const event)
{
    static char one[8] = { 0, 0, 0, 0, 0, 0, 0, 1 };
    if (write(event.fds[1], one, sizeof(one)) <= 0)
        return errno ? errno : -1;
    return 0;
}

int SOCKUNIXAPI socket_connect(int const socket, void const* const address_struct)
{
    if (connect(socket, (struct sockaddr const*)address_struct, sizeof(struct sockaddr_un)) != 0)
        return errno ? errno : -1;
    return 0;
}

int SOCKUNIXAPI socket_poll(int const socket, thread_exit_event const event, poll_status* const out_status)
{
    struct pollfd fds[2];
    int nfds;

    fds[0].fd = socket;
    fds[0].events = POLLIN | POLLPRI | POLLHUP;
    fds[0].revents = 0;
    fds[1].fd = event.fds[0];
    fds[1].events = POLLIN | POLLPRI | POLLHUP;
    fds[1].revents = 0;
    while ((nfds = poll(fds, sizeof(fds) / sizeof(fds[0]), -1)) == 0 ||
           (nfds == -1 && (errno == EAGAIN || errno == EINTR)));
    if (nfds == -1)
        return errno ? errno : -1;

    if (fds[1].revents)
        *out_status = POLL_STATUS_EXIT_SIGNALED;
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

int SOCKUNIXAPI socket_recv(int const socket, unsigned char* const buffer, size_t const buffer_size,
                            recv_status* const out_status, size_t* const out_message_length)
{
    ssize_t recv_ret = recv(socket, buffer, buffer_size, MSG_PEEK);
    if (recv_ret == -1)
        return errno ? errno : -1;

    if ((size_t)recv_ret >= buffer_size)
    {
        *out_status = RECV_STATUS_INSUFFICIENT_BUFFER;
        *out_message_length = (size_t)recv_ret;
        return 0;
    }

    recv_ret = recv(socket, buffer, buffer_size, 0);
    if (recv_ret == -1)
        return errno ? errno : -1;

    *out_status = (size_t)recv_ret >= buffer_size ? RECV_STATUS_DISCARDED_DATA : RECV_STATUS_SUCCESS;
    *out_message_length = (size_t)recv_ret;
    return 0;
}

int SOCKUNIXAPI socket_send(int const socket, unsigned char const* const message, size_t const message_length,
                            size_t* const written)
{
    ssize_t bytes_written = write(socket, message, message_length);
    if (bytes_written == -1)
        return errno ? errno : -1;
    *written = (size_t)bytes_written;
    return 0;
}

#ifdef __cplusplus
extern "C"
#endif /* defined(__cplusplus) */
int SOCKUNIXAPI socket_unix_init(socket_unix_funcs* const out_funcs)
{
    out_funcs->get_address_struct_size = socket_get_address_struct_size;
    out_funcs->get_max_path_length = socket_get_max_path_length;
    out_funcs->init_address = socket_init_address;
    out_funcs->create = socket_create;
    out_funcs->close = socket_close;
    out_funcs->create_thread_exit_event = socket_create_thread_exit_event;
    out_funcs->close_thread_exit_event = socket_close_thread_exit_event;
    out_funcs->send_thread_exit_event = socket_send_thread_exit_event;
    out_funcs->connect = socket_connect;
    out_funcs->poll = socket_poll;
    out_funcs->recv = socket_recv;
    out_funcs->send = socket_send;
    return 0;
}
