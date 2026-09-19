# Copyright (C) 2026 EngawaRuntime contributors.
# SPDX-License-Identifier: BSD-2-Clause
include_guard(GLOBAL)

# ICU is shared by JSC, WebCore, and the proprietary bundle-key normalizer.
# Linux also aggregates its locked external HarfBuzz and libwpe archives.
# WebKit-owned code remains in the public engine libraries; operating-system
# libraries retain their normal dynamic/system linkage.
function(er_add_unix_runtime_utils)
    add_library(EngawaRuntimeUtils SHARED
        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/EngawaRuntimeUtilsAnchor.cpp")
    add_library(EngawaRuntimeUtils::EngawaRuntimeUtils ALIAS EngawaRuntimeUtils)
    set(_er_utils_directory "${CMAKE_BINARY_DIR}/lib")
    set_target_properties(EngawaRuntimeUtils PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY "${_er_utils_directory}"
        CXX_VISIBILITY_PRESET hidden VISIBILITY_INLINES_HIDDEN ON
        POSITION_INDEPENDENT_CODE ON)
    set(_er_archive_targets ICU::i18n ICU::uc ICU::data)
    if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
        list(APPEND _er_archive_targets HarfBuzz::ICU HarfBuzz::HarfBuzz WPE::libwpe)
        # libwpe's static loader directly references this backend hook. Own it
        # beside libwpe so Utils never depends back on WebCore.
        if (NOT TARGET WPE::libwpe)
            message(FATAL_ERROR "Linux Utils requires WPE::libwpe.")
        endif ()
        target_sources(EngawaRuntimeUtils PRIVATE
            "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/EngawaInProcessWPELoader.c")
        get_target_property(_er_wpe_includes WPE::libwpe INTERFACE_INCLUDE_DIRECTORIES)
        get_target_property(_er_wpe_options WPE::libwpe INTERFACE_COMPILE_OPTIONS)
        if (_er_wpe_includes)
            target_include_directories(EngawaRuntimeUtils PRIVATE ${_er_wpe_includes})
        endif ()
        if (_er_wpe_options)
            target_compile_options(EngawaRuntimeUtils PRIVATE ${_er_wpe_options})
        endif ()
        # FindHarfBuzz omits the static archive's FreeType dependency. The
        # locked build enables that backend. libwpe similarly requires its
        # XKB/EGL platform closure, previously attached to WPE::libwpe.
        foreach (_er_system_target IN ITEMS Freetype::Freetype XkbCommon::XkbCommon)
            if (NOT TARGET ${_er_system_target})
                message(FATAL_ERROR "Linux Utils requires ${_er_system_target}.")
            endif ()
        endforeach ()
        if (NOT EGL_LIBRARIES)
            message(FATAL_ERROR "Linux Utils requires the selected EGL libraries.")
        endif ()
        set(_er_system_links Freetype::Freetype XkbCommon::XkbCommon ${EGL_LIBRARIES})
    endif ()
    foreach (_er_dependency IN LISTS _er_archive_targets)
        if (NOT TARGET ${_er_dependency})
            message(FATAL_ERROR "The split engine requires ${_er_dependency}.")
        endif ()
        get_target_property(_er_dependency_target ${_er_dependency} ALIASED_TARGET)
        if (NOT _er_dependency_target)
            set(_er_dependency_target ${_er_dependency})
        endif ()
        get_target_property(_er_imported ${_er_dependency_target} IMPORTED)
        if (NOT _er_imported)
            message(FATAL_ERROR "Utils accepts only locked imported dependency archives: ${_er_dependency}")
        endif ()
        get_target_property(_er_archive ${_er_dependency_target} IMPORTED_LOCATION_RELEASE)
        if (NOT _er_archive)
            get_target_property(_er_archive ${_er_dependency_target} IMPORTED_LOCATION)
        endif ()
        if (NOT _er_archive OR NOT _er_archive MATCHES "\\.a$"
            OR NOT EXISTS "${_er_archive}")
            message(FATAL_ERROR "Utils requires the locked static archive ${_er_dependency}: ${_er_archive}")
        endif ()
        get_target_property(_er_previous_links ${_er_dependency_target} INTERFACE_LINK_LIBRARIES)
        if (_er_previous_links AND NOT _er_previous_links MATCHES "-NOTFOUND$")
            foreach (_er_link IN LISTS _er_previous_links)
                if (NOT _er_link IN_LIST _er_archive_targets
                    AND NOT _er_link IN_LIST _er_system_links)
                    message(FATAL_ERROR
                        "Review the new ${_er_dependency} link dependency before aggregation: ${_er_link}")
                endif ()
            endforeach ()
        endif ()
        if (APPLE)
            target_link_options(EngawaRuntimeUtils PRIVATE "LINKER:-force_load,${_er_archive}")
        else ()
            target_link_libraries(EngawaRuntimeUtils PRIVATE
                "-Wl,--whole-archive" "${_er_archive}" "-Wl,--no-whole-archive")
        endif ()
        set(_er_utils_file "${_er_utils_directory}/${CMAKE_SHARED_LIBRARY_PREFIX}EngawaRuntimeUtils${CMAKE_SHARED_LIBRARY_SUFFIX}")
        # Keep dependency headers/compile policy but redirect every link configuration
        # to the independently produced, replaceable aggregate binary.
        set_target_properties(${_er_dependency_target} PROPERTIES
            IMPORTED_LOCATION "${_er_utils_file}"
            INTERFACE_LINK_LIBRARIES "")
        foreach (_er_config IN ITEMS DEBUG RELEASE RELWITHDEBINFO MINSIZEREL NOCONFIG)
            set_property(TARGET ${_er_dependency_target} PROPERTY
                IMPORTED_LOCATION_${_er_config} "${_er_utils_file}")
        endforeach ()
        add_dependencies(${_er_dependency_target} EngawaRuntimeUtils)
    endforeach ()
    if (NOT APPLE)
        # Keep system dependencies after the archive objects for --as-needed.
        target_link_libraries(EngawaRuntimeUtils PRIVATE
            ${_er_system_links} "${CMAKE_DL_LIBS}" Threads::Threads)
        target_link_options(EngawaRuntimeUtils PRIVATE "LINKER:-z,defs")
    endif ()
endfunction()

function(er_configure_shared_engine)
    if (NOT WIN32)
        er_add_unix_runtime_utils()
    elseif (NOT ER_ENABLE_INPROCESS_UTILS_DLL)
        message(FATAL_ERROR "The Windows split SDK requires ER_ENABLE_INPROCESS_UTILS_DLL=ON.")
    endif ()
    foreach (_er_target IN ITEMS JavaScriptCore WebCore EngawaRuntimeUtils)
        get_target_property(_er_type ${_er_target} TYPE)
        if (NOT _er_type STREQUAL "SHARED_LIBRARY")
            message(FATAL_ERROR "The split engine requires shared ${_er_target}; got ${_er_type}.")
        endif ()
        get_target_property(_er_imported ${_er_target} IMPORTED)
        if (_er_imported)
            install(FILES "$<TARGET_FILE:${_er_target}>"
                DESTINATION bin COMPONENT "${ER_INSTALL_COMPONENT}")
            continue()
        endif ()
        set_target_properties(${_er_target} PROPERTIES
            FRAMEWORK FALSE
            OUTPUT_NAME "${_er_target}")
        # Unset these properties entirely. Empty strings produce a literal
        # trailing dot in ELF SONAMEs and a second symlink on CMake 4.x.
        set_property(TARGET ${_er_target} PROPERTY VERSION)
        set_property(TARGET ${_er_target} PROPERTY SOVERSION)
        if (APPLE)
            set_target_properties(${_er_target} PROPERTIES
                INSTALL_NAME_DIR "@rpath" MACOSX_RPATH ON
                BUILD_WITH_INSTALL_NAME_DIR ON INSTALL_RPATH "@loader_path")
        elseif (CMAKE_SYSTEM_NAME STREQUAL "Linux")
            set_target_properties(${_er_target} PROPERTIES
                BUILD_RPATH_USE_ORIGIN ON INSTALL_RPATH "$ORIGIN"
                INSTALL_RPATH_USE_LINK_PATH OFF)
        endif ()
        install(TARGETS ${_er_target}
            RUNTIME DESTINATION bin COMPONENT "${ER_INSTALL_COMPONENT}"
            LIBRARY DESTINATION bin COMPONENT "${ER_INSTALL_COMPONENT}")
    endforeach ()
    foreach (_er_engine IN ITEMS JavaScriptCore WebCore)
        # An imported ICU target retains its original STATIC type after its
        # artifact is redirected. Declare the shared target explicitly too so
        # CMake emits the aggregate's build-time RPATH and dependency edge.
        target_link_libraries(${_er_engine} PRIVATE EngawaRuntimeUtils::EngawaRuntimeUtils)
        add_dependencies(${_er_engine} EngawaRuntimeUtils)
    endforeach ()
    foreach (_er_state IN ITEMS WTF bmalloc)
        get_property(_er_owner GLOBAL PROPERTY ${_er_state}_LINKED_INTO)
        if (NOT _er_owner STREQUAL "JavaScriptCore")
            message(FATAL_ERROR "${_er_state} must have exactly one owner: JavaScriptCore (got ${_er_owner}).")
        endif ()
    endforeach ()
endfunction()
