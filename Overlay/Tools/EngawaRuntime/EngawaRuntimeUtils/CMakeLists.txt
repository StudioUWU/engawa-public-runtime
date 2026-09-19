cmake_minimum_required(VERSION 3.24)

cmake_policy(SET CMP0091 NEW)
project(EngawaRuntimeUtils LANGUAGES CXX)

if (NOT WIN32 OR NOT MSVC)
    message(FATAL_ERROR "EngawaRuntimeUtils currently requires the Windows MSVC-compatible ABI.")
endif ()

set(ER_ENGAWARUNTIMEUTILS_EXPORTS_FILE "" CACHE FILEPATH
    "Absolute path to the audited EngawaRuntimeUtils module-definition file")
set(ER_ENGAWARUNTIMEUTILS_VCPKG_PREFIX "" CACHE PATH
    "Absolute static-/MD vcpkg triplet prefix used only by EngawaRuntimeUtils")
set(ER_ENGAWARUNTIMEUTILS_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin" CACHE PATH
    "Directory for the private EngawaRuntimeUtils runtime DLL")
set(ER_ENGAWARUNTIMEUTILS_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib" CACHE PATH
    "Directory for the private EngawaRuntimeUtils import library")
option(ER_ENGAWARUNTIMEUTILS_ENABLE_ANGLE
    "Link caller-supplied static ANGLE archives into EngawaRuntimeUtils" OFF)
set(ER_ENGAWARUNTIMEUTILS_ANGLE_ARCHIVES "" CACHE STRING
    "Semicolon-separated absolute paths to the reviewed static ANGLE archives")
set(ER_ENGAWARUNTIMEUTILS_ANGLE_LINK_LIBRARIES "" CACHE STRING
    "Semicolon-separated ANGLE system/imported-link closure")
option(ER_ENGAWARUNTIMEUTILS_ALLOW_DYNAMIC_PREFIX_FOR_CONFIGURE_CHECK
    "Internal: permit the r20 dynamic prefix for a configure-only scaffold check" OFF)
option(ER_ENGAWARUNTIMEUTILS_PRODUCTION_RELEASE
    "Build the private support DLL without public debug paths or records" OFF)
set(ER_ENGAWARUNTIMEUTILS_PRODUCTION_PATH_MAP_ROOT "" CACHE PATH
    "Canonical workspace root removed from the production support DLL")
mark_as_advanced(ER_ENGAWARUNTIMEUTILS_ALLOW_DYNAMIC_PREFIX_FOR_CONFIGURE_CHECK)

if (NOT IS_ABSOLUTE "${ER_ENGAWARUNTIMEUTILS_EXPORTS_FILE}"
    OR NOT EXISTS "${ER_ENGAWARUNTIMEUTILS_EXPORTS_FILE}")
    message(FATAL_ERROR
        "ER_ENGAWARUNTIMEUTILS_EXPORTS_FILE must name an existing absolute, audited .def file.")
endif ()
get_filename_component(_er_engawaruntimeutils_exports_extension
    "${ER_ENGAWARUNTIMEUTILS_EXPORTS_FILE}" EXT)
if (NOT _er_engawaruntimeutils_exports_extension STREQUAL ".def")
    message(FATAL_ERROR "ER_ENGAWARUNTIMEUTILS_EXPORTS_FILE must have the .def extension.")
endif ()

if (ER_ENGAWARUNTIMEUTILS_PRODUCTION_RELEASE)
    set(_er_engawaruntimeutils_expected_triplet
        "x64-windows-engawaruntimeutils-production")
else ()
    set(_er_engawaruntimeutils_expected_triplet
        "x64-windows-engawaruntimeutils")
endif ()
if (NOT ER_ENGAWARUNTIMEUTILS_ALLOW_DYNAMIC_PREFIX_FOR_CONFIGURE_CHECK)
    if (NOT DEFINED VCPKG_TARGET_TRIPLET
        OR NOT VCPKG_TARGET_TRIPLET STREQUAL _er_engawaruntimeutils_expected_triplet)
        message(FATAL_ERROR
            "EngawaRuntimeUtils must be isolated in the ${_er_engawaruntimeutils_expected_triplet} "
            "static-/MD triplet; the main WebKit configure must retain its r20 dynamic triplet.")
    endif ()
endif ()

if (NOT ER_ENGAWARUNTIMEUTILS_VCPKG_PREFIX)
    if (DEFINED VCPKG_INSTALLED_DIR AND DEFINED VCPKG_TARGET_TRIPLET)
        set(ER_ENGAWARUNTIMEUTILS_VCPKG_PREFIX
            "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}")
    elseif (DEFINED _VCPKG_INSTALLED_DIR AND DEFINED VCPKG_TARGET_TRIPLET)
        set(ER_ENGAWARUNTIMEUTILS_VCPKG_PREFIX
            "${_VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}")
    endif ()
endif ()
if (NOT IS_ABSOLUTE "${ER_ENGAWARUNTIMEUTILS_VCPKG_PREFIX}"
    OR NOT IS_DIRECTORY "${ER_ENGAWARUNTIMEUTILS_VCPKG_PREFIX}/lib")
    message(FATAL_ERROR
        "ER_ENGAWARUNTIMEUTILS_VCPKG_PREFIX must be an installed absolute vcpkg triplet prefix.")
endif ()

# The private DLL and every vcpkg archive use the DLL CRT.  An accidental /MT
# archive also fails at link time through MSVC's RuntimeLibrary mismatch guard.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")

find_package(Threads REQUIRED)
find_package(ICU CONFIG REQUIRED)
find_package(ZLIB CONFIG REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(CURL CONFIG REQUIRED)
find_package(libjpeg-turbo CONFIG REQUIRED)
find_package(lcms2 CONFIG REQUIRED)
find_package(LibXml2 CONFIG REQUIRED)
find_package(LibXslt CONFIG REQUIRED)
find_package(PNG CONFIG REQUIRED)
find_package(unofficial-brotli CONFIG REQUIRED)
find_package(unofficial-sqlite3 CONFIG REQUIRED)
find_package(WebP CONFIG REQUIRED)

if (NOT TARGET libjpeg-turbo::jpeg-static)
    message(FATAL_ERROR
        "The isolated libjpeg-turbo package must expose libjpeg-turbo::jpeg-static.")
endif ()
foreach (_er_engawaruntimeutils_icu_target IN ITEMS ICU::dt ICU::in ICU::uc)
    if (NOT TARGET "${_er_engawaruntimeutils_icu_target}")
        message(FATAL_ERROR
            "The isolated ICU package must expose ${_er_engawaruntimeutils_icu_target}.")
    endif ()
endforeach ()

function(_er_engawaruntimeutils_add_vcpkg_archive target_name release_basename debug_basename)
    set(_release_path
        "${ER_ENGAWARUNTIMEUTILS_VCPKG_PREFIX}/lib/${release_basename}.lib")
    set(_debug_path
        "${ER_ENGAWARUNTIMEUTILS_VCPKG_PREFIX}/debug/lib/${debug_basename}.lib")
    if (NOT EXISTS "${_release_path}" OR NOT EXISTS "${_debug_path}")
        message(FATAL_ERROR
            "The isolated vcpkg prefix is missing ${release_basename}.lib or "
            "its Debug archive; both configurations are required.")
    endif ()

    add_library(${target_name} STATIC IMPORTED GLOBAL)
    set_target_properties(${target_name} PROPERTIES
        IMPORTED_CONFIGURATIONS "Debug;Release;RelWithDebInfo;MinSizeRel"
        IMPORTED_LOCATION_DEBUG "${_debug_path}"
        IMPORTED_LOCATION_MINSIZEREL "${_release_path}"
        IMPORTED_LOCATION_RELEASE "${_release_path}"
        IMPORTED_LOCATION_RELWITHDEBINFO "${_release_path}"
    )
endfunction()

# These vcpkg ports do not expose every archive as a CMake package target.
# Wrap the exact static archives and describe their link closure explicitly.
_er_engawaruntimeutils_add_vcpkg_archive(
    EngawaRuntimeUtilsSupport::HarfBuzz harfbuzz harfbuzz)
_er_engawaruntimeutils_add_vcpkg_archive(
    EngawaRuntimeUtilsSupport::HarfBuzzICU harfbuzz-icu harfbuzz-icu)
_er_engawaruntimeutils_add_vcpkg_archive(
    EngawaRuntimeUtilsSupport::LibPSL psl psl)
_er_engawaruntimeutils_add_vcpkg_archive(
    EngawaRuntimeUtilsSupport::NgHttp2 nghttp2 nghttp2)
_er_engawaruntimeutils_add_vcpkg_archive(
    EngawaRuntimeUtilsSupport::NgHttp3 nghttp3 nghttp3)
_er_engawaruntimeutils_add_vcpkg_archive(
    EngawaRuntimeUtilsSupport::NgTcp2 ngtcp2 ngtcp2)
_er_engawaruntimeutils_add_vcpkg_archive(
    EngawaRuntimeUtilsSupport::NgTcp2Crypto ngtcp2_crypto_quictls ngtcp2_crypto_quictls)

set_property(TARGET EngawaRuntimeUtilsSupport::HarfBuzz APPEND PROPERTY
    INTERFACE_LINK_LIBRARIES "ICU::uc;dwrite")
set_property(TARGET EngawaRuntimeUtilsSupport::HarfBuzzICU APPEND PROPERTY
    INTERFACE_LINK_LIBRARIES "EngawaRuntimeUtilsSupport::HarfBuzz;ICU::uc")
set_property(TARGET EngawaRuntimeUtilsSupport::LibPSL APPEND PROPERTY
    INTERFACE_LINK_LIBRARIES "ICU::uc;ws2_32")
set_property(TARGET EngawaRuntimeUtilsSupport::NgHttp2 APPEND PROPERTY
    INTERFACE_LINK_LIBRARIES "ws2_32")
set_property(TARGET EngawaRuntimeUtilsSupport::NgTcp2Crypto APPEND PROPERTY
    INTERFACE_LINK_LIBRARIES
        "EngawaRuntimeUtilsSupport::NgTcp2;OpenSSL::SSL;OpenSSL::Crypto")

# Keep Windows libraries in an imported interface target so the static support
# closure remains private to EngawaRuntimeUtils and cannot leak into SDK consumers.
add_library(EngawaRuntimeUtilsSupport::WindowsSystem INTERFACE IMPORTED GLOBAL)
set_property(TARGET EngawaRuntimeUtilsSupport::WindowsSystem PROPERTY
    INTERFACE_LINK_LIBRARIES
        "advapi32;bcrypt;crypt32;iphlpapi;ntdll;ole32;oleaut32;shell32;"
        "shlwapi;user32;uuid;windowscodecs;ws2_32")

add_library(EngawaRuntimeUtilsSupportDependencies INTERFACE)
target_link_libraries(EngawaRuntimeUtilsSupportDependencies INTERFACE
    # The list intentionally mirrors every non-ANGLE DLL in the accepted r20
    # runtime closure.  Archive members are selected by the .def references;
    # no blanket /WHOLEARCHIVE is used.
    unofficial::brotli::brotlicommon
    unofficial::brotli::brotlidec
    OpenSSL::Crypto
    EngawaRuntimeUtilsSupport::HarfBuzzICU
    EngawaRuntimeUtilsSupport::HarfBuzz
    ICU::dt
    ICU::in
    ICU::uc
    libjpeg-turbo::jpeg-static
    lcms2::lcms2
    CURL::libcurl
    PNG::PNG
    WebP::sharpyuv
    WebP::webp
    WebP::webpdemux
    WebP::libwebpmux
    LibXml2::LibXml2
    LibXslt::LibXslt
    EngawaRuntimeUtilsSupport::NgHttp2
    EngawaRuntimeUtilsSupport::NgHttp3
    EngawaRuntimeUtilsSupport::NgTcp2
    EngawaRuntimeUtilsSupport::NgTcp2Crypto
    EngawaRuntimeUtilsSupport::LibPSL
    unofficial::sqlite3::sqlite3
    OpenSSL::SSL
    ZLIB::ZLIB
    EngawaRuntimeUtilsSupport::WindowsSystem
)

function(_er_engawaruntimeutils_assert_static_target target_name ancestry)
    if (ER_ENGAWARUNTIMEUTILS_ALLOW_DYNAMIC_PREFIX_FOR_CONFIGURE_CHECK)
        return()
    endif ()
    if (NOT TARGET "${target_name}")
        return()
    endif ()
    if ("${target_name}" IN_LIST ancestry)
        return()
    endif ()
    list(APPEND ancestry "${target_name}")

    get_target_property(_aliased_target "${target_name}" ALIASED_TARGET)
    if (_aliased_target)
        _er_engawaruntimeutils_assert_static_target("${_aliased_target}" "${ancestry}")
        return()
    endif ()

    get_target_property(_target_type "${target_name}" TYPE)
    if (_target_type STREQUAL "SHARED_LIBRARY"
        OR _target_type STREQUAL "MODULE_LIBRARY")
        message(FATAL_ERROR
            "${target_name} resolves to ${_target_type}; EngawaRuntimeUtils accepts only static support libraries.")
    endif ()

    get_target_property(_interface_libraries
        "${target_name}" INTERFACE_LINK_LIBRARIES)
    foreach (_interface_library IN LISTS _interface_libraries)
        if (TARGET "${_interface_library}")
            _er_engawaruntimeutils_assert_static_target(
                "${_interface_library}" "${ancestry}")
        endif ()
    endforeach ()
endfunction()

get_target_property(_er_engawaruntimeutils_support_targets
    EngawaRuntimeUtilsSupportDependencies INTERFACE_LINK_LIBRARIES)
foreach (_er_engawaruntimeutils_support_target IN LISTS _er_engawaruntimeutils_support_targets)
    if (TARGET "${_er_engawaruntimeutils_support_target}")
        _er_engawaruntimeutils_assert_static_target(
            "${_er_engawaruntimeutils_support_target}" "")
    endif ()
endforeach ()

if (ER_ENGAWARUNTIMEUTILS_ENABLE_ANGLE)
    if (NOT ER_ENGAWARUNTIMEUTILS_ANGLE_ARCHIVES)
        message(FATAL_ERROR
            "ER_ENGAWARUNTIMEUTILS_ENABLE_ANGLE requires reviewed ANGLE archive paths.")
    endif ()
    add_library(EngawaRuntimeUtilsANGLEClosure INTERFACE)
    set(_er_engawaruntimeutils_angle_index 0)
    foreach (_er_engawaruntimeutils_angle_archive IN LISTS ER_ENGAWARUNTIMEUTILS_ANGLE_ARCHIVES)
        if (NOT IS_ABSOLUTE "${_er_engawaruntimeutils_angle_archive}"
            OR NOT EXISTS "${_er_engawaruntimeutils_angle_archive}")
            message(FATAL_ERROR
                "Every ER_ENGAWARUNTIMEUTILS_ANGLE_ARCHIVES entry must be an existing absolute path: "
                "${_er_engawaruntimeutils_angle_archive}")
        endif ()
        math(EXPR _er_engawaruntimeutils_angle_index
            "${_er_engawaruntimeutils_angle_index} + 1")
        set(_er_engawaruntimeutils_angle_target
            "EngawaRuntimeUtilsANGLEArchive${_er_engawaruntimeutils_angle_index}")
        add_library(${_er_engawaruntimeutils_angle_target} STATIC IMPORTED GLOBAL)
        set_target_properties(${_er_engawaruntimeutils_angle_target} PROPERTIES
            IMPORTED_LOCATION "${_er_engawaruntimeutils_angle_archive}")
        target_link_libraries(EngawaRuntimeUtilsANGLEClosure INTERFACE
            ${_er_engawaruntimeutils_angle_target})
    endforeach ()
    target_link_libraries(EngawaRuntimeUtilsANGLEClosure INTERFACE
        ${ER_ENGAWARUNTIMEUTILS_ANGLE_LINK_LIBRARIES})
endif ()

add_library(EngawaRuntimeUtils SHARED
    EngawaRuntimeUtils.cpp
    "${ER_ENGAWARUNTIMEUTILS_EXPORTS_FILE}"
)
target_compile_features(EngawaRuntimeUtils PRIVATE cxx_std_20)
target_compile_options(EngawaRuntimeUtils PRIVATE /WX)
if (ER_ENGAWARUNTIMEUTILS_PRODUCTION_RELEASE)
    if (NOT ER_ENGAWARUNTIMEUTILS_PRODUCTION_PATH_MAP_ROOT
        OR "${ER_ENGAWARUNTIMEUTILS_PRODUCTION_PATH_MAP_ROOT}" MATCHES
            "[;=\"<>$\r\n]"
        OR NOT IS_ABSOLUTE
            "${ER_ENGAWARUNTIMEUTILS_PRODUCTION_PATH_MAP_ROOT}"
        OR NOT IS_DIRECTORY
            "${ER_ENGAWARUNTIMEUTILS_PRODUCTION_PATH_MAP_ROOT}")
        message(FATAL_ERROR
            "Production EngawaRuntimeUtils requires an existing absolute path-map root.")
    endif ()
    cmake_path(NORMAL_PATH ER_ENGAWARUNTIMEUTILS_PRODUCTION_PATH_MAP_ROOT
        OUTPUT_VARIABLE _er_engawaruntimeutils_map_requested)
    file(REAL_PATH "${ER_ENGAWARUNTIMEUTILS_PRODUCTION_PATH_MAP_ROOT}"
        _er_engawaruntimeutils_map_canonical EXPAND_TILDE)
    cmake_path(NORMAL_PATH _er_engawaruntimeutils_map_canonical)
    string(TOLOWER "${_er_engawaruntimeutils_map_requested}"
        _er_engawaruntimeutils_map_requested_lower)
    string(TOLOWER "${_er_engawaruntimeutils_map_canonical}"
        _er_engawaruntimeutils_map_canonical_lower)
    if (NOT _er_engawaruntimeutils_map_requested_lower STREQUAL
        _er_engawaruntimeutils_map_canonical_lower)
        message(FATAL_ERROR
            "EngawaRuntimeUtils production path-map root must be canonical.")
    endif ()
    cmake_path(GET _er_engawaruntimeutils_map_canonical ROOT_PATH
        _er_engawaruntimeutils_map_root_path)
    cmake_path(IS_PREFIX _er_engawaruntimeutils_map_canonical
        "${CMAKE_SOURCE_DIR}" NORMALIZE _er_engawaruntimeutils_map_contains_source)
    cmake_path(IS_PREFIX _er_engawaruntimeutils_map_canonical
        "${CMAKE_BINARY_DIR}" NORMALIZE _er_engawaruntimeutils_map_contains_binary)
    if (_er_engawaruntimeutils_map_canonical STREQUAL
            _er_engawaruntimeutils_map_root_path
        OR NOT _er_engawaruntimeutils_map_contains_source
        OR NOT _er_engawaruntimeutils_map_contains_binary)
        message(FATAL_ERROR
            "EngawaRuntimeUtils production path-map root must be a non-root parent of its source and build trees.")
    endif ()
    if (NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang"
        OR NOT CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
        message(FATAL_ERROR
            "Production EngawaRuntimeUtils requires the pinned clang-cl frontend.")
    endif ()
    target_compile_options(EngawaRuntimeUtils PRIVATE
        "/clang:-ffile-prefix-map=${_er_engawaruntimeutils_map_canonical}=engawaruntime-production")
    target_link_options(EngawaRuntimeUtils PRIVATE "/DEBUG:NONE")
endif ()
target_link_libraries(EngawaRuntimeUtils PRIVATE EngawaRuntimeUtilsSupportDependencies)
if (ER_ENGAWARUNTIMEUTILS_ENABLE_ANGLE)
    target_link_libraries(EngawaRuntimeUtils PRIVATE EngawaRuntimeUtilsANGLEClosure)
    # ANGLE owns process attach/detach handling in global_state.obj.  Pull that
    # archive member into the DLL without exporting DllMain from the SDK.
    target_link_options(EngawaRuntimeUtils PRIVATE "/INCLUDE:DllMain")
endif ()
set_property(TARGET EngawaRuntimeUtils APPEND PROPERTY
    LINK_DEPENDS "${ER_ENGAWARUNTIMEUTILS_EXPORTS_FILE}")
set_target_properties(EngawaRuntimeUtils PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${ER_ENGAWARUNTIMEUTILS_ARCHIVE_OUTPUT_DIRECTORY}"
    CXX_EXTENSIONS OFF
    CXX_STANDARD_REQUIRED ON
    OUTPUT_NAME "EngawaRuntimeUtils"
    PREFIX ""
    RUNTIME_OUTPUT_DIRECTORY "${ER_ENGAWARUNTIMEUTILS_RUNTIME_OUTPUT_DIRECTORY}"
    WINDOWS_EXPORT_ALL_SYMBOLS OFF
)
foreach (_er_engawaruntimeutils_configuration IN ITEMS
    DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
    set_target_properties(EngawaRuntimeUtils PROPERTIES
        "ARCHIVE_OUTPUT_DIRECTORY_${_er_engawaruntimeutils_configuration}"
            "${ER_ENGAWARUNTIMEUTILS_ARCHIVE_OUTPUT_DIRECTORY}"
        "RUNTIME_OUTPUT_DIRECTORY_${_er_engawaruntimeutils_configuration}"
            "${ER_ENGAWARUNTIMEUTILS_RUNTIME_OUTPUT_DIRECTORY}"
    )
endforeach ()

unset(_er_engawaruntimeutils_exports_extension)
unset(_er_engawaruntimeutils_map_canonical)
unset(_er_engawaruntimeutils_map_canonical_lower)
unset(_er_engawaruntimeutils_map_contains_binary)
unset(_er_engawaruntimeutils_map_contains_source)
unset(_er_engawaruntimeutils_map_root_path)
unset(_er_engawaruntimeutils_map_requested)
unset(_er_engawaruntimeutils_map_requested_lower)
unset(_er_engawaruntimeutils_expected_triplet)
unset(_er_engawaruntimeutils_icu_target)
unset(_er_engawaruntimeutils_support_target)
unset(_er_engawaruntimeutils_support_targets)
