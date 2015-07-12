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

#include <gtest/gtest.h>
#include "common.h"

int KQLegacyTests::kqfd_ = -1;

////////////////////////////////////////////////////////////////////////////////
TEST(KQueue, IsPollable)
{
    int kq, rv;
    struct kevent kev;
    fd_set fds;
    struct timeval tv;

    kq = kqueue();
    EXPECT_GE(kq, 0);

    EXPECT_NO_EVENT(kq);
    kev = KEventCreate(2, EVFILT_TIMER, EV_ADD | EV_ONESHOT, 0, 1000);
    EXPECT_EQ(0, kevent(kq, &kev, 1, NULL, 0, NULL)) << strerror(errno) << " - "
                                                     << kev;
    EXPECT_NO_EVENT(kq);

    FD_ZERO(&fds);
    FD_SET(kq, &fds);
    tv.tv_sec = 0;
    tv.tv_usec = 0;
    rv = select(1, &fds, NULL, NULL, &tv);
    EXPECT_GE(rv, 0) << "select() error";
    EXPECT_GE(rv, 1) << "select() no events";
    EXPECT_TRUE(FD_ISSET(kq, &fds)) << "descriptor is not ready for reading";

    close(kq);
}

/*
 * Test the method for detecting when one end of a socketpair
 * has been closed. This technique is used in kqueue_validate()
 */
#ifdef _WIN32
// FIXME
#else
TEST(KQueue, PeerCloseDetection)
{
    int sockfd[2];
    char buf[1];
    struct pollfd pfd;

    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockfd) < 0)
        FAIL() << "socketpair";

    pfd.fd = sockfd[0];
    pfd.events = POLLIN | POLLHUP;
    pfd.revents = 0;

    if (poll(&pfd, 1, 0) > 0)
        FAIL() << "unexpected data";

    if (close(sockfd[1]) < 0)
        FAIL() << "close";

    if (poll(&pfd, 1, 0) > 0) {
        if (recv(sockfd[0], buf, sizeof(buf), MSG_PEEK | MSG_DONTWAIT) != 0)
            FAIL() << "failed to detect peer shutdown";
    }
}
#endif

TEST(KQueue, BasicUsage)
{
    int kqfd;

    kqfd = kqueue();
    EXPECT_GE(kqfd, 0);
    EXPECT_NO_EVENT(kqfd);
    EXPECT_EQ(0, close(kqfd));
}

TEST(KQueue, BadFd)
{
    struct kevent kev;
    memset(&kev, 0, sizeof(kev));
    /* Provide an invalid kqueue descriptor */
    EXPECT_NE(0, kevent(-1, &kev, 1, NULL, 0, NULL));
    EXPECT_EQ(EBADF, errno);
}

TEST(KQueue, EVReceipt)
{
    int kq;
    struct kevent kev;

    if ((kq = kqueue()) < 0)
        FAIL() << "kqueue()";
    EV_SET(&kev, SIGUSR2, EVFILT_SIGNAL, EV_ADD | EV_RECEIPT, 0, 0, NULL);
    int ret = kevent(kq, &kev, 1, &kev, 1, NULL);
    ASSERT_EQ(1, ret);

    /* TODO: check the receipt */

    close(kq);
}

TEST_F(KQLegacyTests, Proc) { test_evfilt_proc(context()); }
TEST_F(KQLegacyTests, Read) { test_evfilt_read(context()); }
TEST_F(KQLegacyTests, Signal) { test_evfilt_signal(context()); }
TEST_F(KQLegacyTests, Timer) { test_evfilt_timer(context()); }
TEST_F(KQLegacyTests, User) { test_evfilt_user(context()); }
TEST_F(KQLegacyTests, VNode) { test_evfilt_vnode(context()); }

int
main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
