#
# Copyright (c) 2011 Marius Zwicker <marius@mlba-team.de>
#
# Permission to use, copy, modify, and distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
# OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
#
cmake_minimum_required(VERSION 2.8.12)
project(kqueue-test CXX)

enable_testing()

set(kqueue_lib)
if (TARGET kqueue_shared)
    set(kqueue_lib kqueue_shared)
endif()

add_library(gtest STATIC googletest/googletest/src/gtest-all.cc)
target_include_directories(gtest PUBLIC googletest/googletest/include PRIVATE googletest/googletest)

#files
if(UNIX)
    set(SRC
        main.cpp
        kevent.cpp
        proc.cpp
        read.cpp
        signal.cpp
        timer.cpp
        vnode.cpp
        user.cpp
    )
else()
    set(SRC
        common.h
        kevent.cpp
        main.cpp
        read.cpp
        timer.cpp
        user.cpp
        vnode.cpp
    )
endif()

if(UNIX)
    set(LIBS pthread rt)
else()
    set(LIBS wsock32)
endif()

add_executable(libkqueue-test ${SRC})
set_target_properties(libkqueue-test PROPERTIES DEBUG_POSTFIX "D")

target_compile_definitions(libkqueue-test PRIVATE LIBKQUEUE)
target_link_libraries(libkqueue-test ${kqueue_lib} ${LIBS} gtest)

add_test(NAME libkqueue-test COMMAND libkqueue-test -n 5)
