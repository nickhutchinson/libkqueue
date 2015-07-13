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

#files
if(UNIX)
    set(SRC
        main.cpp
        kevent.cpp
        test.cpp
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
        test.cpp
        timer.cpp
        user.cpp
        vnode.cpp
    )
endif()


#includes
include_directories(
	../include
)

if(UNIX)
    add_definitions(
        -rdynamic
    )
    set(LIBS pthread rt)
else()
    set(LIBS wsock32)
endif()

add_library(gtest STATIC googletest/src/gtest-all.cc)
target_include_directories(gtest PUBLIC googletest/include PRIVATE googletest)

add_executable(libkqueue-test ${SRC})
target_compile_definitions(libkqueue-test PRIVATE LIBKQUEUE)
target_link_libraries(libkqueue-test kqueue_shared ${LIBS} gtest)
set_target_properties(libkqueue-test PROPERTIES DEBUG_POSTFIX "D")

if (NOT CMAKE_VERSION VERSION_LESS 3.2)
    set (USES_TERMINAL_FLAG USES_TERMINAL)
else ()
    set (USES_TERMINAL_FLAG "")
endif ()

add_custom_target(check
    libkqueue-test -n 5
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    VERBATIM ${USES_TERMINAL_FLAG})
