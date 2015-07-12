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

#ifndef _COMMON_H
#define _COMMON_H

#include "config/config.h"
#include <gtest/gtest.h>

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef _WIN32
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/event.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <poll.h>
#include <netdb.h>
#include <sys/time.h>
#else
# include "include/sys/event.h"
# include "src/windows/platform.h"
#endif

struct test_context;

struct unit_test {
    const char *ut_name;
    void      (*ut_func)(struct test_context *);
};

#define MAX_TESTS 50
struct test_context {
    int kqfd;

    /* EVFILT_READ and EVFILT_WRITE */
    int client_fd;
    int server_fd;

    /* EVFILT_VNODE */
    int vnode_fd;
    char testfile[PATH_MAX];
};

inline bool
operator==(const struct kevent &a, const struct kevent &b)
{
    return 0 == memcmp(&a, &b, sizeof(struct kevent));
}

inline bool
operator!=(const struct kevent &a, const struct kevent &b)
{
    return !operator==(a, b);
}

inline std::ostream &
operator<<(std::ostream &os, const struct kevent &kev)
{
    os << "(";
    if (kev.filter == EVFILT_READ)
        os << "READ";
    else if (kev.filter == EVFILT_WRITE)
        os << "WRITE";
    else if (kev.filter == EVFILT_AIO)
        os << "AIO";
    else if (kev.filter == EVFILT_VNODE)
        os << "VNODE";
    else if (kev.filter == EVFILT_PROC)
        os << "PROC";
    else if (kev.filter == EVFILT_SIGNAL)
        os << "SIGNAL";
    else if (kev.filter == EVFILT_TIMER)
        os << "TIMER";
#ifdef EVFILT_MACHPORT
    if (kev.filter == EVFILT_MACHPORT)
        os << "MACHPORT";
#endif
    else if (kev.filter == EVFILT_FS)
        os << "FS";
    else if (kev.filter == EVFILT_USER)
        os << "USER";
#ifdef EVFILT_VM
    else if (kev.filter == EVFILT_VM)
        os << "VM";
#endif
    else
        os << "???";

    os << ", " << kev.ident << ")";

    os << ", flags=";
    if (kev.flags & EV_ADD)
        os << "ADD ";
    if (kev.flags & EV_ENABLE)
        os << "ENABLE ";
    if (kev.flags & EV_DISABLE)
        os << "DISABLE ";
    if (kev.flags & EV_DELETE)
        os << "DELETE ";
    if (kev.flags & EV_ONESHOT)
        os << "ONESHOT ";
    if (kev.flags & EV_CLEAR)
        os << "CLEAR ";
    if (kev.flags & EV_EOF)
        os << "EOF ";
#ifdef EV_OOBAND
    if (kev.flags & EV_OOBAND)
        os << "OOBAND ";
#endif
    if (kev.flags & EV_ERROR)
        os << "ERROR ";
    if (kev.flags & EV_DISPATCH)
        os << "DISPATCH ";
    if (kev.flags & EV_RECEIPT)
        os << "RECEIPT ";
    if (kev.flags)
        os.seekp(-1, std::ios_base::end);

    os << ", data=" << kev.data << ", udata=" << static_cast<void *>(kev.udata);

    return os;
}

inline ::testing::AssertionResult
ExpectEventImpl(const char * /*kq_expr*/, const char * /*eventlist_expr*/,
                int kq, struct kevent *event)
{
    struct kevent tmp = {};

    if (!event)
        event = &tmp;
    else
        memset(event, 0, sizeof(*event));

    struct timespec ts = {};
    int status = kevent(kq, NULL, 0, event, 1, &ts);

    if (status == 1)
        return ::testing::AssertionSuccess();
    else if (status == 0)
        return ::testing::AssertionFailure() << "no event";
    else
        return ::testing::AssertionFailure() << "error; status=" << status
                                             << ", error: " << strerror(errno);
}

inline ::testing::AssertionResult
ExpectNoEventImpl(const char * /*kq_expr*/, int kq)
{
    struct kevent event = {};

    struct timespec ts = {};
    int status = kevent(kq, NULL, 0, &event, 1, &ts);

    if (status == 0)
        return ::testing::AssertionSuccess();
    else if (status == 1)
        return ::testing::AssertionFailure() << "unexpected event: "
                                             << ::testing::PrintToString(event);
    else
        return ::testing::AssertionFailure() << "error; status=" << status
                                             << ", error: " << strerror(errno);
}

#define EXPECT_EVENT(kq, event_buf) \
    EXPECT_PRED_FORMAT2(ExpectEventImpl, (kq), (event_buf))

#define EXPECT_NO_EVENT(kq) EXPECT_PRED_FORMAT1(ExpectNoEventImpl, (kq))

class KQLegacyTests : public ::testing::Test {
public:
    // Global to the entire test run, for compatibility with the legacy test
    // suite.
    static int kqfd() { return kqfd_; }
    static void SetUpTestCase()
    {
        kqfd_ = kqueue();
#ifdef _WIN32
        /* Initialize the Winsock library */
        WSADATA wsaData;
        ASSERT_TRUE(WSAStartup(MAKEWORD(2, 2), &wsaData));
#endif
    }

    static void TearDownTestCase()
    {
        close(kqfd_);
        kqfd_ = -1;
    }

    virtual void SetUp()
    {
        memset(&legacy_context_, 0, sizeof(legacy_context_));
        legacy_context_.kqfd = kqfd();
    }

    struct test_context *context() { return &legacy_context_; }
private:
    struct test_context legacy_context_;
    static int kqfd_;
};

void test_evfilt_read(struct test_context *);
void test_evfilt_signal(struct test_context *);
void test_evfilt_vnode(struct test_context *);
void test_evfilt_timer(struct test_context *);
void test_evfilt_proc(struct test_context *);
void test_evfilt_user(struct test_context *);

#define test(f, ctx, ...)    \
    do {                     \
        assert(ctx != NULL); \
        test_##f(ctx);       \
    } while (/*CONSTCOND*/ 0)

void kevent_get_hires(struct kevent *, int);

inline struct kevent
KEventCreate(uintptr_t ident, short filter, u_short flags, u_int fflags = 0,
             intptr_t data = 0)
{
    struct kevent kev = {ident, filter, flags, fflags, data};
    return kev;
}

#endif  /* _COMMON_H */
