/*
 * Copyright (c) 2009 Mark Heily <mark@heily.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */


#include "common.h"

/*
 * Create a connected TCP socket.
 */
static void
create_socket_connection(int *client, int *server)
{
    struct sockaddr_in sain;
    int one = 1;
    int clnt, srvr, accepted;

    /* Create a passive socket */
    memset(&sain, 0, sizeof(sain));
    sain.sin_family = AF_INET;
    if ((srvr = socket(PF_INET, SOCK_STREAM, 0)) < 0)
        FAIL() << "socket: " << strerror(errno);
    if (setsockopt(srvr, SOL_SOCKET, SO_REUSEADDR, (char *)&one, sizeof(one)) !=
        0)
        FAIL() << "setsockopt: " << strerror(errno);
    if (bind(srvr, (const struct sockaddr *)&sain, sizeof(sain)) < 0) {
        FAIL() << "bind: " << strerror(errno);
    }
    if (listen(srvr, 100) < 0)
        FAIL() << "listen: " << strerror(errno);

    // Fill in the port number
    socklen_t addrlen = sizeof(sain);
    if (getsockname(srvr, (struct sockaddr *)&sain, &addrlen))
        FAIL() << "getsockname: " << strerror(errno);

    /* Simulate a client connecting to the server */
    sain.sin_family = AF_INET;
    sain.sin_addr.s_addr = inet_addr("127.0.0.1");
    if ((clnt = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        FAIL() << "clnt: socket: " << strerror(errno);
    if (connect(clnt, (const struct sockaddr *)&sain, sizeof(sain)) < 0)
        FAIL() << "clnt: connect: " << strerror(errno);
    if ((accepted = accept(srvr, NULL, 0)) < 0)
        FAIL() << "srvr: accept: " << strerror(errno);

    *client = clnt;
    *server = accepted;
}

static void
kevent_socket_drain(struct test_context *ctx)
{
    char buf[1];

    /* Drain the read buffer, then make sure there are no more events. */
    if (recv(ctx->client_fd, &buf[0], 1, 0) < 1)
        FAIL() << "recv(2)";
}

static void
kevent_socket_fill(struct test_context *ctx)
{
    if (send(ctx->server_fd, ".", 1, 0) < 1)
        FAIL() << "send(2)";
}


void
test_kevent_socket_add(struct test_context *ctx)
{
    struct kevent kev;

    kev = KEventCreate(ctx->client_fd, EVFILT_READ, EV_ADD);
    EXPECT_EQ(0, kevent(ctx->kqfd, &kev, 1, NULL, 0, NULL)) << strerror(errno)
                                                            << " - " << kev;
}

void
test_kevent_socket_add_without_ev_add(struct test_context *ctx)
{
    struct kevent kev;

    /* Try to add a kevent without specifying EV_ADD */
    EV_SET(&kev, ctx->client_fd, EVFILT_READ, 0, 0, 0, &ctx->client_fd);
    if (kevent(ctx->kqfd, &kev, 1, NULL, 0, NULL) == 0)
        FAIL() << "kevent should have failed";

    kevent_socket_fill(ctx);
    EXPECT_NO_EVENT(ctx->kqfd);
    kevent_socket_drain(ctx);

    /* Try to delete a kevent which does not exist */
    kev.flags = EV_DELETE;
    if (kevent(ctx->kqfd, &kev, 1, NULL, 0, NULL) == 0)
        FAIL() << "kevent should have failed";
}

void
test_kevent_socket_get(struct test_context *ctx)
{
    struct kevent kev, ret;

    EV_SET(&kev, ctx->client_fd, EVFILT_READ, EV_ADD, 0, 0, &ctx->client_fd);
    if (kevent(ctx->kqfd, &kev, 1, NULL, 0, NULL) < 0)
        FAIL() << "kevent:1";

    kevent_socket_fill(ctx);

    kev.data = 1;
    EXPECT_EVENT(ctx->kqfd, &ret);
    EXPECT_EQ(kev, ret);

    kevent_socket_drain(ctx);
    EXPECT_NO_EVENT(ctx->kqfd);

    kev.flags = EV_DELETE;
    if (kevent(ctx->kqfd, &kev, 1, NULL, 0, NULL) < 0)
        FAIL() << "kevent:2";
}

void
test_kevent_socket_clear(struct test_context *ctx)
{
    struct kevent kev, ret;

    EXPECT_NO_EVENT(ctx->kqfd);
    kevent_socket_drain(ctx);

    EV_SET(&kev, ctx->client_fd, EVFILT_READ, EV_ADD | EV_CLEAR, 0, 0, &ctx->client_fd);
    if (kevent(ctx->kqfd, &kev, 1, NULL, 0, NULL) < 0)
        FAIL() << "kevent1";

    kevent_socket_fill(ctx);
    kevent_socket_fill(ctx);

/* Solaris does not offer a way to get the amount of data pending */
#if defined(__sun__)
    kev.data = 1;
#else
    kev.data = 2;
#endif
    EXPECT_EVENT(ctx->kqfd, &ret);
    EXPECT_EQ(kev, ret);

    /* We filled twice, but drain once. Edge-triggered would not generate
       additional events.
     */
    kevent_socket_drain(ctx);
    EXPECT_NO_EVENT(ctx->kqfd);

    kevent_socket_drain(ctx);
    EV_SET(&kev, ctx->client_fd, EVFILT_READ, EV_DELETE, 0, 0, &ctx->client_fd);
    if (kevent(ctx->kqfd, &kev, 1, NULL, 0, NULL) < 0)
        FAIL() << "kevent2";
}

void
test_kevent_socket_disable_and_enable(struct test_context *ctx)
{
    struct kevent kev, ret;

    /* Add an event, then disable it. */
    EV_SET(&kev, ctx->client_fd, EVFILT_READ, EV_ADD, 0, 0, &ctx->client_fd);
    if (kevent(ctx->kqfd, &kev, 1, NULL, 0, NULL) < 0)
        FAIL() << "kevent";
    EV_SET(&kev, ctx->client_fd, EVFILT_READ, EV_DISABLE, 0, 0, &ctx->client_fd);
    if (kevent(ctx->kqfd, &kev, 1, NULL, 0, NULL) < 0)
        FAIL() << "kevent";

    kevent_socket_fill(ctx);
    EXPECT_NO_EVENT(ctx->kqfd);

    /* Re-enable the knote, then see if an event is generated */
    kev.flags = EV_ENABLE;
    if (kevent(ctx->kqfd, &kev, 1, NULL, 0, NULL) < 0)
        FAIL() << "kevent";
    kev.flags = EV_ADD;
    kev.data = 1;
    EXPECT_EVENT(ctx->kqfd, &ret);
    EXPECT_EQ(kev, ret);

    kevent_socket_drain(ctx);

    kev.flags = EV_DELETE;
    if (kevent(ctx->kqfd, &kev, 1, NULL, 0, NULL) < 0)
        FAIL() << "kevent";
}

void
test_kevent_socket_del(struct test_context *ctx)
{
    struct kevent kev;

    EV_SET(&kev, ctx->client_fd, EVFILT_READ, EV_DELETE, 0, 0, &ctx->client_fd);
    if (kevent(ctx->kqfd, &kev, 1, NULL, 0, NULL) < 0)
        FAIL() << "kevent";

    kevent_socket_fill(ctx);
    EXPECT_NO_EVENT(ctx->kqfd);
    kevent_socket_drain(ctx);
}

void
test_kevent_socket_oneshot(struct test_context *ctx)
{
    struct kevent kev, ret;

    /* Re-add the watch and make sure no events are pending */
    kev = KEventCreate(ctx->client_fd, EVFILT_READ, EV_ADD | EV_ONESHOT);
    EXPECT_EQ(0, kevent(ctx->kqfd, &kev, 1, NULL, 0, NULL)) << strerror(errno)
                                                            << " - " << kev;
    EXPECT_NO_EVENT(ctx->kqfd);

    kevent_socket_fill(ctx);
    kev.data = 1;
    EXPECT_EVENT(ctx->kqfd, &ret);
    EXPECT_EQ(kev, ret);

    EXPECT_NO_EVENT(ctx->kqfd);

    /* Verify that the kernel watch has been deleted */
    kevent_socket_fill(ctx);
    EXPECT_NO_EVENT(ctx->kqfd);
    kevent_socket_drain(ctx);

    /* Verify that the kevent structure does not exist. */
    kev.flags = EV_DELETE;
    if (kevent(ctx->kqfd, &kev, 1, NULL, 0, NULL) == 0)
        FAIL() << "kevent() should have failed";
}

/*
 * Test if the data field returns 1 when a listen(2) socket has
 * a pending connection.
 */
TEST_F(KQLegacyTests, ReadSocketListenBacklog)
{
    struct kevent kev, ret;
    struct sockaddr_in sain;
    int one = 1;
    int clnt, srvr;

    /* Create a passive socket */
    memset(&sain, 0, sizeof(sain));
    sain.sin_family = AF_INET;
    if ((srvr = socket(PF_INET, SOCK_STREAM, 0)) < 0)
        FAIL() << "socket(): " << strerror(errno);
    if (setsockopt(srvr, SOL_SOCKET, SO_REUSEADDR,
                (char *) &one, sizeof(one)) != 0)
        FAIL() << "setsockopt(): " << strerror(errno);
    if (bind(srvr, (const struct sockaddr *)&sain, sizeof(sain)) < 0)
        FAIL() << "bind(): " << strerror(errno);
    if (listen(srvr, 100) < 0)
        FAIL() << "listen(): " << strerror(errno);

    /* Watch for events on the socket */
    EXPECT_NO_EVENT(kqfd());
    kev = KEventCreate(srvr, EVFILT_READ, EV_ADD | EV_ONESHOT);
    EXPECT_EQ(0, kevent(kqfd(), &kev, 1, NULL, 0, NULL)) << strerror(errno)
                                                            << " - " << kev;
    EXPECT_NO_EVENT(kqfd());

    // Fill in the server port.
    socklen_t addrlen = sizeof(sain);
    if (getsockname(srvr, (struct sockaddr *)&sain, &addrlen))
        FAIL() << "getsockname: " << strerror(errno);

    /* Simulate a client connecting to the server */
    sain.sin_addr.s_addr = inet_addr("127.0.0.1");
    if ((clnt = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        FAIL() << "socket(): " << strerror(errno);
    if (connect(clnt, (const struct sockaddr *)&sain, sizeof(sain)) < 0)
        FAIL() << "connect(): " << strerror(errno);

    /* Verify that data=1 */
    kev.data = 1;
    EXPECT_EVENT(kqfd(), &ret);
    EXPECT_EQ(kev, ret);
    EXPECT_NO_EVENT(kqfd());
}

void
test_kevent_socket_dispatch(struct test_context *ctx)
{
    struct kevent kev, ret;

    /* Re-add the watch and make sure no events are pending */
    kev = KEventCreate(ctx->client_fd, EVFILT_READ, EV_ADD | EV_DISPATCH);
    EXPECT_EQ(0, kevent(ctx->kqfd, &kev, 1, NULL, 0, NULL)) << strerror(errno)
                                                            << " - " << kev;
    EXPECT_NO_EVENT(ctx->kqfd);

    /* The event will occur only once, even though EV_CLEAR is not
       specified. */
    kevent_socket_fill(ctx);
    kev.data = 1;
    EXPECT_EVENT(ctx->kqfd, &ret);
    EXPECT_EQ(kev, ret);
    EXPECT_NO_EVENT(ctx->kqfd);

    /* Re-enable the kevent */
    /* FIXME- is EV_DISPATCH needed when rearming ? */
    kev = KEventCreate(ctx->client_fd, EVFILT_READ, EV_ENABLE | EV_DISPATCH);
    EXPECT_EQ(0, kevent(ctx->kqfd, &kev, 1, NULL, 0, NULL)) << strerror(errno)
                                                            << " - " << kev;
    kev.data = 1;
    kev.flags = EV_ADD | EV_DISPATCH;   /* FIXME: may not be portable */
    EXPECT_EVENT(ctx->kqfd, &ret);
    EXPECT_EQ(kev, ret);
    EXPECT_NO_EVENT(ctx->kqfd);

    /* Since the knote is disabled, the EV_DELETE operation succeeds. */
    kev = KEventCreate(ctx->client_fd, EVFILT_READ, EV_DELETE);
    EXPECT_EQ(0, kevent(ctx->kqfd, &kev, 1, NULL, 0, NULL)) << strerror(errno)
                                                            << " - " << kev;

    kevent_socket_drain(ctx);
}

void
test_kevent_socket_lowat(struct test_context *ctx)
{
#if defined(NOTE_LOWAT)
    struct kevent kev;
    /* Re-add the watch and make sure no events are pending */
    puts("-- re-adding knote, setting low watermark to 2 bytes");
    EV_SET(&kev, ctx->client_fd, EVFILT_READ, EV_ADD | EV_ONESHOT, NOTE_LOWAT, 2, &ctx->client_fd);
    if (kevent(ctx->kqfd, &kev, 1, NULL, 0, NULL) < 0)
        die("%s", test_id);
    EXPECT_NO_EVENT();

    puts("-- checking that one byte does not trigger an event..");
    kevent_socket_fill(ctx);
    EXPECT_NO_EVENT();

    puts("-- checking that two bytes triggers an event..");
    kevent_socket_fill(ctx);
    if (kevent(ctx->kqfd, NULL, 0, &kev, 1, NULL) != 1)
        die("%s", test_id);
    KEV_CMP(kev, ctx->client_fd, EVFILT_READ, 0);
    EXPECT_NO_EVENT();

    kevent_socket_drain(ctx);
    kevent_socket_drain(ctx);
#endif  // NOTE_LOWAT
}

void
test_kevent_socket_eof(struct test_context *ctx)
{
    struct kevent kev, ret;

    /* Re-add the watch and make sure no events are pending */
    kev = KEventCreate(ctx->client_fd, EVFILT_READ, EV_ADD);
    EXPECT_EQ(0, kevent(ctx->kqfd, &kev, 1, NULL, 0, NULL)) << strerror(errno)
                                                            << " - " << kev;
    EXPECT_NO_EVENT(ctx->kqfd);

    //if (shutdown(ctx->server_fd, SHUT_RDWR) < 0)
    //    FAIL() << "close(2)";
    if (close(ctx->server_fd) < 0)
        FAIL() << "close(2)";

    kev.flags |= EV_EOF;
    EXPECT_EVENT(ctx->kqfd, &ret);
    EXPECT_EQ(kev, ret);

    /* Delete the watch */
    kev = KEventCreate(ctx->client_fd, EVFILT_READ, EV_DELETE);
    EXPECT_EQ(0, kevent(ctx->kqfd, &kev, 1, NULL, 0, NULL)) << strerror(errno)
                                                            << " - " << kev;
}

/* Test if EVFILT_READ works with regular files */
TEST_F(KQLegacyTests, ReadRegularFile)
{
    struct kevent kev, ret;
    off_t curpos;
    int fd;

    fd = open("/etc/hosts", O_RDONLY);
    if (fd < 0)
        FAIL();

    EV_SET(&kev, fd, EVFILT_READ, EV_ADD, 0, 0, &fd);
    if (kevent(kqfd(), &kev, 1, NULL, 0, NULL) < 0)
        FAIL() << "kevent";

    EXPECT_EVENT(kqfd(), &ret);

    /* Set file position to EOF-1 */
    ret.data--;
    if ((curpos = lseek(fd, ret.data, SEEK_SET)) != ret.data) {
        printf("seek to %u failed with rv=%lu\n",
                (unsigned int) ret.data, (unsigned long) curpos);
        FAIL();
    }

    /* Set file position to EOF */
    EXPECT_EVENT(kqfd(), NULL);
    ret.data = curpos + 1;
    if ((curpos = lseek(fd, ret.data, SEEK_SET)) != ret.data) {
        printf("seek to %u failed with rv=%lu\n",
                (unsigned int) ret.data, (unsigned long) curpos);
        FAIL();
    }

    EXPECT_NO_EVENT(kqfd());

    kev.flags = EV_DELETE;
    if (kevent(kqfd(), &kev, 1, NULL, 0, NULL) < 0)
        FAIL() << "kevent";
    close(fd);
}

void
test_evfilt_read(struct test_context *ctx)
{
    create_socket_connection(&ctx->client_fd, &ctx->server_fd);

    test(kevent_socket_add, ctx);
    test(kevent_socket_del, ctx);
    test(kevent_socket_add_without_ev_add, ctx);
    test(kevent_socket_get, ctx);
    test(kevent_socket_disable_and_enable, ctx);
    test(kevent_socket_oneshot, ctx);
    test(kevent_socket_clear, ctx);
    test(kevent_socket_dispatch, ctx);
    test(kevent_socket_eof, ctx);

    close(ctx->client_fd);
    close(ctx->server_fd);
}
