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

static void
test_kevent_user_add_and_delete(struct test_context *ctx)
{
    struct kevent kev;

    kev = KEventCreate(1, EVFILT_USER, EV_ADD);
    EXPECT_EQ(0, kevent(ctx->kqfd, &kev, 1, NULL, 0, NULL)) << strerror(errno)
                                                            << " - " << kev;
    EXPECT_NO_EVENT(ctx->kqfd);

    kev = KEventCreate(1, EVFILT_USER, EV_DELETE);
    EXPECT_EQ(0, kevent(ctx->kqfd, &kev, 1, NULL, 0, NULL)) << strerror(errno)
                                                            << " - " << kev;
    EXPECT_NO_EVENT(ctx->kqfd);
}

static void
test_kevent_user_get(struct test_context *ctx)
{
    struct kevent kev, ret;

    EXPECT_NO_EVENT(ctx->kqfd);

    /* Add the event, and then trigger it */
    kev = KEventCreate(1, EVFILT_USER, EV_ADD | EV_CLEAR);
    EXPECT_EQ(0, kevent(ctx->kqfd, &kev, 1, NULL, 0, NULL)) << strerror(errno)
                                                            << " - " << kev;
    kev = KEventCreate(1, EVFILT_USER, 0, NOTE_TRIGGER);
    EXPECT_EQ(0, kevent(ctx->kqfd, &kev, 1, NULL, 0, NULL)) << strerror(errno)
                                                            << " - " << kev;

    kev.fflags &= ~NOTE_FFCTRLMASK;
    kev.fflags &= ~NOTE_TRIGGER;
    kev.flags = EV_CLEAR;
    EXPECT_EVENT(ctx->kqfd, &ret);
    EXPECT_EQ(kev, ret);

    EXPECT_NO_EVENT(ctx->kqfd);
}

static void
test_kevent_user_get_hires(struct test_context *ctx)
{
    struct kevent kev, ret;

    EXPECT_NO_EVENT(ctx->kqfd);

    /* Add the event, and then trigger it */
    kev = KEventCreate(1, EVFILT_USER, EV_ADD | EV_CLEAR);
    EXPECT_EQ(0, kevent(ctx->kqfd, &kev, 1, NULL, 0, NULL)) << strerror(errno)
                                                            << " - " << kev;
    kev = KEventCreate(1, EVFILT_USER, 0, NOTE_TRIGGER);
    EXPECT_EQ(0, kevent(ctx->kqfd, &kev, 1, NULL, 0, NULL)) << strerror(errno)
                                                            << " - " << kev;

    kev.fflags &= ~NOTE_FFCTRLMASK;
    kev.fflags &= ~NOTE_TRIGGER;
    kev.flags = EV_CLEAR;
    kevent_get_hires(&ret, ctx->kqfd);
    EXPECT_EQ(kev, ret);

    EXPECT_NO_EVENT(ctx->kqfd);
}

static void
test_kevent_user_disable_and_enable(struct test_context *ctx)
{
    struct kevent kev, ret;

    EXPECT_NO_EVENT(ctx->kqfd);

    kev = KEventCreate(1, EVFILT_USER, EV_ADD);
    EXPECT_EQ(0, kevent(ctx->kqfd, &kev, 1, NULL, 0, NULL)) << strerror(errno)
                                                            << " - " << kev;
    kev = KEventCreate(1, EVFILT_USER, EV_DISABLE);
    EXPECT_EQ(0, kevent(ctx->kqfd, &kev, 1, NULL, 0, NULL)) << strerror(errno)
                                                            << " - " << kev;

    /* Trigger the event, but since it is disabled, nothing will happen. */
    kev = KEventCreate(1, EVFILT_USER, 0, NOTE_TRIGGER);
    EXPECT_EQ(0, kevent(ctx->kqfd, &kev, 1, NULL, 0, NULL)) << strerror(errno)
                                                            << " - " << kev;
    EXPECT_NO_EVENT(ctx->kqfd);

    kev = KEventCreate(1, EVFILT_USER, EV_ENABLE);
    EXPECT_EQ(0, kevent(ctx->kqfd, &kev, 1, NULL, 0, NULL)) << strerror(errno)
                                                            << " - " << kev;
    kev = KEventCreate(1, EVFILT_USER, 0, NOTE_TRIGGER);
    EXPECT_EQ(0, kevent(ctx->kqfd, &kev, 1, NULL, 0, NULL)) << strerror(errno)
                                                            << " - " << kev;

    kev.flags = EV_CLEAR;
    kev.fflags &= ~NOTE_FFCTRLMASK;
    kev.fflags &= ~NOTE_TRIGGER;
    EXPECT_EVENT(ctx->kqfd, &ret);
    EXPECT_EQ(kev, ret);
}

static void
test_kevent_user_oneshot(struct test_context *ctx)
{
    struct kevent kev, ret;

    EXPECT_NO_EVENT(ctx->kqfd);

    kev = KEventCreate(2, EVFILT_USER, EV_ADD | EV_ONESHOT);
    EXPECT_EQ(0, kevent(ctx->kqfd, &kev, 1, NULL, 0, NULL)) << strerror(errno)
                                                            << " - " << kev;
    kev = KEventCreate(2, EVFILT_USER, 0, NOTE_TRIGGER);
    EXPECT_EQ(0, kevent(ctx->kqfd, &kev, 1, NULL, 0, NULL)) << strerror(errno)
                                                            << " - " << kev;

    kev.flags = EV_ONESHOT;
    kev.fflags &= ~NOTE_FFCTRLMASK;
    kev.fflags &= ~NOTE_TRIGGER;
    EXPECT_EVENT(ctx->kqfd, &ret);
    EXPECT_EQ(kev, ret);

    EXPECT_NO_EVENT(ctx->kqfd);
}

static void
test_kevent_user_multi_trigger_merged(struct test_context *ctx)
{
    struct kevent kev, ret;
    int i;

    EXPECT_NO_EVENT(ctx->kqfd);

    kev = KEventCreate(2, EVFILT_USER, EV_ADD | EV_CLEAR);
    EXPECT_EQ(0, kevent(ctx->kqfd, &kev, 1, NULL, 0, NULL)) << strerror(errno)
                                                            << " - " << kev;

    for (i = 0; i < 10; i++)
        kev = KEventCreate(2, EVFILT_USER, 0, NOTE_TRIGGER);
    EXPECT_EQ(0, kevent(ctx->kqfd, &kev, 1, NULL, 0, NULL)) << strerror(errno)
                                                            << " - " << kev;

    kev.flags = EV_CLEAR;
    kev.fflags &= ~NOTE_FFCTRLMASK;
    kev.fflags &= ~NOTE_TRIGGER;
    EXPECT_EVENT(ctx->kqfd, &ret);
    EXPECT_EQ(kev, ret);

    EXPECT_NO_EVENT(ctx->kqfd);
}

#ifdef EV_DISPATCH
void
test_kevent_user_dispatch(struct test_context *ctx)
{
    struct kevent kev, ret;

    EXPECT_NO_EVENT(ctx->kqfd);

    /* Add the event, and then trigger it */
    kev = KEventCreate(1, EVFILT_USER, EV_ADD | EV_CLEAR | EV_DISPATCH);
    EXPECT_EQ(0, kevent(ctx->kqfd, &kev, 1, NULL, 0, NULL)) << strerror(errno)
                                                            << " - " << kev;
    kev = KEventCreate(1, EVFILT_USER, 0, NOTE_TRIGGER);
    EXPECT_EQ(0, kevent(ctx->kqfd, &kev, 1, NULL, 0, NULL)) << strerror(errno)
                                                            << " - " << kev;

    /* Retrieve one event */
    kev.fflags &= ~NOTE_FFCTRLMASK;
    kev.fflags &= ~NOTE_TRIGGER;
    kev.flags = EV_CLEAR;
    EXPECT_EVENT(ctx->kqfd, &ret);
    EXPECT_EQ(kev, ret);

    /* Confirm that the knote is disabled automatically */
    EXPECT_NO_EVENT(ctx->kqfd);

    /* Re-enable the kevent */
    /* FIXME- is EV_DISPATCH needed when rearming ? */
    kev = KEventCreate(1, EVFILT_USER, EV_ENABLE | EV_CLEAR | EV_DISPATCH);
    EXPECT_EQ(0, kevent(ctx->kqfd, &kev, 1, NULL, 0, NULL)) << strerror(errno)
                                                            << " - " << kev;
    EXPECT_NO_EVENT(ctx->kqfd);

    /* Trigger the event */
    kev = KEventCreate(1, EVFILT_USER, 0, NOTE_TRIGGER);
    EXPECT_EQ(0, kevent(ctx->kqfd, &kev, 1, NULL, 0, NULL)) << strerror(errno)
                                                            << " - " << kev;
    kev.fflags &= ~NOTE_FFCTRLMASK;
    kev.fflags &= ~NOTE_TRIGGER;
    kev.flags = EV_CLEAR;
    EXPECT_EVENT(ctx->kqfd, &ret);
    EXPECT_EQ(kev, ret);
    EXPECT_NO_EVENT(ctx->kqfd);

    /* Delete the watch */
    kev = KEventCreate(1, EVFILT_USER, EV_DELETE);
    EXPECT_EQ(0, kevent(ctx->kqfd, &kev, 1, NULL, 0, NULL)) << strerror(errno)
                                                            << " - " << kev;
    EXPECT_NO_EVENT(ctx->kqfd);
}
#endif     /* EV_DISPATCH */

void
test_evfilt_user(struct test_context *ctx)
{
    test(kevent_user_add_and_delete, ctx);
    test(kevent_user_get, ctx);
    test(kevent_user_get_hires, ctx);
    test(kevent_user_disable_and_enable, ctx);
    test(kevent_user_oneshot, ctx);
    test(kevent_user_multi_trigger_merged, ctx);
#ifdef EV_DISPATCH
    test(kevent_user_dispatch, ctx);
#endif
    /* TODO: try different fflags operations */
}
