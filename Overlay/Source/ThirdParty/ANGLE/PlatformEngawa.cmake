if (APPLE)
    include("${CMAKE_CURRENT_LIST_DIR}/PlatformMac.cmake")
elseif (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    include("${CMAKE_CURRENT_LIST_DIR}/PlatformWPE.cmake")
else ()
    include("${CMAKE_CURRENT_LIST_DIR}/PlatformWin.cmake")
endif ()

# EngawaRuntimeUtils is the DLL boundary for the aggregate profile.  Keep ANGLE's
# implementation as static inputs to that DLL, but deliberately do not define
# ANGLE_STATIC: the Windows DllMain/TLS lifecycle in global_state.cpp must stay
# enabled in the final shared image.
if (DEFINED ER_ENABLE_INPROCESS_UTILS_DLL AND ER_ENABLE_INPROCESS_UTILS_DLL)
    set(ANGLE_LIBRARY_TYPE STATIC)
    set(GLESv2_LIBRARY_TYPE STATIC)
    set(EGL_LIBRARY_TYPE STATIC)
endif ()
