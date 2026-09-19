include_guard(GLOBAL)

# ANGLE is one of the parent-built archives consumed by the subordinate
# EngawaRuntimeUtils link. Its private ZLIB edge must not be redirected back to the
# aggregate import target, or the graph becomes ANGLE -> EngawaRuntimeUtils -> ANGLE.
# A static archive needs only Zlib's compile usage here; the subordinate link
# supplies the pinned static Zlib artifact from its isolated prefix.
function(_er_detach_angle_zlib_artifact)
    if (NOT TARGET ANGLE OR NOT TARGET ZLIB::ZLIB)
        message(FATAL_ERROR
            "EngawaRuntimeUtils requires both ANGLE and ZLIB::ZLIB before remapping.")
    endif ()
    get_target_property(_er_angle_type ANGLE TYPE)
    if (NOT _er_angle_type STREQUAL "STATIC_LIBRARY")
        message(FATAL_ERROR
            "EngawaRuntimeUtils can detach ANGLE's Zlib artifact only from a static ANGLE target.")
    endif ()

    set(_er_angle_zlib_usage EREngawaRuntimeUtilsANGLEZlibCompileUsage)
    if (TARGET ${_er_angle_zlib_usage})
        message(FATAL_ERROR "EngawaRuntimeUtils ANGLE Zlib usage facade already exists.")
    endif ()
    add_library(${_er_angle_zlib_usage} INTERFACE)

    get_target_property(_er_zlib_target ZLIB::ZLIB ALIASED_TARGET)
    if (NOT _er_zlib_target)
        set(_er_zlib_target ZLIB::ZLIB)
    endif ()
    foreach (_er_usage_property IN ITEMS
        INTERFACE_INCLUDE_DIRECTORIES
        INTERFACE_SYSTEM_INCLUDE_DIRECTORIES
        INTERFACE_COMPILE_DEFINITIONS
        INTERFACE_COMPILE_OPTIONS
        INTERFACE_COMPILE_FEATURES)
        get_target_property(_er_usage_value
            "${_er_zlib_target}" ${_er_usage_property})
        if (_er_usage_value
            AND NOT _er_usage_value MATCHES "-NOTFOUND$")
            set_property(TARGET ${_er_angle_zlib_usage}
                PROPERTY ${_er_usage_property} "${_er_usage_value}")
        endif ()
    endforeach ()

    set(_er_zlib_edge_replaced OFF)
    foreach (_er_angle_link_property IN ITEMS
        LINK_LIBRARIES INTERFACE_LINK_LIBRARIES)
        get_target_property(_er_angle_links ANGLE ${_er_angle_link_property})
        if (NOT _er_angle_links
            OR _er_angle_links MATCHES "-NOTFOUND$")
            continue()
        endif ()
        set(_er_rewritten_angle_links)
        foreach (_er_angle_link IN LISTS _er_angle_links)
            string(FIND "${_er_angle_link}" "ZLIB::ZLIB"
                _er_zlib_edge_position)
            if (NOT _er_zlib_edge_position EQUAL -1)
                string(REPLACE "ZLIB::ZLIB" "${_er_angle_zlib_usage}"
                    _er_angle_link "${_er_angle_link}")
                set(_er_zlib_edge_replaced ON)
            endif ()
            list(APPEND _er_rewritten_angle_links "${_er_angle_link}")
        endforeach ()
        set_property(TARGET ANGLE PROPERTY ${_er_angle_link_property}
            "${_er_rewritten_angle_links}")
    endforeach ()
    if (NOT _er_zlib_edge_replaced)
        message(FATAL_ERROR
            "EngawaRuntimeUtils did not find ANGLE's expected ZLIB::ZLIB link edge.")
    endif ()
endfunction()

# Preserve every dependency target's usage requirements while replacing only
# its dynamic link artifact.  WebCore, JSC, WTF, PAL, and Skia can therefore
# keep compiling against the accepted r20 headers and import decorations, but
# their final consumer resolves the same symbol names from EngawaRuntimeUtils.lib.
function(_er_redirect_imported_dependency target_name)
    if (NOT TARGET "${target_name}")
        message(FATAL_ERROR
            "EngawaRuntimeUtils cannot redirect missing dependency target ${target_name}.")
    endif ()

    get_target_property(_er_alias_target "${target_name}" ALIASED_TARGET)
    if (_er_alias_target)
        set(target_name "${_er_alias_target}")
    endif ()
    get_target_property(_er_imported "${target_name}" IMPORTED)
    if (NOT _er_imported)
        message(FATAL_ERROR
            "EngawaRuntimeUtils expected ${target_name} to be an imported dependency target.")
    endif ()

    get_target_property(_er_target_type "${target_name}" TYPE)
    if (_er_target_type STREQUAL "INTERFACE_LIBRARY")
        # Preserve wrapper usage requirements such as PNG::PNG ->
        # PNG::png_shared. Redirect the wrapped artifact target instead of
        # replacing the wrapper edge and losing inherited include directories.
        get_target_property(_er_wrapped_targets "${target_name}"
            INTERFACE_LINK_LIBRARIES)
        set(_er_redirected_wrapped_target OFF)
        foreach (_er_wrapped_target IN LISTS _er_wrapped_targets)
            if (TARGET "${_er_wrapped_target}")
                _er_redirect_imported_dependency("${_er_wrapped_target}")
                set(_er_redirected_wrapped_target ON)
            endif ()
        endforeach ()
        if (NOT _er_redirected_wrapped_target)
            message(FATAL_ERROR
                "EngawaRuntimeUtils cannot identify the artifact wrapped by ${target_name}.")
        endif ()
    elseif (_er_target_type STREQUAL "SHARED_LIBRARY")
        # A shared imported target presents its .lib to the linker and its DLL
        # as the runtime artifact.
        set_target_properties("${target_name}" PROPERTIES
            IMPORTED_IMPLIB "${ER_ENGAWARUNTIMEUTILS_IMPORT_LIBRARY}"
            IMPORTED_LOCATION "${ER_ENGAWARUNTIMEUTILS_RUNTIME_FILE}"
            INTERFACE_LINK_LIBRARIES ""
            IMPORTED_LINK_DEPENDENT_LIBRARIES ""
            IMPORTED_LINK_INTERFACE_LIBRARIES ""
        )
        foreach (_er_config IN ITEMS
            DEBUG RELEASE RELWITHDEBINFO MINSIZEREL NOCONFIG)
            set_target_properties("${target_name}" PROPERTIES
                "IMPORTED_IMPLIB_${_er_config}" "${ER_ENGAWARUNTIMEUTILS_IMPORT_LIBRARY}"
                "IMPORTED_LOCATION_${_er_config}" "${ER_ENGAWARUNTIMEUTILS_RUNTIME_FILE}"
                "IMPORTED_LINK_DEPENDENT_LIBRARIES_${_er_config}" ""
                "IMPORTED_LINK_INTERFACE_LIBRARIES_${_er_config}" ""
            )
        endforeach ()
    elseif (_er_target_type STREQUAL "UNKNOWN_LIBRARY"
        OR _er_target_type STREQUAL "STATIC_LIBRARY")
        # UNKNOWN imported targets and static-style Find modules link the file
        # in IMPORTED_LOCATION directly, so point that property at the .lib.
        set_target_properties("${target_name}" PROPERTIES
            IMPORTED_LOCATION "${ER_ENGAWARUNTIMEUTILS_IMPORT_LIBRARY}"
            INTERFACE_LINK_LIBRARIES ""
            IMPORTED_LINK_DEPENDENT_LIBRARIES ""
            IMPORTED_LINK_INTERFACE_LIBRARIES ""
        )
        foreach (_er_config IN ITEMS
            DEBUG RELEASE RELWITHDEBINFO MINSIZEREL NOCONFIG)
            set_target_properties("${target_name}" PROPERTIES
                "IMPORTED_LOCATION_${_er_config}" "${ER_ENGAWARUNTIMEUTILS_IMPORT_LIBRARY}"
                "IMPORTED_LINK_DEPENDENT_LIBRARIES_${_er_config}" ""
                "IMPORTED_LINK_INTERFACE_LIBRARIES_${_er_config}" ""
            )
        endforeach ()
    else ()
        message(FATAL_ERROR
            "EngawaRuntimeUtils cannot redirect ${target_name} of type ${_er_target_type}.")
    endif ()
endfunction()

function(_er_assert_no_legacy_support_link target_name ancestry)
    get_target_property(_er_alias_target "${target_name}" ALIASED_TARGET)
    if (_er_alias_target)
        set(target_name "${_er_alias_target}")
    endif ()
    if (target_name STREQUAL "ANGLE"
        OR target_name STREQUAL "GLESv2"
        OR target_name STREQUAL "EGL")
        message(FATAL_ERROR
            "EngawaRuntimeUtils root-link audit reached the private static ANGLE "
            "producer ${target_name}.")
    endif ()
    if ("${target_name}" IN_LIST ancestry)
        return()
    endif ()
    get_property(_er_visited_targets GLOBAL PROPERTY
        ER_ENGAWARUNTIMEUTILS_ROOT_LINK_VISITED)
    if ("${target_name}" IN_LIST _er_visited_targets)
        return()
    endif ()
    set_property(GLOBAL APPEND PROPERTY
        ER_ENGAWARUNTIMEUTILS_ROOT_LINK_VISITED "${target_name}")
    list(APPEND ancestry "${target_name}")

    set(_er_legacy_import_basenames
        brotlicommon
        brotlidec
        crypto
        harfbuzz
        harfbuzz-icu
        icudt
        icuin
        icuuc
        jpeg
        lcms2
        libcurl_imp
        libegl
        libglesv2
        libpng16
        libsharpyuv
        libwebp
        libwebpdemux
        libwebpmux
        libxml2
        libxslt
        psl
        sqlite3
        ssl
        zlib
    )
    set(_er_link_properties
        LINK_LIBRARIES
        INTERFACE_LINK_LIBRARIES
        IMPORTED_IMPLIB
        IMPORTED_LOCATION
        IMPORTED_LINK_DEPENDENT_LIBRARIES
        IMPORTED_LINK_INTERFACE_LIBRARIES
    )
    foreach (_er_config IN ITEMS
        DEBUG RELEASE RELWITHDEBINFO MINSIZEREL NOCONFIG)
        list(APPEND _er_link_properties
            "IMPORTED_IMPLIB_${_er_config}"
            "IMPORTED_LOCATION_${_er_config}"
            "IMPORTED_LINK_DEPENDENT_LIBRARIES_${_er_config}"
            "IMPORTED_LINK_INTERFACE_LIBRARIES_${_er_config}"
        )
    endforeach ()

    foreach (_er_property IN LISTS _er_link_properties)
        get_target_property(_er_values "${target_name}" ${_er_property})
        if (NOT _er_values OR _er_values MATCHES "-NOTFOUND$")
            continue()
        endif ()
        foreach (_er_value IN LISTS _er_values)
            string(TOLOWER "${_er_value}" _er_lower_value)
            foreach (_er_basename IN LISTS _er_legacy_import_basenames)
                string(FIND "${_er_lower_value}"
                    "${_er_basename}.lib" _er_legacy_position)
                if (NOT _er_legacy_position EQUAL -1)
                    message(FATAL_ERROR
                        "EngawaRuntimeUtils root-link audit found legacy support library "
                        "${_er_basename}.lib in ${target_name}'s ${_er_property}: "
                        "${_er_value}")
                endif ()
            endforeach ()

            if (TARGET "${_er_value}")
                _er_assert_no_legacy_support_link(
                    "${_er_value}" "${ancestry}")
            else ()
                # Static framework interfaces commonly hide target names in
                # $<LINK_ONLY:...>. Extract target-like tokens and recurse into
                # every token CMake recognizes as a target.
                string(REGEX MATCHALL
                    "[A-Za-z0-9_.+-]+(::[A-Za-z0-9_.+-]+)?"
                    _er_target_candidates "${_er_value}")
                foreach (_er_candidate IN LISTS _er_target_candidates)
                    if (TARGET "${_er_candidate}")
                        _er_assert_no_legacy_support_link(
                            "${_er_candidate}" "${ancestry}")
                    endif ()
                endforeach ()
            endif ()
        endforeach ()
    endforeach ()
endfunction()

function(er_assert_engawaruntimeutils_root_link target_name)
    if (NOT ER_ENABLE_INPROCESS_UTILS_DLL)
        return()
    endif ()
    if (NOT TARGET "${target_name}")
        message(FATAL_ERROR
            "EngawaRuntimeUtils root-link audit cannot find ${target_name}.")
    endif ()
    set_property(GLOBAL PROPERTY ER_ENGAWARUNTIMEUTILS_ROOT_LINK_VISITED "")
    _er_assert_no_legacy_support_link("${target_name}" "")
    message(STATUS
        "EngawaRuntimeUtils root-link graph is free of legacy dynamic support import libraries.")
endfunction()

function(er_configure_engawaruntimeutils_parent_link)
    if (NOT ER_ENABLE_INPROCESS_UTILS_DLL)
        return()
    endif ()
    if (NOT TARGET EngawaRuntimeUtils::EngawaRuntimeUtils)
        message(FATAL_ERROR "EngawaRuntimeUtils imported target is not available.")
    endif ()

    _er_detach_angle_zlib_artifact()

    set(_er_dynamic_dependency_targets
        Brotli::common
        Brotli::dec
        CURL::libcurl
        HarfBuzz::HarfBuzz
        HarfBuzz::ICU
        ICU::data
        ICU::i18n
        ICU::uc
        JPEG::JPEG
        LCMS2::LCMS2
        LibPSL::LibPSL
        LibXml2::LibXml2
        LibXslt::LibXslt
        OpenSSL::Crypto
        OpenSSL::SSL
        PNG::PNG
        SQLite3::SQLite3
        WebP::demux
        WebP::libwebp
        WebP::mux
        ZLIB::ZLIB
    )
    foreach (_er_target IN LISTS _er_dynamic_dependency_targets)
        _er_redirect_imported_dependency("${_er_target}")
    endforeach ()

    # ANGLE headers and prototype definitions stay on their framework targets;
    # only their link items move to the aggregate import library.
    foreach (_er_angle_framework IN ITEMS EGLFramework GLESv2Framework)
        if (NOT TARGET ${_er_angle_framework})
            message(FATAL_ERROR
                "EngawaRuntimeUtils cannot remap missing ${_er_angle_framework}.")
        endif ()
        set_property(TARGET ${_er_angle_framework} PROPERTY
            INTERFACE_LINK_LIBRARIES EngawaRuntimeUtils::EngawaRuntimeUtils)
    endforeach ()

    # OptionsWin exposes sharpyuv as a raw path and WebCore is already
    # constructed when this port-level integration runs. Rewrite the exact
    # artifact in both link properties and fail if the expected edge is absent.
    if (NOT SHARPYUV_LIBS)
        message(FATAL_ERROR "EngawaRuntimeUtils expected OptionsWin to resolve sharpyuv.")
    endif ()
    set(_er_sharpyuv_replaced OFF)
    foreach (_er_webcore_link_property IN ITEMS
        LINK_LIBRARIES INTERFACE_LINK_LIBRARIES)
        get_target_property(_er_webcore_links WebCore
            ${_er_webcore_link_property})
        if (NOT _er_webcore_links)
            continue()
        endif ()
        set(_er_rewritten_webcore_links)
        foreach (_er_webcore_link IN LISTS _er_webcore_links)
            string(FIND "${_er_webcore_link}" "${SHARPYUV_LIBS}"
                _er_sharpyuv_position)
            if (NOT _er_sharpyuv_position EQUAL -1)
                string(REPLACE "${SHARPYUV_LIBS}"
                    "EngawaRuntimeUtils::EngawaRuntimeUtils"
                    _er_webcore_link "${_er_webcore_link}")
                list(APPEND _er_rewritten_webcore_links
                    "${_er_webcore_link}")
                set(_er_sharpyuv_replaced ON)
            else ()
                list(APPEND _er_rewritten_webcore_links
                    "${_er_webcore_link}")
            endif ()
        endforeach ()
        set_property(TARGET WebCore PROPERTY ${_er_webcore_link_property}
            "${_er_rewritten_webcore_links}")
    endforeach ()
    if (NOT _er_sharpyuv_replaced)
        message(FATAL_ERROR
            "EngawaRuntimeUtils did not find the resolved sharpyuv path in WebCore's link graph.")
    endif ()
endfunction()
