include("${CMAKE_CURRENT_LIST_DIR}/x64-windows-engawaruntimeutils.cmake")

set(_er_production_map_root "$ENV{ER_PRODUCTION_PATH_MAP_ROOT}")
if (NOT _er_production_map_root
    OR "${_er_production_map_root}" MATCHES "[;=\"<>$\r\n]")
    message(FATAL_ERROR
        "The production Utils triplet requires ER_PRODUCTION_PATH_MAP_ROOT.")
endif ()
cmake_path(IS_ABSOLUTE _er_production_map_root
    _er_production_map_is_absolute)
if (NOT _er_production_map_is_absolute
    OR NOT IS_DIRECTORY "${_er_production_map_root}")
    message(FATAL_ERROR
        "ER_PRODUCTION_PATH_MAP_ROOT must be an existing absolute directory.")
endif ()
cmake_path(NORMAL_PATH _er_production_map_root
    OUTPUT_VARIABLE _er_production_map_requested)
file(REAL_PATH "${_er_production_map_root}"
    _er_production_map_canonical EXPAND_TILDE)
cmake_path(NORMAL_PATH _er_production_map_canonical)
string(TOLOWER "${_er_production_map_requested}"
    _er_production_map_requested_lower)
string(TOLOWER "${_er_production_map_canonical}"
    _er_production_map_canonical_lower)
if (NOT _er_production_map_requested_lower STREQUAL
    _er_production_map_canonical_lower)
    message(FATAL_ERROR
        "ER_PRODUCTION_PATH_MAP_ROOT must already be canonical.")
endif ()
cmake_path(GET _er_production_map_canonical ROOT_PATH
    _er_production_map_root_path)
if (_er_production_map_canonical STREQUAL _er_production_map_root_path)
    message(FATAL_ERROR
        "ER_PRODUCTION_PATH_MAP_ROOT cannot be a filesystem root.")
endif ()

set(VCPKG_ENV_PASSTHROUGH_UNTRACKED ER_PRODUCTION_PATH_MAP_ROOT)
set(_er_production_compile_flags
    " /experimental:deterministic \"/pathmap:${_er_production_map_canonical}=engawaruntime-production\"")
string(APPEND VCPKG_C_FLAGS "${_er_production_compile_flags}")
string(APPEND VCPKG_CXX_FLAGS "${_er_production_compile_flags}")
string(APPEND VCPKG_LINKER_FLAGS " /DEBUG:NONE")

if (PORT STREQUAL "libxml2")
    list(APPEND VCPKG_CMAKE_CONFIGURE_OPTIONS
        "-DCMAKE_INSTALL_SYSCONFDIR:PATH=C:/ProgramData/EngawaRuntime/etc")
endif ()

unset(_er_production_compile_flags)
unset(_er_production_map_canonical)
unset(_er_production_map_canonical_lower)
unset(_er_production_map_is_absolute)
unset(_er_production_map_requested)
unset(_er_production_map_requested_lower)
unset(_er_production_map_root)
unset(_er_production_map_root_path)
