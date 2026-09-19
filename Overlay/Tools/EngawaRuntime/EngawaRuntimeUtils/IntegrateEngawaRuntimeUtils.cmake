include_guard(GLOBAL)

include(ExternalProject)

set(ER_ENGAWARUNTIMEUTILS_EXPORTS_FILE "" CACHE FILEPATH
    "Absolute path to the audited EngawaRuntimeUtils module-definition file")
set(ER_ENGAWARUNTIMEUTILS_VCPKG_TOOLCHAIN_FILE "${CMAKE_TOOLCHAIN_FILE}" CACHE FILEPATH
    "vcpkg toolchain used by the isolated EngawaRuntimeUtils configure")
if (IS_ABSOLUTE "${VCPKG_INSTALLED_DIR}")
    get_filename_component(_er_engawaruntimeutils_vcpkg_state_dir
        "${VCPKG_INSTALLED_DIR}" DIRECTORY)
    set(_er_engawaruntimeutils_default_installed_dir
        "${_er_engawaruntimeutils_vcpkg_state_dir}/utils-vcpkg_installed")
else ()
    set(_er_engawaruntimeutils_default_installed_dir
        "${CMAKE_BINARY_DIR}/utils-vcpkg_installed")
endif ()
set(ER_ENGAWARUNTIMEUTILS_VCPKG_INSTALLED_DIR
    "${_er_engawaruntimeutils_default_installed_dir}" CACHE PATH
    "Dedicated preinstalled vcpkg root containing the static EngawaRuntimeUtils triplet")
set(ER_ENGAWARUNTIMEUTILS_VCPKG_STATUS_SHA256
    "141a390cfc3946709cf4a14524d1e7ea125ba9dbba773df7825f0c7c5438e1ee"
    CACHE STRING
    "Exact audited vcpkg status hash for the static EngawaRuntimeUtils dependency graph")
set(ER_ENGAWARUNTIMEUTILS_VCPKG_MANIFEST_DIR "${CMAKE_SOURCE_DIR}" CACHE PATH
    "Manifest root used to install the EngawaRuntimeUtils static dependency graph")
set(ER_ENGAWARUNTIMEUTILS_VCPKG_OVERLAY_TRIPLETS
    "${CMAKE_SOURCE_DIR}/WebKitLibraries/triplets" CACHE STRING
    "Overlay triplet directories visible to the isolated EngawaRuntimeUtils configure")
set(ER_ENGAWARUNTIMEUTILS_VCPKG_OVERLAY_PORTS
    "${CMAKE_SOURCE_DIR}/WebKitLibraries/ports/engawaruntimeutils" CACHE STRING
    "Overlay port directories visible to the isolated EngawaRuntimeUtils configure")
set(ER_ENGAWARUNTIMEUTILS_VCPKG_MANIFEST_FEATURES
    "web;lcms;skia;woff2" CACHE STRING
    "Exact manifest features used for the static EngawaRuntimeUtils dependency closure")
set(ER_ENGAWARUNTIMEUTILS_VCPKG_TARGET_TRIPLET
    "x64-windows-engawaruntimeutils" CACHE STRING
    "Static-library/dynamic-CRT vcpkg triplet used only by EngawaRuntimeUtils")
option(ER_ENGAWARUNTIMEUTILS_VCPKG_MANIFEST_INSTALL
    "Allow the isolated configure to mutate its dedicated vcpkg dependency root" OFF)
set(ER_ENGAWARUNTIMEUTILS_EXTERNAL_BINARY_DIR
    "${CMAKE_BINARY_DIR}/EngawaRuntimeUtilsStatic" CACHE PATH
    "Private subordinate build tree for EngawaRuntimeUtils")
set(ER_ENGAWARUNTIMEUTILS_RUNTIME_OUTPUT_DIRECTORY
    "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}" CACHE PATH
    "Directory receiving EngawaRuntimeUtils.dll")
set(ER_ENGAWARUNTIMEUTILS_ARCHIVE_OUTPUT_DIRECTORY
    "${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}" CACHE PATH
    "Directory receiving the private EngawaRuntimeUtils import library")
set(ER_ENGAWARUNTIMEUTILS_EXTERNAL_DEPENDS "" CACHE STRING
    "Parent targets that must finish before the isolated configure (for example static ANGLE archives)")
option(ER_ENGAWARUNTIMEUTILS_ENABLE_ANGLE
    "Pass reviewed static ANGLE archives to the isolated EngawaRuntimeUtils build" OFF)
set(ER_ENGAWARUNTIMEUTILS_ANGLE_ARCHIVES "" CACHE STRING
    "Semicolon-separated absolute paths or resolved target-file expressions for static ANGLE archives")
set(ER_ENGAWARUNTIMEUTILS_ANGLE_LINK_LIBRARIES "" CACHE STRING
    "Semicolon-separated ANGLE system-link closure")

function(er_add_engawaruntimeutils_external_project)
    if (TARGET EngawaRuntimeUtils OR TARGET EngawaRuntimeUtilsExternalProject)
        message(FATAL_ERROR "The EngawaRuntimeUtils external project may be declared only once.")
    endif ()
    if (NOT WIN32 OR NOT MSVC)
        message(FATAL_ERROR
            "The current EngawaRuntimeUtils folding revision is Windows/MSVC-ABI only.")
    endif ()
    if (NOT DEFINED VCPKG_TARGET_TRIPLET
        OR NOT "${VCPKG_TARGET_TRIPLET}" STREQUAL "x64-windows-webkit")
        message(FATAL_ERROR
            "The parent WebKit configure must retain the accepted r20 "
            "x64-windows-webkit dynamic triplet/declaration semantics.")
    endif ()
    if (ER_PRODUCTION_RELEASE)
        set(_er_engawaruntimeutils_expected_triplet
            "x64-windows-engawaruntimeutils-production")
    else ()
        set(_er_engawaruntimeutils_expected_triplet
            "x64-windows-engawaruntimeutils")
    endif ()
    if (NOT "${ER_ENGAWARUNTIMEUTILS_VCPKG_TARGET_TRIPLET}"
        STREQUAL "${_er_engawaruntimeutils_expected_triplet}")
        message(FATAL_ERROR
            "EngawaRuntimeUtils requires the mode-specific "
            "${_er_engawaruntimeutils_expected_triplet} static-/MD triplet.")
    endif ()
    if (NOT IS_ABSOLUTE "${ER_ENGAWARUNTIMEUTILS_EXPORTS_FILE}"
        OR NOT EXISTS "${ER_ENGAWARUNTIMEUTILS_EXPORTS_FILE}")
        message(FATAL_ERROR
            "ER_ENGAWARUNTIMEUTILS_EXPORTS_FILE must name the existing absolute audited .def file.")
    endif ()
    foreach (_er_engawaruntimeutils_required_path IN ITEMS
        ER_ENGAWARUNTIMEUTILS_VCPKG_TOOLCHAIN_FILE
        ER_ENGAWARUNTIMEUTILS_VCPKG_INSTALLED_DIR
        ER_ENGAWARUNTIMEUTILS_VCPKG_MANIFEST_DIR
        ER_ENGAWARUNTIMEUTILS_EXTERNAL_BINARY_DIR
        ER_ENGAWARUNTIMEUTILS_RUNTIME_OUTPUT_DIRECTORY
        ER_ENGAWARUNTIMEUTILS_ARCHIVE_OUTPUT_DIRECTORY
    )
        if (NOT IS_ABSOLUTE "${${_er_engawaruntimeutils_required_path}}")
            message(FATAL_ERROR
                "${_er_engawaruntimeutils_required_path} must be an absolute path.")
        endif ()
    endforeach ()
    if (NOT IS_DIRECTORY "${VCPKG_INSTALLED_DIR}")
        message(FATAL_ERROR
            "The parent VCPKG_INSTALLED_DIR does not exist: ${VCPKG_INSTALLED_DIR}")
    endif ()
    if (NOT IS_DIRECTORY "${ER_ENGAWARUNTIMEUTILS_VCPKG_INSTALLED_DIR}")
        message(FATAL_ERROR
            "The dedicated EngawaRuntimeUtils vcpkg root does not exist: "
            "${ER_ENGAWARUNTIMEUTILS_VCPKG_INSTALLED_DIR}")
    endif ()
    file(REAL_PATH "${VCPKG_INSTALLED_DIR}"
        _er_engawaruntimeutils_parent_installed_dir)
    file(REAL_PATH "${ER_ENGAWARUNTIMEUTILS_VCPKG_INSTALLED_DIR}"
        _er_engawaruntimeutils_child_installed_dir)
    file(TO_CMAKE_PATH "${_er_engawaruntimeutils_parent_installed_dir}"
        _er_engawaruntimeutils_parent_installed_dir)
    file(TO_CMAKE_PATH "${_er_engawaruntimeutils_child_installed_dir}"
        _er_engawaruntimeutils_child_installed_dir)
    string(TOLOWER "${_er_engawaruntimeutils_parent_installed_dir}"
        _er_engawaruntimeutils_parent_installed_dir_lower)
    string(TOLOWER "${_er_engawaruntimeutils_child_installed_dir}"
        _er_engawaruntimeutils_child_installed_dir_lower)
    if (_er_engawaruntimeutils_parent_installed_dir_lower STREQUAL
        _er_engawaruntimeutils_child_installed_dir_lower)
        message(FATAL_ERROR
            "EngawaRuntimeUtils must use a dedicated vcpkg installed root; sharing "
            "the parent VCPKG_INSTALLED_DIR lets manifest mode prune one triplet graph.")
    endif ()
    if (NOT EXISTS "${ER_ENGAWARUNTIMEUTILS_VCPKG_TOOLCHAIN_FILE}")
        message(FATAL_ERROR
            "The isolated vcpkg toolchain does not exist: "
            "${ER_ENGAWARUNTIMEUTILS_VCPKG_TOOLCHAIN_FILE}")
    endif ()
    if (NOT EXISTS
        "${CMAKE_SOURCE_DIR}/WebKitLibraries/triplets/${ER_ENGAWARUNTIMEUTILS_VCPKG_TARGET_TRIPLET}.cmake")
        message(FATAL_ERROR "The EngawaRuntimeUtils static-/MD triplet is missing.")
    endif ()
    if (NOT ER_ENGAWARUNTIMEUTILS_VCPKG_OVERLAY_PORTS)
        message(FATAL_ERROR
            "ER_ENGAWARUNTIMEUTILS_VCPKG_OVERLAY_PORTS must include the pinned ICU overlay.")
    endif ()
    foreach (_er_engawaruntimeutils_overlay_port IN LISTS
        ER_ENGAWARUNTIMEUTILS_VCPKG_OVERLAY_PORTS)
        if (NOT IS_ABSOLUTE "${_er_engawaruntimeutils_overlay_port}"
            OR NOT IS_DIRECTORY "${_er_engawaruntimeutils_overlay_port}")
            message(FATAL_ERROR
                "Every ER_ENGAWARUNTIMEUTILS_VCPKG_OVERLAY_PORTS entry must be an "
                "existing absolute directory: ${_er_engawaruntimeutils_overlay_port}")
        endif ()
    endforeach ()
    if (NOT EXISTS
        "${CMAKE_SOURCE_DIR}/WebKitLibraries/ports/engawaruntimeutils/icu/ORIGIN.md")
        message(FATAL_ERROR "The pinned EngawaRuntimeUtils ICU overlay is missing.")
    endif ()

    if (NOT ER_ENGAWARUNTIMEUTILS_VCPKG_MANIFEST_INSTALL)
        set(_er_engawaruntimeutils_prefix
            "${ER_ENGAWARUNTIMEUTILS_VCPKG_INSTALLED_DIR}/${ER_ENGAWARUNTIMEUTILS_VCPKG_TARGET_TRIPLET}")
        set(_er_engawaruntimeutils_status_file
            "${ER_ENGAWARUNTIMEUTILS_VCPKG_INSTALLED_DIR}/vcpkg/status")
        foreach (_er_engawaruntimeutils_prefix_path IN ITEMS
            "${_er_engawaruntimeutils_prefix}/include"
            "${_er_engawaruntimeutils_prefix}/lib"
            "${_er_engawaruntimeutils_prefix}/debug/lib"
            "${_er_engawaruntimeutils_prefix}/share/icu"
            "${_er_engawaruntimeutils_status_file}"
        )
            if (NOT EXISTS "${_er_engawaruntimeutils_prefix_path}")
                message(FATAL_ERROR
                    "The preinstalled EngawaRuntimeUtils static prefix is incomplete: "
                    "${_er_engawaruntimeutils_prefix_path}")
            endif ()
        endforeach ()

        string(LENGTH "${ER_ENGAWARUNTIMEUTILS_VCPKG_STATUS_SHA256}"
            _er_engawaruntimeutils_expected_status_sha256_length)
        if (NOT _er_engawaruntimeutils_expected_status_sha256_length EQUAL 64
            OR NOT ER_ENGAWARUNTIMEUTILS_VCPKG_STATUS_SHA256 MATCHES
                "^[0-9a-fA-F]+$")
            message(FATAL_ERROR
                "ER_ENGAWARUNTIMEUTILS_VCPKG_STATUS_SHA256 must be an exact SHA-256 digest.")
        endif ()
        file(SHA256 "${_er_engawaruntimeutils_status_file}"
            _er_engawaruntimeutils_actual_status_sha256)
        string(TOLOWER "${ER_ENGAWARUNTIMEUTILS_VCPKG_STATUS_SHA256}"
            _er_engawaruntimeutils_expected_status_sha256)
        string(TOLOWER "${_er_engawaruntimeutils_actual_status_sha256}"
            _er_engawaruntimeutils_actual_status_sha256)
        if (NOT _er_engawaruntimeutils_actual_status_sha256 STREQUAL
            _er_engawaruntimeutils_expected_status_sha256)
            message(FATAL_ERROR
                "The dedicated EngawaRuntimeUtils vcpkg status hash is stale: expected "
                "${_er_engawaruntimeutils_expected_status_sha256}, got "
                "${_er_engawaruntimeutils_actual_status_sha256}.")
        endif ()

        file(READ "${_er_engawaruntimeutils_status_file}"
            _er_engawaruntimeutils_status_contents)
        string(REPLACE "\r\n" "\n" _er_engawaruntimeutils_status_contents
            "${_er_engawaruntimeutils_status_contents}")
        string(REPLACE "\n\n" ";" _er_engawaruntimeutils_status_paragraphs
            "${_er_engawaruntimeutils_status_contents}")
        set(_er_engawaruntimeutils_required_packages
            brotli curl harfbuzz icu lcms libjpeg-turbo libpng libpsl
            libressl libwebp libxml2 libxslt nghttp2 nghttp3 ngtcp2 openssl
            sqlite3 woff2 zlib zlib-ng)
        foreach (_er_engawaruntimeutils_required_package IN LISTS
            _er_engawaruntimeutils_required_packages)
            set(_er_engawaruntimeutils_package_installed FALSE)
            foreach (_er_engawaruntimeutils_status_paragraph IN LISTS
                _er_engawaruntimeutils_status_paragraphs)
                if (_er_engawaruntimeutils_status_paragraph MATCHES
                    "(^|\n)Package: ${_er_engawaruntimeutils_required_package}(\n|$)"
                    AND _er_engawaruntimeutils_status_paragraph MATCHES
                    "(^|\n)Architecture: ${ER_ENGAWARUNTIMEUTILS_VCPKG_TARGET_TRIPLET}(\n|$)"
                    AND _er_engawaruntimeutils_status_paragraph MATCHES
                    "(^|\n)Status: .* installed(\n|$)")
                    set(_er_engawaruntimeutils_package_installed TRUE)
                    break()
                endif ()
            endforeach ()
            if (NOT _er_engawaruntimeutils_package_installed)
                message(FATAL_ERROR
                    "The dedicated EngawaRuntimeUtils prefix status does not contain "
                    "installed ${_er_engawaruntimeutils_required_package}:"
                    "${ER_ENGAWARUNTIMEUTILS_VCPKG_TARGET_TRIPLET}.")
            endif ()
        endforeach ()
    endif ()

    if (CMAKE_CONFIGURATION_TYPES)
        set(_er_engawaruntimeutils_configuration "Release")
    elseif (CMAKE_BUILD_TYPE)
        set(_er_engawaruntimeutils_configuration "${CMAKE_BUILD_TYPE}")
    else ()
        message(FATAL_ERROR
            "The isolated EngawaRuntimeUtils project requires an explicit build configuration.")
    endif ()

    set(_er_engawaruntimeutils_manifest_install OFF)
    if (ER_ENGAWARUNTIMEUTILS_VCPKG_MANIFEST_INSTALL)
        set(_er_engawaruntimeutils_manifest_install ON)
    endif ()

    # ExternalProject uses '|' as its temporary list separator so manifest
    # features, overlay directories, and ANGLE lists reach the subordinate
    # configure as semicolon-separated CMake lists without entering this one.
    string(REPLACE ";" "|" _er_engawaruntimeutils_manifest_features
        "${ER_ENGAWARUNTIMEUTILS_VCPKG_MANIFEST_FEATURES}")
    string(REPLACE ";" "|" _er_engawaruntimeutils_overlay_triplets
        "${ER_ENGAWARUNTIMEUTILS_VCPKG_OVERLAY_TRIPLETS}")
    string(REPLACE ";" "|" _er_engawaruntimeutils_overlay_ports
        "${ER_ENGAWARUNTIMEUTILS_VCPKG_OVERLAY_PORTS}")
    # ExternalProject writes the final command into a generated CMake script.
    # Native backslashes in cache values would be parsed there as escapes
    # (notably `\U` in C:\Users), so normalize all path-list arguments first.
    file(TO_CMAKE_PATH "${_er_engawaruntimeutils_overlay_triplets}"
        _er_engawaruntimeutils_overlay_triplets)
    file(TO_CMAKE_PATH "${_er_engawaruntimeutils_overlay_ports}"
        _er_engawaruntimeutils_overlay_ports)
    string(REPLACE ";" "|" _er_engawaruntimeutils_angle_archives
        "${ER_ENGAWARUNTIMEUTILS_ANGLE_ARCHIVES}")
    string(REPLACE ";" "|" _er_engawaruntimeutils_angle_link_libraries
        "${ER_ENGAWARUNTIMEUTILS_ANGLE_LINK_LIBRARIES}")

    set(_er_engawaruntimeutils_runtime_file
        "${ER_ENGAWARUNTIMEUTILS_RUNTIME_OUTPUT_DIRECTORY}/EngawaRuntimeUtils.dll")
    set(_er_engawaruntimeutils_import_library
        "${ER_ENGAWARUNTIMEUTILS_ARCHIVE_OUTPUT_DIRECTORY}/EngawaRuntimeUtils.lib")
    set(_er_engawaruntimeutils_source_dir
        "${CMAKE_SOURCE_DIR}/Tools/EngawaRuntime/EngawaRuntimeUtils")

    set(_er_engawaruntimeutils_cmake_args
        "-DCMAKE_BUILD_TYPE:STRING=${_er_engawaruntimeutils_configuration}"
        "-DCMAKE_CXX_COMPILER:FILEPATH=${CMAKE_CXX_COMPILER}"
        "-DCMAKE_MSVC_RUNTIME_LIBRARY:STRING=MultiThreadedDLL"
        "-DCMAKE_TOOLCHAIN_FILE:FILEPATH=${ER_ENGAWARUNTIMEUTILS_VCPKG_TOOLCHAIN_FILE}"
        "-DER_ENGAWARUNTIMEUTILS_ARCHIVE_OUTPUT_DIRECTORY:PATH=${ER_ENGAWARUNTIMEUTILS_ARCHIVE_OUTPUT_DIRECTORY}"
        "-DER_ENGAWARUNTIMEUTILS_ANGLE_ARCHIVES:STRING=${_er_engawaruntimeutils_angle_archives}"
        "-DER_ENGAWARUNTIMEUTILS_ANGLE_LINK_LIBRARIES:STRING=${_er_engawaruntimeutils_angle_link_libraries}"
        "-DER_ENGAWARUNTIMEUTILS_ENABLE_ANGLE:BOOL=${ER_ENGAWARUNTIMEUTILS_ENABLE_ANGLE}"
        "-DER_ENGAWARUNTIMEUTILS_EXPORTS_FILE:FILEPATH=${ER_ENGAWARUNTIMEUTILS_EXPORTS_FILE}"
        "-DER_ENGAWARUNTIMEUTILS_RUNTIME_OUTPUT_DIRECTORY:PATH=${ER_ENGAWARUNTIMEUTILS_RUNTIME_OUTPUT_DIRECTORY}"
        "-DVCPKG_INSTALLED_DIR:PATH=${ER_ENGAWARUNTIMEUTILS_VCPKG_INSTALLED_DIR}"
        "-DVCPKG_MANIFEST_DIR:PATH=${ER_ENGAWARUNTIMEUTILS_VCPKG_MANIFEST_DIR}"
        "-DVCPKG_MANIFEST_FEATURES:STRING=${_er_engawaruntimeutils_manifest_features}"
        "-DVCPKG_MANIFEST_INSTALL:BOOL=${_er_engawaruntimeutils_manifest_install}"
        "-DVCPKG_OVERLAY_PORTS:STRING=${_er_engawaruntimeutils_overlay_ports}"
        "-DVCPKG_OVERLAY_TRIPLETS:STRING=${_er_engawaruntimeutils_overlay_triplets}"
        "-DVCPKG_TARGET_TRIPLET:STRING=${ER_ENGAWARUNTIMEUTILS_VCPKG_TARGET_TRIPLET}"
    )
    if (ER_PRODUCTION_RELEASE)
        list(APPEND _er_engawaruntimeutils_cmake_args
            "-DER_ENGAWARUNTIMEUTILS_PRODUCTION_PATH_MAP_ROOT:PATH=${ER_PRODUCTION_PATH_MAP_ROOT}"
            "-DER_ENGAWARUNTIMEUTILS_PRODUCTION_RELEASE:BOOL=ON")
    endif ()
    if (CMAKE_MAKE_PROGRAM)
        list(APPEND _er_engawaruntimeutils_cmake_args
            "-DCMAKE_MAKE_PROGRAM:FILEPATH=${CMAKE_MAKE_PROGRAM}")
    endif ()
    if (CMAKE_LINKER)
        list(APPEND _er_engawaruntimeutils_cmake_args
            "-DCMAKE_LINKER:FILEPATH=${CMAKE_LINKER}")
    endif ()
    if (CMAKE_AR)
        list(APPEND _er_engawaruntimeutils_cmake_args
            "-DCMAKE_AR:FILEPATH=${CMAKE_AR}")
    endif ()

    set(_er_engawaruntimeutils_external_arguments
        EngawaRuntimeUtilsExternalProject
        SOURCE_DIR "${_er_engawaruntimeutils_source_dir}"
        BINARY_DIR "${ER_ENGAWARUNTIMEUTILS_EXTERNAL_BINARY_DIR}"
        LIST_SEPARATOR "|"
        CMAKE_ARGS ${_er_engawaruntimeutils_cmake_args}
        BUILD_COMMAND
            "${CMAKE_COMMAND}" --build <BINARY_DIR>
                --config "${_er_engawaruntimeutils_configuration}"
                --target EngawaRuntimeUtils
        # Parent dependencies give ExternalProject ordering, not file-level
        # invalidation. Always enter the subordinate incremental build so its
        # Ninja graph can observe changed ANGLE archives, dependency archives,
        # child sources, and the audited export definition.
        BUILD_ALWAYS 1
        BUILD_BYPRODUCTS
            "${_er_engawaruntimeutils_runtime_file}"
            "${_er_engawaruntimeutils_import_library}"
        DOWNLOAD_COMMAND ""
        UPDATE_COMMAND ""
        PATCH_COMMAND ""
        # Building the DLL is the complete subordinate action. An empty list
        # item is discarded when this argument vector is expanded, which would
        # accidentally restore ExternalProject's default `--target install`.
        INSTALL_COMMAND "${CMAKE_COMMAND}" -E true
        TEST_COMMAND ""
        LOG_CONFIGURE ON
        LOG_BUILD ON
        LOG_OUTPUT_ON_FAILURE ON
        EXCLUDE_FROM_ALL TRUE
        USES_TERMINAL_CONFIGURE TRUE
        USES_TERMINAL_BUILD TRUE
    )
    if (ER_ENGAWARUNTIMEUTILS_EXTERNAL_DEPENDS)
        list(APPEND _er_engawaruntimeutils_external_arguments
            DEPENDS ${ER_ENGAWARUNTIMEUTILS_EXTERNAL_DEPENDS})
    endif ()
    ExternalProject_Add(${_er_engawaruntimeutils_external_arguments})
    get_property(_er_engawaruntimeutils_install_command_set
        TARGET EngawaRuntimeUtilsExternalProject PROPERTY _EP_INSTALL_COMMAND SET)
    get_property(_er_engawaruntimeutils_install_command
        TARGET EngawaRuntimeUtilsExternalProject PROPERTY _EP_INSTALL_COMMAND)
    set(_er_engawaruntimeutils_expected_install_command
        "${CMAKE_COMMAND};-E;true")
    if (NOT _er_engawaruntimeutils_install_command_set
        OR NOT _er_engawaruntimeutils_install_command STREQUAL
            _er_engawaruntimeutils_expected_install_command)
        message(FATAL_ERROR
            "EngawaRuntimeUtils must suppress ExternalProject's default install step; "
            "got '${_er_engawaruntimeutils_install_command}'.")
    endif ()

    add_library(EngawaRuntimeUtils SHARED IMPORTED GLOBAL)
    set_target_properties(EngawaRuntimeUtils PROPERTIES
        IMPORTED_CONFIGURATIONS "Debug;Release;RelWithDebInfo;MinSizeRel"
        IMPORTED_IMPLIB "${_er_engawaruntimeutils_import_library}"
        IMPORTED_LOCATION "${_er_engawaruntimeutils_runtime_file}"
        IMPORTED_IMPLIB_DEBUG "${_er_engawaruntimeutils_import_library}"
        IMPORTED_IMPLIB_MINSIZEREL "${_er_engawaruntimeutils_import_library}"
        IMPORTED_IMPLIB_RELEASE "${_er_engawaruntimeutils_import_library}"
        IMPORTED_IMPLIB_RELWITHDEBINFO "${_er_engawaruntimeutils_import_library}"
        IMPORTED_LOCATION_DEBUG "${_er_engawaruntimeutils_runtime_file}"
        IMPORTED_LOCATION_MINSIZEREL "${_er_engawaruntimeutils_runtime_file}"
        IMPORTED_LOCATION_RELEASE "${_er_engawaruntimeutils_runtime_file}"
        IMPORTED_LOCATION_RELWITHDEBINFO "${_er_engawaruntimeutils_runtime_file}"
        INTERFACE_LINK_LIBRARIES ""
    )
    add_library(EngawaRuntimeUtils::EngawaRuntimeUtils ALIAS EngawaRuntimeUtils)
    add_dependencies(EngawaRuntimeUtils EngawaRuntimeUtilsExternalProject)

    set(ER_ENGAWARUNTIMEUTILS_RUNTIME_FILE
        "${_er_engawaruntimeutils_runtime_file}" PARENT_SCOPE)
    set(ER_ENGAWARUNTIMEUTILS_IMPORT_LIBRARY
        "${_er_engawaruntimeutils_import_library}" PARENT_SCOPE)
    # Tools/PlatformEngawa.cmake is evaluated in a sibling directory scope after
    # Source/PlatformEngawa.cmake initializes the aggregate. Publish the exact
    # ExternalProject byproducts globally so the dedicated package component
    # installs the same files that back the imported target.
    set(ER_ENGAWARUNTIMEUTILS_RUNTIME_FILE
        "${_er_engawaruntimeutils_runtime_file}" CACHE INTERNAL
        "EngawaRuntimeUtils ExternalProject runtime byproduct" FORCE)
    set(ER_ENGAWARUNTIMEUTILS_IMPORT_LIBRARY
        "${_er_engawaruntimeutils_import_library}" CACHE INTERNAL
        "EngawaRuntimeUtils ExternalProject import-library byproduct" FORCE)
endfunction()

unset(_er_engawaruntimeutils_default_installed_dir)
unset(_er_engawaruntimeutils_vcpkg_state_dir)
