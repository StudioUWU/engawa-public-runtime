if (ER_ENABLE_INPROCESS_UTILS_DLL)
    foreach (_er_engawaruntimeutils_angle_target IN ITEMS ANGLE GLESv2 EGL)
        if (NOT TARGET ${_er_engawaruntimeutils_angle_target})
            message(FATAL_ERROR
                "The EngawaRuntimeUtils profile is missing the static ANGLE target "
                "${_er_engawaruntimeutils_angle_target}.")
        endif ()
        get_target_property(_er_engawaruntimeutils_angle_type
            ${_er_engawaruntimeutils_angle_target} TYPE)
        if (NOT _er_engawaruntimeutils_angle_type STREQUAL "STATIC_LIBRARY")
            message(FATAL_ERROR
                "EngawaRuntimeUtils requires ${_er_engawaruntimeutils_angle_target} to be "
                "STATIC_LIBRARY, got ${_er_engawaruntimeutils_angle_type}.")
        endif ()
    endforeach ()

    set(ER_ENGAWARUNTIMEUTILS_EXPORTS_FILE
        "${CMAKE_SOURCE_DIR}/SDK/EngawaRuntimeUtilsWindowsExports.def"
        CACHE FILEPATH "Audited EngawaRuntimeUtils dependency export contract" FORCE)
    if (NOT EXISTS "${ER_ENGAWARUNTIMEUTILS_EXPORTS_FILE}")
        message(FATAL_ERROR
            "The audited EngawaRuntimeUtils export contract is missing: "
            "${ER_ENGAWARUNTIMEUTILS_EXPORTS_FILE}")
    endif ()
    set(ER_ENGAWARUNTIMEUTILS_ENABLE_ANGLE ON CACHE BOOL
        "Fold ANGLE into EngawaRuntimeUtils" FORCE)
    set(ER_ENGAWARUNTIMEUTILS_ANGLE_ARCHIVES
        "$<TARGET_FILE:EGL>;$<TARGET_FILE:GLESv2>;$<TARGET_FILE:ANGLE>"
        CACHE STRING "Static ANGLE archives folded into EngawaRuntimeUtils" FORCE)
    set(ER_ENGAWARUNTIMEUTILS_ANGLE_LINK_LIBRARIES
        "dxguid;dxgi;d3d9;synchronization"
        CACHE STRING "ANGLE Windows system-link closure" FORCE)
    set(ER_ENGAWARUNTIMEUTILS_EXTERNAL_DEPENDS
        "ANGLE;GLESv2;EGL"
        CACHE STRING "Parent producers required by EngawaRuntimeUtils" FORCE)
    set(ER_ENGAWARUNTIMEUTILS_VCPKG_MANIFEST_FEATURES
        "web;lcms;skia;woff2"
        CACHE STRING "Minimal static support manifest for EngawaRuntimeUtils" FORCE)

    include("${CMAKE_SOURCE_DIR}/Tools/EngawaRuntime/EngawaRuntimeUtils/IntegrateEngawaRuntimeUtils.cmake")
    er_add_engawaruntimeutils_external_project()
    include("${CMAKE_SOURCE_DIR}/Tools/EngawaRuntime/ConfigureEngawaRuntimeUtilsParentLink.cmake")
    er_configure_engawaruntimeutils_parent_link()

    unset(_er_engawaruntimeutils_angle_target)
    unset(_er_engawaruntimeutils_angle_type)
endif ()

if (ER_ENABLE_INPROCESS_SHARED_LIBRARIES)
    include("${CMAKE_SOURCE_DIR}/Source/cmake/EngawaSharedLibraries.cmake")
    er_configure_shared_engine()
endif ()
if (ER_BUILD_PUBLIC_ENGINE_ONLY)
    add_custom_target(EngawaPublicEngine DEPENDS WebCore JavaScriptCore EngawaRuntimeUtils)
    install(FILES "${CMAKE_SOURCE_DIR}/SDK/include/engawa_webcore.h"
        DESTINATION include COMPONENT "${ER_INSTALL_COMPONENT}")
else ()
    add_subdirectory(EngawaRuntime)
endif ()

# ANGLE exposes its copied EGL headers through an interface target, but that
# interface does not carry the custom-target dependency which materializes the
# headers.  A clean Engawa/Win build can therefore race Skia compilation against
# ANGLEHeaders.  Make the generated-header edge explicit for this port.
if ((ER_ENABLE_WEBKIT_ENGINE OR ER_ENABLE_INPROCESS_ENGINE)
    AND TARGET Skia AND TARGET ANGLEHeaders)
    add_dependencies(Skia ANGLEHeaders)
endif ()
