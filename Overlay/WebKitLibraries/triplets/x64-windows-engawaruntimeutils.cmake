# EngawaRuntimeUtils owns the static support-library aggregation boundary.  Keep the
# same Windows x64 toolchain and dynamic MSVC runtime as the accepted WebKit
# triplet, but build every vcpkg library as an archive for the private DLL.
include("${CMAKE_CURRENT_LIST_DIR}/x64-windows-webkit.cmake")

set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)

# curl's port derives CURL_STATIC_CRT from BUILD_STATIC_LIBS, which would
# otherwise select /MT for its configure probes even though this triplet owns
# an /MD DLL boundary.  Triplet configure options are appended after the
# port's options, so keep curl aligned with VCPKG_CRT_LINKAGE here.
if (PORT STREQUAL "curl")
    list(APPEND VCPKG_CMAKE_CONFIGURE_OPTIONS -DCURL_STATIC_CRT=OFF)

    # curl includes ngtcp2's public headers directly.  On Windows those
    # headers default to dllimport unless the static-consumer definition is
    # present, even when CMake found ngtcp2::ngtcp2_static.  Without this the
    # libcurl archive records __imp_ngtcp2_* references that cannot be
    # satisfied by ngtcp2.lib.  Keep HTTP/3 enabled and compile curl as the
    # static ngtcp2 consumer that this triplet requires.
    string(APPEND VCPKG_C_FLAGS " /DNGTCP2_STATICLIB")
    string(APPEND VCPKG_CXX_FLAGS " /DNGTCP2_STATICLIB")
endif ()

# libxslt 1.1.45 publishes LIBXSLT_STATIC only as an INTERFACE definition on
# its static target. Its own C objects therefore see the Windows dllexport
# declarations and embed /EXPORT directives in libxslt.lib. Compile the port's
# implementation as static too so EngawaRuntimeUtils's checked-in .def remains the
# sole export contract.
if (PORT STREQUAL "libxslt")
    string(APPEND VCPKG_C_FLAGS " /DLIBXSLT_STATIC")
    string(APPEND VCPKG_CXX_FLAGS " /DLIBXSLT_STATIC")
endif ()
