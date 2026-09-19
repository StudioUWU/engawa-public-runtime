include("${CMAKE_SOURCE_DIR}/SDK/EngawaRuntimeVersion.cmake")

if (ER_ENABLE_WEBKIT_ENGINE)
    message(FATAL_ERROR
        "SDK 1.4 disables the legacy ER_ENABLE_WEBKIT_ENGINE profile because it "
        "mixes WebKit C++ with the private runtime. Use ER_ENABLE_INPROCESS_ENGINE=ON "
        "and ER_ENABLE_INPROCESS_SHARED_LIBRARIES=ON.")
endif ()

set(_er_dependency_lock_path "${CMAKE_SOURCE_DIR}/Dependencies/EngawaRuntime.lock.json")
if (NOT EXISTS "${_er_dependency_lock_path}")
    message(FATAL_ERROR "Engawa dependency lock is missing: ${_er_dependency_lock_path}")
endif ()

file(READ "${_er_dependency_lock_path}" _er_dependency_lock_json)
string(JSON _er_lock_schema GET "${_er_dependency_lock_json}" schema_version)
string(JSON _er_lock_product GET "${_er_dependency_lock_json}" product)
string(JSON _er_lock_webkit_sha GET "${_er_dependency_lock_json}" webkit revision)
if (NOT _er_lock_schema EQUAL 1
    OR NOT "${_er_lock_product}" STREQUAL "EngawaRuntime"
    OR NOT "${_er_lock_webkit_sha}" STREQUAL "${ER_WEBKIT_SHA}")
    message(FATAL_ERROR
        "Engawa dependency lock identity does not match SDK/WebKit ${ER_WEBKIT_SHA}.")
endif ()

foreach (_er_lock_input IN ITEMS vcpkg_manifest vcpkg_configuration)
    string(JSON _er_lock_input_path
        GET "${_er_dependency_lock_json}" inputs ${_er_lock_input} path)
    string(JSON _er_lock_input_sha256
        GET "${_er_dependency_lock_json}" inputs ${_er_lock_input} sha256)
    set(_er_lock_input_absolute "${CMAKE_SOURCE_DIR}/${_er_lock_input_path}")
    if (NOT EXISTS "${_er_lock_input_absolute}")
        message(FATAL_ERROR "Pinned dependency input is missing: ${_er_lock_input_path}")
    endif ()
    file(SHA256 "${_er_lock_input_absolute}" _er_lock_input_actual_sha256)
    if (NOT "${_er_lock_input_actual_sha256}" STREQUAL "${_er_lock_input_sha256}")
        message(FATAL_ERROR
            "Pinned dependency input hash mismatch for ${_er_lock_input_path}: "
            "expected ${_er_lock_input_sha256}, got ${_er_lock_input_actual_sha256}.")
    endif ()
endforeach ()

set(_er_feature_policy_path "${CMAKE_SOURCE_DIR}/SDK/EngawaRuntimeFeaturePolicy.json")
if (NOT EXISTS "${_er_feature_policy_path}")
    message(FATAL_ERROR "Engawa feature policy is missing: ${_er_feature_policy_path}")
endif ()

file(READ "${_er_feature_policy_path}" _er_feature_policy_json)
string(JSON _er_policy_schema GET "${_er_feature_policy_json}" schema_version)
string(JSON _er_policy_product GET "${_er_feature_policy_json}" product)
string(JSON _er_policy_sdk_major GET "${_er_feature_policy_json}" sdk_major)
string(JSON _er_policy_webkit_sha GET "${_er_feature_policy_json}" webkit_base_sha)

if (NOT _er_policy_schema EQUAL 1)
    message(FATAL_ERROR "Unsupported Engawa feature-policy schema: ${_er_policy_schema}")
endif ()
if (NOT "${_er_policy_product}" STREQUAL "EngawaRuntime")
    message(FATAL_ERROR "Unexpected Engawa feature-policy product: ${_er_policy_product}")
endif ()
if (NOT _er_policy_sdk_major EQUAL 1)
    message(FATAL_ERROR "Unexpected Engawa feature-policy SDK major: ${_er_policy_sdk_major}")
endif ()
if (NOT "${_er_policy_webkit_sha}" STREQUAL "${ER_WEBKIT_SHA}")
    message(FATAL_ERROR
        "Engawa feature policy is for WebKit ${_er_policy_webkit_sha}, expected ${ER_WEBKIT_SHA}.")
endif ()

string(JSON _er_locked_feature_count
    LENGTH "${_er_feature_policy_json}" locked_off_cmake_options)
if (_er_locked_feature_count LESS 1)
    message(FATAL_ERROR "Engawa feature policy has no locked_off_cmake_options.")
endif ()

math(EXPR _er_locked_feature_last "${_er_locked_feature_count} - 1")
set(ENGAWARUNTIME_LOCKED_OFF_FEATURES)
foreach (_er_index RANGE 0 ${_er_locked_feature_last})
    string(JSON _er_feature_name
        GET "${_er_feature_policy_json}" locked_off_cmake_options ${_er_index})
    if (NOT _er_feature_name MATCHES "^ENABLE_[A-Z0-9_]+$")
        message(FATAL_ERROR "Invalid Engawa feature-policy option name: ${_er_feature_name}")
    endif ()
    list(APPEND ENGAWARUNTIME_LOCKED_OFF_FEATURES "${_er_feature_name}")
endforeach ()

set(_er_sorted_locked_features ${ENGAWARUNTIME_LOCKED_OFF_FEATURES})
list(SORT _er_sorted_locked_features)
if (NOT "${ENGAWARUNTIME_LOCKED_OFF_FEATURES}" STREQUAL "${_er_sorted_locked_features}")
    message(FATAL_ERROR "Engawa feature-policy options must be lexically sorted.")
endif ()
list(LENGTH ENGAWARUNTIME_LOCKED_OFF_FEATURES _er_locked_feature_list_count)
list(REMOVE_DUPLICATES _er_sorted_locked_features)
list(LENGTH _er_sorted_locked_features _er_unique_locked_feature_count)
if (NOT _er_locked_feature_list_count EQUAL _er_unique_locked_feature_count)
    message(FATAL_ERROR "Engawa feature-policy options must be unique.")
endif ()

# These cache entries must exist before OptionsJSCOnly calls WEBKIT_OPTION_END(),
# because that macro computes FEATURE_DEFINES from the resulting option values.
foreach (_er_feature IN LISTS ENGAWARUNTIME_LOCKED_OFF_FEATURES)
    if (DEFINED CACHE{${_er_feature}})
        get_property(_er_requested_value CACHE "${_er_feature}" PROPERTY VALUE)
        if (_er_requested_value)
            message(FATAL_ERROR
                "Engawa 1.0 requires ${_er_feature}=OFF (requested '${_er_requested_value}').")
        endif ()
    endif ()
    set(${_er_feature} OFF CACHE BOOL "Locked OFF by the Engawa 1.0 product boundary." FORCE)
    set(${_er_feature} OFF)
endforeach ()

option(ER_ENABLE_WEBKIT_ENGINE
    "Unsupported legacy multi-process profile; rejected by SDK 1.4."
    OFF)
option(ER_ENABLE_INPROCESS_ENGINE
    "Build the strict in-process WebCore engine profile without WebKit helpers."
    OFF)
option(ER_ENABLE_INPROCESS_MONOLITHIC_LINK
    "Removed in SDK 1.4: monolithic private WebKit linkage is forbidden."
    OFF)
option(ER_ENABLE_INPROCESS_SHARED_LIBRARIES
    "Build replaceable WebCore and JavaScriptCore libraries." ${ER_ENABLE_INPROCESS_ENGINE})
option(ER_BUILD_PUBLIC_ENGINE_ONLY
    "Build the corresponding-source engine without proprietary runtime sources." OFF)
option(ER_ENABLE_INPROCESS_UTILS_DLL
    "Fold the strict in-process runtime's non-WebKit dependencies into EngawaRuntimeUtils."
    OFF)
option(ER_ENABLE_WATERMARK
    "Compile the build-identity frame watermark into the strict in-process renderer."
    OFF)
option(ER_MACOS_UNIVERSAL_SLICE
    "Build one thin arm64 or x86_64 slice for the guarded macOS universal pipeline."
    OFF)
set(ER_WATERMARK_BUILD_COMMIT "" CACHE STRING
    "Clean monorepo Git commit embedded by a --watermark SDK build.")
option(ER_PRODUCTION_RELEASE
    "Build public release binaries without host-specific source paths or public debug records."
    OFF)
set(ER_PRODUCTION_PATH_MAP_ROOT "" CACHE PATH
    "Canonical build-workspace root removed from production compiler paths.")

if (ER_ENABLE_WEBKIT_ENGINE AND ER_ENABLE_INPROCESS_ENGINE)
    message(FATAL_ERROR
        "ER_ENABLE_WEBKIT_ENGINE and ER_ENABLE_INPROCESS_ENGINE are mutually exclusive.")
endif ()
if (ER_ENABLE_INPROCESS_MONOLITHIC_LINK)
    message(FATAL_ERROR
        "SDK 1.4 requires replaceable shared WebCore/JavaScriptCore; set ER_ENABLE_INPROCESS_MONOLITHIC_LINK=OFF.")
endif ()
if (ER_ENABLE_INPROCESS_ENGINE AND NOT ER_ENABLE_INPROCESS_SHARED_LIBRARIES)
    message(FATAL_ERROR "SDK 1.4 requires ER_ENABLE_INPROCESS_SHARED_LIBRARIES=ON.")
endif ()
if (ER_ENABLE_INPROCESS_SHARED_LIBRARIES AND NOT ER_ENABLE_INPROCESS_ENGINE)
    message(FATAL_ERROR "Shared engine libraries require ER_ENABLE_INPROCESS_ENGINE=ON.")
endif ()
if (ER_BUILD_PUBLIC_ENGINE_ONLY AND NOT ER_ENABLE_INPROCESS_ENGINE)
    message(FATAL_ERROR "The public engine build requires ER_ENABLE_INPROCESS_ENGINE=ON.")
endif ()
if (ER_MACOS_UNIVERSAL_SLICE AND NOT APPLE)
    message(FATAL_ERROR
        "ER_MACOS_UNIVERSAL_SLICE is accepted only by the macOS SDK pipeline.")
endif ()
if (ER_ENABLE_WATERMARK)
    if (NOT ER_ENABLE_INPROCESS_ENGINE)
        message(FATAL_ERROR
            "ER_ENABLE_WATERMARK requires ER_ENABLE_INPROCESS_ENGINE=ON.")
    endif ()
    string(LENGTH "${ER_WATERMARK_BUILD_COMMIT}" _er_watermark_commit_length)
    if (NOT ER_WATERMARK_BUILD_COMMIT MATCHES "^[0-9a-f]+$"
        OR (NOT _er_watermark_commit_length EQUAL 40
            AND NOT _er_watermark_commit_length EQUAL 64))
        message(FATAL_ERROR
            "ER_ENABLE_WATERMARK requires ER_WATERMARK_BUILD_COMMIT to be one "
            "complete lower-case 40- or 64-character Git object ID.")
    endif ()
    math(EXPR _er_watermark_suffix_offset "${_er_watermark_commit_length} - 5")
    string(SUBSTRING "${ER_WATERMARK_BUILD_COMMIT}"
        ${_er_watermark_suffix_offset} 5 ER_WATERMARK_COMMIT_SUFFIX)
    add_compile_definitions(
        ER_ENABLE_WATERMARK=1
        "ER_WATERMARK_COMMIT_SUFFIX=\"${ER_WATERMARK_COMMIT_SUFFIX}\""
    )
    unset(_er_watermark_commit_length)
    unset(_er_watermark_suffix_offset)
elseif (NOT ER_WATERMARK_BUILD_COMMIT STREQUAL "")
    message(FATAL_ERROR
        "ER_WATERMARK_BUILD_COMMIT must be empty when ER_ENABLE_WATERMARK=OFF.")
endif ()
if (ER_ENABLE_INPROCESS_UTILS_DLL)
    if (NOT WIN32)
        message(FATAL_ERROR
            "ER_ENABLE_INPROCESS_UTILS_DLL is currently Windows-only.")
    endif ()
    if (NOT ER_ENABLE_INPROCESS_ENGINE
        OR NOT ER_ENABLE_INPROCESS_SHARED_LIBRARIES)
        message(FATAL_ERROR
            "ER_ENABLE_INPROCESS_UTILS_DLL requires the split in-process engine.")
    endif ()
    if (NOT DEFINED VCPKG_TARGET_TRIPLET
        OR NOT "${VCPKG_TARGET_TRIPLET}" STREQUAL "x64-windows-webkit")
        message(FATAL_ERROR
            "ER_ENABLE_INPROCESS_UTILS_DLL must keep the parent WebKit graph on "
            "the accepted x64-windows-webkit triplet.")
    endif ()
endif ()

set(_er_profile_disabled_targets
    ENABLE_API_TESTS
    ENABLE_WEBINSPECTORUI
    ENABLE_WEBKIT_LEGACY
)
if (ER_ENABLE_INPROCESS_ENGINE)
    list(APPEND _er_profile_disabled_targets
        ENABLE_WEBKIT
    )
elseif (NOT ER_ENABLE_WEBKIT_ENGINE)
    list(APPEND _er_profile_disabled_targets
        ENABLE_WEBCORE
        ENABLE_WEBKIT
    )
endif ()
foreach (_er_option IN LISTS _er_profile_disabled_targets)
    set(${_er_option} OFF CACHE BOOL "Disabled by the selected Engawa runtime profile." FORCE)
    set(${_er_option} OFF)
endforeach ()
if (ER_ENABLE_WEBKIT_ENGINE)
    if (NOT WIN32)
        message(FATAL_ERROR
            "The first EngawaRuntime engine profile is Windows-only; "
            "configure other platforms with ER_ENABLE_WEBKIT_ENGINE=OFF.")
    endif ()
    set(ENABLE_WEBCORE ON CACHE BOOL "Required by the Engawa Windows engine profile." FORCE)
    set(ENABLE_WEBCORE ON)
    set(ENABLE_WEBKIT ON CACHE BOOL "Required by the Engawa Windows engine profile." FORCE)
    set(ENABLE_WEBKIT ON)
elseif (ER_ENABLE_INPROCESS_ENGINE)
    if (NOT WIN32 AND NOT APPLE
        AND NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
        message(FATAL_ERROR
            "The strict in-process WebCore adapter supports only Windows, "
            "macOS, and Linux x86_64.")
    endif ()
    set(ENABLE_WEBCORE ON CACHE BOOL
        "Required by the Engawa strict in-process engine profile." FORCE)
    set(ENABLE_WEBCORE ON)
    set(ENABLE_WEBKIT OFF CACHE BOOL
        "WebKit2 is forbidden by the Engawa strict in-process engine profile." FORCE)
    set(ENABLE_WEBKIT OFF)
    set(ENABLE_WEBKIT_LEGACY OFF CACHE BOOL
        "WebKitLegacy is unavailable in the Engawa strict in-process engine profile." FORCE)
    set(ENABLE_WEBKIT_LEGACY OFF)
    set(USE_JPEGXL OFF CACHE BOOL
        "JPEG XL is outside the Engawa runtime product boundary." FORCE)
    set(USE_JPEGXL OFF)
endif ()
set(ENABLE_TOOLS ON CACHE BOOL "Build the selected Engawa runtime tool targets." FORCE)
set(ENABLE_TOOLS ON)

if (APPLE AND ER_ENABLE_INPROCESS_ENGINE AND NOT ENABLE_PAYMENT_REQUEST)
    # OptionsMac defaults the whole Apple Pay family ON and calls
    # WEBKIT_OPTION_END() internally. Seed every cache entry before that call
    # so FEATURE_DEFINES and generated-source dependencies are computed from
    # the locked-off product boundary, rather than corrected after the fact.
    set(_er_apple_pay_features
        ENABLE_APPLE_PAY
        ENABLE_APPLE_PAY_AUTOMATIC_RELOAD_LINE_ITEM
        ENABLE_APPLE_PAY_AUTOMATIC_RELOAD_PAYMENTS
        ENABLE_APPLE_PAY_COUPON_CODE
        ENABLE_APPLE_PAY_DEFERRED_LINE_ITEM
        ENABLE_APPLE_PAY_DEFERRED_PAYMENTS
        ENABLE_APPLE_PAY_DELEGATED_REQUEST
        ENABLE_APPLE_PAY_DISBURSEMENTS
        ENABLE_APPLE_PAY_INSTALLMENTS
        ENABLE_APPLE_PAY_LATER
        ENABLE_APPLE_PAY_LATER_AVAILABILITY
        ENABLE_APPLE_PAY_MERCHANT_CATEGORY_CODE
        ENABLE_APPLE_PAY_MULTI_MERCHANT_PAYMENTS
        ENABLE_APPLE_PAY_PAYMENT_ORDER_DETAILS
        ENABLE_APPLE_PAY_RECURRING_LINE_ITEM
        ENABLE_APPLE_PAY_RECURRING_PAYMENTS
        ENABLE_APPLE_PAY_SELECTED_SHIPPING_METHOD
        ENABLE_APPLE_PAY_SHIPPING_CONTACT_EDITING_MODE
        ENABLE_APPLE_PAY_SHIPPING_METHOD_DATE_COMPONENTS_RANGE
    )
    foreach (_er_apple_pay_feature IN LISTS _er_apple_pay_features)
        set(${_er_apple_pay_feature} OFF CACHE BOOL
            "Disabled with Payment Request by the Engawa product boundary." FORCE)
        set(${_er_apple_pay_feature} OFF)
    endforeach ()
endif ()

if (ER_ENABLE_WEBKIT_ENGINE OR ER_ENABLE_INPROCESS_ENGINE)
    if (WIN32)
        include(OptionsWin)
    elseif (APPLE AND ER_ENABLE_INPROCESS_ENGINE)
        include(OptionsMac)
    elseif (CMAKE_SYSTEM_NAME STREQUAL "Linux"
        AND ER_ENABLE_INPROCESS_ENGINE)
        # WPE supplies WebKit's maintained Linux/GLib/Soup/Skia platform
        # foundation. Engawa consumes only its lower WebCore layers: WebKit2,
        # helper processes, platform shells, media, and GPU presentation stay
        # outside the strict direct-WebCore product boundary.
        set(_er_linux_wpe_locked_off_options
            ENABLE_BUBBLEWRAP_SANDBOX
            ENABLE_COG
            ENABLE_DOCUMENTATION
            ENABLE_INTROSPECTION
            ENABLE_JSC_RESTRICTED_OPTIONS_BY_DEFAULT
            ENABLE_JOURNALD_LOG
            ENABLE_MEMORY_SAMPLER
            ENABLE_RESOURCE_USAGE
            ENABLE_WPE_1_1_API
            ENABLE_WPE_PLATFORM
            ENABLE_WPE_PLATFORM_DRM
            ENABLE_WPE_PLATFORM_HEADLESS
            ENABLE_WPE_PLATFORM_WAYLAND
            ENABLE_WPE_QT_API
            USE_ATK
            USE_EXTERNAL_HOLEPUNCH
            USE_FLITE
            USE_GBM
            USE_GSTREAMER
            USE_GSTREAMER_FULL
            USE_GSTREAMER_GL
            USE_GSTREAMER_MPEGTS
            USE_GSTREAMER_WEBRTC
            USE_LIBBACKTRACE
            USE_LIBDRM
            USE_LIBHYPHEN
            USE_LIBRICE
            USE_SPIEL
            USE_SYSPROF_CAPTURE
            USE_SYSTEM_SYSPROF_CAPTURE
            USE_VULKAN
        )
        foreach (_er_linux_wpe_option IN LISTS
            _er_linux_wpe_locked_off_options)
            if (DEFINED CACHE{${_er_linux_wpe_option}})
                get_property(_er_linux_wpe_requested_value CACHE
                    "${_er_linux_wpe_option}" PROPERTY VALUE)
                if (_er_linux_wpe_requested_value)
                    message(FATAL_ERROR
                        "The Engawa Linux direct-WebCore profile requires "
                        "${_er_linux_wpe_option}=OFF (requested "
                        "'${_er_linux_wpe_requested_value}').")
                endif ()
            endif ()
            set(${_er_linux_wpe_option} OFF CACHE BOOL
                "Locked OFF by the Engawa Linux direct-WebCore profile." FORCE)
            set(${_er_linux_wpe_option} OFF)
        endforeach ()

        if (DEFINED CACHE{ENABLE_WPE_LEGACY_API})
            get_property(_er_linux_wpe_legacy_requested CACHE
                ENABLE_WPE_LEGACY_API PROPERTY VALUE)
            if (NOT _er_linux_wpe_legacy_requested)
                message(FATAL_ERROR
                    "The Engawa Linux direct-WebCore profile requires "
                    "ENABLE_WPE_LEGACY_API=ON.")
            endif ()
        endif ()
        set(ENABLE_WPE_LEGACY_API ON CACHE BOOL
            "Required as the Engawa Linux WebCore platform foundation." FORCE)
        set(ENABLE_WPE_LEGACY_API ON)

        include(OptionsWPE)

        # FindWPE creates an imported target for the selected archive, but it
        # does not preserve the Requires closure from libwpe's pkg-config
        # metadata.  That closure is part of the locked static libwpe contract:
        # its optional XKB object needs xkbcommon and its public EGL surface
        # needs EGL.  Attach both explicitly so a newly selected archive member
        # cannot turn into a late, order-dependent final-link failure.
        # WebKit's own FindOpenGL module intentionally has no EGL component;
        # use its dedicated FindEGL module for the libwpe pkg-config closure.
        find_package(EGL REQUIRED)
        foreach (_er_linux_wpe_target IN ITEMS
            WPE::libwpe
            XkbCommon::XkbCommon)
            if (NOT TARGET ${_er_linux_wpe_target})
                message(FATAL_ERROR
                    "The Engawa Linux static libwpe closure requires target "
                    "${_er_linux_wpe_target}.")
            endif ()
        endforeach ()
        if (NOT EGL_LIBRARIES)
            message(FATAL_ERROR
                "The Engawa Linux static libwpe closure requires EGL libraries.")
        endif ()
        set_property(TARGET WPE::libwpe APPEND PROPERTY
            INTERFACE_LINK_LIBRARIES
                XkbCommon::XkbCommon
                ${EGL_LIBRARIES}
        )
        unset(_er_linux_wpe_target)

        foreach (_er_linux_wpe_option IN LISTS
            _er_linux_wpe_locked_off_options)
            if (${_er_linux_wpe_option})
                message(FATAL_ERROR
                    "Engawa Linux WPE lock failed: "
                    "${_er_linux_wpe_option} is enabled.")
            endif ()
        endforeach ()
        if (NOT ENABLE_WPE_LEGACY_API OR ENABLE_WPE_PLATFORM
            OR NOT USE_SKIA OR NOT USE_ATSPI OR USE_GSTREAMER)
            message(FATAL_ERROR
                "The Engawa Linux WPE foundation did not preserve its locked "
                "legacy-headless Skia/AT-SPI configuration.")
        endif ()
    else ()
        message(FATAL_ERROR
            "The selected Engawa engine profile has no options for ${CMAKE_SYSTEM_NAME}.")
    endif ()
    if (ER_ENABLE_INPROCESS_ENGINE)
        # WebKit2 normally creates this imported target through its own
        # dependency path.  The direct-WebCore profile omits WebKit2 while
        # WTF and JavaScriptCore still link the platform thread target.
        find_package(Threads REQUIRED)
    endif ()
    # Upstream engine ports may select a GPU-process drawing area. Neither
    # Engawa engine profile exposes that process.
    SET_AND_EXPOSE_TO_BUILD(USE_GRAPHICS_LAYER_WC OFF)
else ()
    include(OptionsJSCOnly)
endif ()

if (ER_PRODUCTION_RELEASE)
    if (NOT ER_PRODUCTION_PATH_MAP_ROOT
        OR "${ER_PRODUCTION_PATH_MAP_ROOT}" MATCHES "[;=\"<>$\r\n]")
        message(FATAL_ERROR
            "ER_PRODUCTION_RELEASE requires one canonical path in "
            "ER_PRODUCTION_PATH_MAP_ROOT.")
    endif ()
    cmake_path(IS_ABSOLUTE ER_PRODUCTION_PATH_MAP_ROOT
        _er_production_map_is_absolute)
    if (NOT _er_production_map_is_absolute
        OR NOT IS_DIRECTORY "${ER_PRODUCTION_PATH_MAP_ROOT}")
        message(FATAL_ERROR
            "ER_PRODUCTION_PATH_MAP_ROOT must be an existing absolute directory: "
            "${ER_PRODUCTION_PATH_MAP_ROOT}")
    endif ()
    cmake_path(NORMAL_PATH ER_PRODUCTION_PATH_MAP_ROOT
        OUTPUT_VARIABLE _er_production_map_requested)
    file(REAL_PATH "${ER_PRODUCTION_PATH_MAP_ROOT}"
        _er_production_map_canonical EXPAND_TILDE)
    cmake_path(NORMAL_PATH _er_production_map_canonical)
    set(_er_production_map_requested_compare
        "${_er_production_map_requested}")
    set(_er_production_map_canonical_compare
        "${_er_production_map_canonical}")
    if (WIN32)
        string(TOLOWER "${_er_production_map_requested_compare}"
            _er_production_map_requested_compare)
        string(TOLOWER "${_er_production_map_canonical_compare}"
            _er_production_map_canonical_compare)
    endif ()
    if (NOT _er_production_map_requested_compare STREQUAL
        _er_production_map_canonical_compare)
        message(FATAL_ERROR
            "ER_PRODUCTION_PATH_MAP_ROOT must already be canonical: requested "
            "${_er_production_map_requested}, resolved "
            "${_er_production_map_canonical}")
    endif ()
    cmake_path(GET _er_production_map_canonical ROOT_PATH
        _er_production_map_root_path)
    if (_er_production_map_canonical STREQUAL _er_production_map_root_path)
        message(FATAL_ERROR
            "ER_PRODUCTION_PATH_MAP_ROOT cannot be a filesystem root.")
    endif ()
    cmake_path(IS_PREFIX _er_production_map_canonical "${CMAKE_SOURCE_DIR}"
        NORMALIZE _er_production_map_contains_source)
    cmake_path(IS_PREFIX _er_production_map_canonical "${CMAKE_BINARY_DIR}"
        NORMALIZE _er_production_map_contains_binary)
    if (NOT _er_production_map_contains_source
        OR NOT _er_production_map_contains_binary)
        message(FATAL_ERROR
            "ER_PRODUCTION_PATH_MAP_ROOT must contain both the source and build trees.")
    endif ()
    foreach (_er_production_language IN ITEMS C CXX)
        if (NOT CMAKE_${_er_production_language}_COMPILER_ID MATCHES
            "^(AppleClang|Clang)$")
            message(FATAL_ERROR
                "Production path remapping requires Clang for "
                "${_er_production_language}, got "
                "${CMAKE_${_er_production_language}_COMPILER_ID}.")
        endif ()
    endforeach ()
    if (WIN32)
        if (NOT CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
            message(FATAL_ERROR
                "Windows production builds require the pinned clang-cl frontend.")
        endif ()
        set(_er_production_prefix_map
            "/clang:-ffile-prefix-map=${_er_production_map_canonical}=engawaruntime-production")
    else ()
        set(_er_production_prefix_map
            "-ffile-prefix-map=${_er_production_map_canonical}=engawaruntime-production")
    endif ()
    add_compile_options(
        "$<$<COMPILE_LANGUAGE:C,CXX,OBJC,OBJCXX>:${_er_production_prefix_map}>")
    if (WIN32)
        # OptionsMSVC requests a diagnostic CodeView/PDB link for ordinary
        # builds. This later option deliberately makes production PE images
        # contain no public debug directory or developer-machine PDB path.
        add_link_options("/DEBUG:NONE")
    endif ()
endif ()

if (ER_ENABLE_INPROCESS_SHARED_LIBRARIES)
    # WebKit's linked-into framework mechanism assigns bmalloc/WTF exactly
    # once to JavaScriptCore and PAL exactly once to WebCore. In particular,
    # never link another private copy of WTF state into EngawaRuntime.
    set(bmalloc_LIBRARY_TYPE OBJECT)
    set(WTF_LIBRARY_TYPE OBJECT)
    set(JavaScriptCore_LIBRARY_TYPE SHARED)
    set(PAL_LIBRARY_TYPE OBJECT)
    set(WebCore_LIBRARY_TYPE SHARED)
    add_compile_definitions(ER_INPROCESS_EMBEDDED_ENGINE=1)
    if (APPLE)
        if (USE_APPLE_ICU)
            message(FATAL_ERROR
                "The Engawa macOS split profile requires versioned ICU libraries; "
                "configure with USE_APPLE_ICU=OFF and the pinned ICU archive paths.")
        endif ()
        # Upstream Cocoa disables ICU symbol renaming for Apple's unversioned
        # libicucore. The Engawa package instead statically links its pinned ICU
        # release, so every source in the monolithic graph must use that
        # release's versioned entry points consistently.
        add_compile_definitions(ER_USE_VERSIONED_ICU=1)
    endif ()
endif ()

if (APPLE AND ER_ENABLE_INPROCESS_ENGINE)
    # OptionsCocoa selects the implementation library independently of the Web
    # API switch. Match the implementation graph to the locked-off WebRTC API.
    SET_AND_EXPOSE_TO_BUILD(USE_LIBWEBRTC OFF)

    if (NOT ENABLE_WEBGPU)
        add_compile_definitions(ER_DISABLE_WEBGPU_IMPLEMENTATION=1)
    endif ()

    # Verify that OptionsMac kept the pre-seeded Apple Pay family out of both
    # its finalized option values and generated-source feature dependencies.
    if (NOT ENABLE_PAYMENT_REQUEST)
        foreach (_er_apple_pay_feature IN LISTS _er_apple_pay_features)
            list(FIND _WEBKIT_AVAILABLE_OPTIONS "${_er_apple_pay_feature}"
                _er_apple_pay_option_index)
            if (_er_apple_pay_option_index EQUAL -1)
                message(FATAL_ERROR
                    "Engawa Apple Pay lock names an unknown option: ${_er_apple_pay_feature}")
            endif ()
            if (${_er_apple_pay_feature})
                message(FATAL_ERROR
                    "Engawa Apple Pay lock failed: ${_er_apple_pay_feature} is enabled.")
            endif ()
            list(FIND FEATURE_DEFINES "${_er_apple_pay_feature}"
                _er_apple_pay_feature_define_index)
            if (NOT _er_apple_pay_feature_define_index EQUAL -1)
                message(FATAL_ERROR
                    "Engawa Apple Pay lock was applied after FEATURE_DEFINES: "
                    "${_er_apple_pay_feature}")
            endif ()
        endforeach ()
    endif ()

    # PlatformEnableCocoa otherwise derives these as enabled from PLATFORM(MAC)
    # even though their parent product features are locked off above. Expose
    # explicit zeroes before cmakeconfig.h is generated so the Cocoa defaults
    # cannot create an internally inconsistent feature set.
    foreach (_er_cocoa_derived_feature IN ITEMS
        ENABLE_APPLE_PAY_AMS_UI
        ENABLE_APPLE_PAY_NEW_BUTTON_TYPES
        ENABLE_APPLE_PAY_SESSION_V11
        ENABLE_APPLE_PAY_SETUP
        ENABLE_APPLE_PAY_UPDATE_SHIPPING_METHODS_WHEN_CHANGING_LINE_ITEMS
        ENABLE_COCOA_WEBM_PLAYER
        ENABLE_DECLARATIVE_WEB_PUSH
        ENABLE_NOTIFICATION_EVENT
        ENABLE_PAYMENT_REQUEST_SELECTED_SHIPPING_OPTION
    )
        set(${_er_cocoa_derived_feature} OFF CACHE BOOL
            "Disabled by the Engawa strict in-process feature boundary." FORCE)
        SET_AND_EXPOSE_TO_BUILD(${_er_cocoa_derived_feature} OFF)
    endforeach ()

endif ()

# The upstream port options have platform-specific assignments (notably
# ENABLE_API_TESTS), and the engine ports set ENABLE_WEBKIT directly. Reapply
# and verify the selected profile boundary after those options have run.
foreach (_er_option IN LISTS _er_profile_disabled_targets)
    set(${_er_option} OFF CACHE BOOL "Disabled by the selected Engawa runtime profile." FORCE)
    set(${_er_option} OFF)
    if (${_er_option})
        message(FATAL_ERROR "Engawa runtime-profile lock failed: ${_er_option} is enabled.")
    endif ()
endforeach ()

if (ER_ENABLE_WEBKIT_ENGINE)
    set(ENABLE_WEBCORE ON CACHE BOOL "Required by the Engawa Windows engine profile." FORCE)
    set(ENABLE_WEBCORE ON)
    set(ENABLE_WEBKIT ON CACHE BOOL "Required by the Engawa Windows engine profile." FORCE)
    set(ENABLE_WEBKIT ON)
elseif (ER_ENABLE_INPROCESS_ENGINE)
    set(ENABLE_WEBCORE ON CACHE BOOL
        "Required by the Engawa strict in-process engine profile." FORCE)
    set(ENABLE_WEBCORE ON)
    set(ENABLE_WEBKIT OFF CACHE BOOL
        "WebKit2 is forbidden by the Engawa strict in-process engine profile." FORCE)
    set(ENABLE_WEBKIT OFF)
    set(ENABLE_WEBKIT_LEGACY OFF CACHE BOOL
        "WebKitLegacy is unavailable in the Engawa strict in-process engine profile." FORCE)
    set(ENABLE_WEBKIT_LEGACY OFF)
    SET_AND_EXPOSE_TO_BUILD(USE_GRAPHICS_LAYER_WC OFF)
endif ()

foreach (_er_feature IN LISTS ENGAWARUNTIME_LOCKED_OFF_FEATURES)
    list(FIND _WEBKIT_AVAILABLE_OPTIONS "${_er_feature}" _er_feature_index)
    if (_er_feature_index EQUAL -1)
        message(FATAL_ERROR
            "Engawa feature-policy option is not defined by this WebKit revision: ${_er_feature}")
    endif ()
    set(${_er_feature} OFF CACHE BOOL "Locked OFF by the Engawa 1.0 product boundary." FORCE)
    set(${_er_feature} OFF)
    if (${_er_feature})
        message(FATAL_ERROR "Engawa feature lock failed: ${_er_feature} is enabled.")
    endif ()
endforeach ()

option(ENABLE_ENGAWARUNTIME_TESTS "Build the EngawaRuntime bootstrap tests." ON)

if (WIN32)
    if (NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
        message(FATAL_ERROR "The Engawa Windows port supports only x64 builds.")
    endif ()
    set(ER_PLATFORM_TRIPLE "windows-x86_64-msvc")
elseif (APPLE)
    if (CMAKE_OSX_ARCHITECTURES)
        set(_er_apple_architecture "${CMAKE_OSX_ARCHITECTURES}")
    else ()
        string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" _er_apple_architecture)
    endif ()
    string(TOLOWER "${_er_apple_architecture}" _er_apple_architecture)
    if (ER_MACOS_UNIVERSAL_SLICE)
        if (NOT _er_apple_architecture STREQUAL "arm64"
            AND NOT _er_apple_architecture STREQUAL "aarch64"
            AND NOT _er_apple_architecture STREQUAL "x86_64")
            message(FATAL_ERROR
                "Each guarded Engawa macOS universal build tree must contain "
                "exactly one arm64 or x86_64 slice.")
        endif ()
        string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" _er_apple_system_processor)
        if (_er_apple_architecture STREQUAL "x86_64")
            if (NOT _er_apple_system_processor STREQUAL "x86_64"
                OR NOT WTF_CPU_X86_64
                OR WTF_CPU_ARM64)
                message(FATAL_ERROR
                    "The guarded x86_64 macOS slice must configure WebKit's "
                    "x86_64 CPU policy; set CMAKE_APPLE_SILICON_PROCESSOR=x86_64.")
            endif ()
        elseif (NOT _er_apple_system_processor MATCHES "^(arm64|aarch64)$"
            OR NOT WTF_CPU_ARM64
            OR WTF_CPU_X86_64)
            message(FATAL_ERROR
                "The guarded arm64 macOS slice must configure WebKit's arm64 CPU policy.")
        endif ()
        set(ER_PLATFORM_TRIPLE "macos-universal2")
    else ()
        if (NOT _er_apple_architecture STREQUAL "arm64"
            AND NOT _er_apple_architecture STREQUAL "aarch64")
            message(FATAL_ERROR
                "The ordinary Engawa macOS build supports only arm64; use the "
                "guarded --universal pipeline for x86_64.")
        endif ()
        set(ER_PLATFORM_TRIPLE "macos-arm64")
    endif ()
elseif (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" _er_linux_architecture)
    if (NOT CMAKE_SIZEOF_VOID_P EQUAL 8
        OR NOT _er_linux_architecture MATCHES "^(x86_64|amd64)$")
        message(FATAL_ERROR "The Engawa Linux port supports only x86_64 builds.")
    endif ()
    set(ER_PLATFORM_TRIPLE "linux-x86_64")
else ()
    message(FATAL_ERROR "The Engawa port does not support ${CMAKE_SYSTEM_NAME}.")
endif ()

if (NOT ER_BUILD_PUBLIC_ENGINE_ONLY)
find_program(ER_GIT_EXECUTABLE NAMES git REQUIRED)
execute_process(
    COMMAND "${ER_GIT_EXECUTABLE}" -C "${CMAKE_SOURCE_DIR}" rev-parse --verify HEAD
    RESULT_VARIABLE _er_git_head_result
    OUTPUT_VARIABLE ER_PATCH_HEAD_SHA
    ERROR_VARIABLE _er_git_head_error
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
if (NOT _er_git_head_result EQUAL 0)
    message(FATAL_ERROR "Cannot resolve the Engawa patch head: ${_er_git_head_error}")
endif ()
string(LENGTH "${ER_PATCH_HEAD_SHA}" _er_git_head_length)
if (NOT _er_git_head_length EQUAL 40 OR NOT "${ER_PATCH_HEAD_SHA}" MATCHES "^[0-9a-f]+$")
    message(FATAL_ERROR "Unexpected Engawa patch-head SHA: ${ER_PATCH_HEAD_SHA}")
endif ()

execute_process(
    COMMAND "${ER_GIT_EXECUTABLE}" -C "${CMAKE_SOURCE_DIR}"
        merge-base --is-ancestor "${ER_PATCH_BASE_SHA}" "${ER_PATCH_HEAD_SHA}"
    RESULT_VARIABLE _er_git_ancestor_result
    ERROR_VARIABLE _er_git_ancestor_error
)
if (NOT _er_git_ancestor_result EQUAL 0)
    message(FATAL_ERROR
        "Accepted WebKit base ${ER_PATCH_BASE_SHA} is not an ancestor of ${ER_PATCH_HEAD_SHA}: "
        "${_er_git_ancestor_error}")
endif ()

execute_process(
    COMMAND "${ER_GIT_EXECUTABLE}" -C "${CMAKE_SOURCE_DIR}"
        status --porcelain=v1 --untracked-files=normal
    RESULT_VARIABLE _er_git_status_result
    OUTPUT_VARIABLE _er_git_status
    ERROR_VARIABLE _er_git_status_error
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
if (NOT _er_git_status_result EQUAL 0)
    message(FATAL_ERROR "Cannot inspect Engawa source status: ${_er_git_status_error}")
endif ()
if ("${_er_git_status}" STREQUAL "")
    set(ER_SOURCE_DIRTY 0)
else ()
    set(ER_SOURCE_DIRTY 1)
endif ()
endif ()

unset(_er_apple_architecture)
unset(_er_apple_system_processor)
unset(_er_cocoa_derived_feature)
unset(_er_linux_wpe_legacy_requested)
unset(_er_linux_wpe_locked_off_options)
unset(_er_linux_wpe_option)
unset(_er_linux_wpe_requested_value)
unset(_er_profile_disabled_targets)
unset(_er_production_language)
unset(_er_production_map_canonical)
unset(_er_production_map_canonical_compare)
unset(_er_production_map_contains_binary)
unset(_er_production_map_contains_source)
unset(_er_production_map_is_absolute)
unset(_er_production_map_requested)
unset(_er_production_map_requested_compare)
unset(_er_production_map_root_path)
unset(_er_production_prefix_map)
unset(_er_dependency_lock_json)
unset(_er_dependency_lock_path)
unset(_er_feature_index)
unset(_er_feature_name)
unset(_er_feature_policy_json)
unset(_er_feature_policy_path)
unset(_er_git_ancestor_error)
unset(_er_git_ancestor_result)
unset(_er_git_head_error)
unset(_er_git_head_length)
unset(_er_git_head_result)
unset(_er_git_status)
unset(_er_git_status_error)
unset(_er_git_status_result)
unset(_er_index)
unset(_er_linux_architecture)
unset(_er_lock_input)
unset(_er_lock_input_absolute)
unset(_er_lock_input_actual_sha256)
unset(_er_lock_input_path)
unset(_er_lock_input_sha256)
unset(_er_lock_product)
unset(_er_lock_schema)
unset(_er_lock_webkit_sha)
unset(_er_locked_feature_count)
unset(_er_locked_feature_last)
unset(_er_locked_feature_list_count)
unset(_er_option)
unset(_er_policy_product)
unset(_er_policy_schema)
unset(_er_policy_sdk_major)
unset(_er_policy_webkit_sha)
unset(_er_requested_value)
unset(_er_sorted_locked_features)
unset(_er_unique_locked_feature_count)
