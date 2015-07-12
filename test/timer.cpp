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

void
test_kevent_timer_add(struct test_context *ctx)
{
    struct kevent kev;

    kev = KEventCreate(1, EVFILT_TIMER, EV_ADD, 0, 1000);
    EXPECT_EQ(0, kevent(ctx->kqfd, &kev, 1, NULL, 0, NULL)) << strerror(errno)
                                                            << " - " << kev;
}

void
test_kevent_timer_del(struct test_context *ctx)
{
    struct kevent kev;

    kev = KEventCreate(1, EVFILT_TIMER, EV_DELETE);
    EXPECT_EQ(0, kevent(ctx->kqfd, &kev, 1, NULL, 0, NULL)) << strerror(errno)
                                                            << " - " << kev;

    EXPECT_NO_EVENT(ctx->kqfd);
}

void
test_kevent_timer_get(struct test_context *ctx)
{
    struct kevent kev, ret;

    kev = KEventCreate(1, EVFILT_TIMER, EV_ADD, 0, 1000);
    EXPECT_EQ(0, kevent(ctx->kqfd, &kev, 1, NULL, 0, NULL)) << strerror(errno)
                                                            << " - " << kev;

    kev.flags |= EV_CLEAR;
    kev.data = 1;
    EXPECT_EVENT(ctx->kqfd, &ret);
    EXPECT_EQ(kev, ret);

    kev = KEventCreate(1, EVFILT_TIMER, EV_DELETE);
    EXPECT_EQ(0, kevent(ctx->kqfd, &kev, 1, NULL, 0, NULL)) << strerror(errno)
                                                            << " - " << kev;
}

static void
test_kevent_timer_oneshot(struct test_context *ctx)
{
    struct kevent kev, ret;

    EXPECT_NO_EVENT(ctx->kqfd);

    kev = KEventCreate(2, EVFILT_TIMER, EV_ADD | EV_ONESHOT);
    EXPECT_EQ(0, kevent(ctx->kqfd, &kev, 1, NULL, 0, NULL)) << strerror(errno)
                                                            << " - " << kev;

    /* Retrieve the event */
    kev.flags = EV_ADD | EV_CLEAR | EV_ONESHOT;
    kev.data = 1;
    EXPECT_EVENT(ctx->kqfd, &ret);
    EXPECT_EQ(kev, ret);

    /* Check if the event occurs again */
    sleep(3);
    EXPECT_NO_EVENT(ctx->kqfd);
}

static void
test_kevent_timer_periodic(struct test_context *ctx)
{
    struct kevent kev, ret;

    EXPECT_NO_EVENT(ctx->kqfd);

    kev = KEventCreate(3, EVFILT_TIMER, EV_ADD);
    EXPECT_EQ(0, kevent(ctx->kqfd, &kev, 1, NULL, 0, NULL)) << strerror(errno)
                                                            << " - " << kev;

    /* Retrieve the event */
    kev.flags = EV_ADD | EV_CLEAR;
    kev.data = 1;
    EXPECT_EVENT(ctx->kqfd, &ret);
    EXPECT_EQ(kev, ret);

    /* Check if the event occurs again */
    sleep(1);
    EXPECT_EVENT(ctx->kqfd, &ret);
    EXPECT_EQ(kev, ret);

    /* Delete the event */
    kev.flags = EV_DELETE;
    EXPECT_EQ(0, kevent(ctx->kqfd, &kev, 1, NULL, 0, NULL)) << kev;
}

static void
test_kevent_timer_disable_and_enable(struct test_context *ctx)
{
    struct kevent kev, ret;

    EXPECT_NO_EVENT(ctx->kqfd);

    /* Add the watch and immediately disable it */
    kev = KEventCreate(4, EVFILT_TIMER, EV_ADD | EV_ONESHOT);
    EXPECT_EQ(0, kevent(ctx->kqfd, &kev, 1, NULL, 0, NULL)) << strerror(errno)
                                                            << " - " << kev;
    kev.flags = EV_DISABLE;
    EXPECT_EQ(0, kevent(ctx->kqfd, &kev, 1, NULL, 0, NULL)) << kev;
    EXPECT_NO_EVENT(ctx->kqfd);

    /* Re-enable and check again */
    kev.flags = EV_ENABLE;
    EXPECT_EQ(0, kevent(ctx->kqfd, &kev, 1, NULL, 0, NULL)) << kev;

    kev.flags = EV_ADD | EV_CLEAR | EV_ONESHOT;
    kev.data = 1;
    EXPECT_EVENT(ctx->kqfd, &ret);
    EXPECT_EQ(kev, ret);
}

#ifdef EV_DISPATCH
void
test_kevent_timer_dispatch(struct test_context *ctx)
{
    struct kevent kev, ret;

    EXPECT_NO_EVENT(ctx->kqfd);

    kev = KEventCreate(4, EVFILT_TIMER, EV_ADD | EV_DISPATCH, 0, 800);
    EXPECT_EQ(0, kevent(ctx->kqfd, &kev, 1, NULL, 0, NULL)) << strerror(errno)
                                                            << " - " << kev;

    /* Get one event */
    kev.flags = EV_ADD | EV_CLEAR | EV_DISPATCH;
    kev.data = 1;
    EXPECT_EVENT(ctx->kqfd, &ret);
    EXPECT_EQ(kev, ret);

    /* Confirm that the knote is disabled */
    sleep(1);
    EXPECT_NO_EVENT(ctx->kqfd);

    /* Enable the knote and make sure no events are pending */
    kev = KEventCreate(4, EVFILT_TIMER, EV_ENABLE | EV_DISPATCH, 0, 800);
    EXPECT_EQ(0, kevent(ctx->kqfd, &kev, 1, NULL, 0, NULL)) << strerror(errno)
                                                            << " - " << kev;
    EXPECT_NO_EVENT(ctx->kqfd);

    /* Get the next event */
    sleep(1);
    kev.flags = EV_ADD | EV_CLEAR | EV_DISPATCH;
    kev.data = 1;
    EXPECT_EVENT(ctx->kqfd, &ret);
    EXPECT_EQ(kev, ret);

    /* Remove the knote and ensure the event no longer fires */
    kev = KEventCreate(4, EVFILT_TIMER, EV_DELETE);
    EXPECT_EQ(0, kevent(ctx->kqfd, &kev, 1, NULL, 0, NULL)) << strerror(errno)
                                                            << " - " << kev;
    sleep(1);
    EXPECT_NO_EVENT(ctx->kqfd);
}
#endif  /* EV_DISPATCH */

void
test_evfilt_timer(struct test_context *ctx)
{
    test(kevent_timer_add, ctx);
    test(kevent_timer_del, ctx);
    test(kevent_timer_get, ctx);
    test(kevent_timer_oneshot, ctx);
    test(kevent_timer_periodic, ctx);
    test(kevent_timer_disable_and_enable, ctx);
#ifdef EV_DISPATCH
    test(kevent_timer_dispatch, ctx);
#endif
}
