if (APPLE)
    include("${CMAKE_CURRENT_LIST_DIR}/PlatformMac.cmake")

    # PlatformCocoa schedules these AirPlay SPI implementations even when the
    # feature is disabled. The public macOS SDK intentionally has no matching
    # declarations in that profile, and the strict runtime does not expose it.
    if (NOT ENABLE_WIRELESS_PLAYBACK_TARGET)
        list(REMOVE_ITEM PAL_SOURCES
            avfoundation/OutputContext.mm
            avfoundation/OutputDevice.mm
        )
    endif ()
elseif (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    include("${CMAKE_CURRENT_LIST_DIR}/PlatformWPE.cmake")
else ()
    include("${CMAKE_CURRENT_LIST_DIR}/PlatformWin.cmake")
endif ()
