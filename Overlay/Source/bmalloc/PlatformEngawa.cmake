if (APPLE)
    include("${CMAKE_CURRENT_LIST_DIR}/PlatformMac.cmake")
elseif (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    # Upstream WPE deliberately uses the common POSIX/JSC-only bmalloc graph;
    # there is no separate bmalloc/PlatformWPE.cmake in this WebKit revision.
    include("${CMAKE_CURRENT_LIST_DIR}/PlatformJSCOnly.cmake")
else ()
    include("${CMAKE_CURRENT_LIST_DIR}/PlatformJSCOnly.cmake")
endif ()
