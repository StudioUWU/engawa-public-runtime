// EngawaRuntimeUtils has no public source API.  Its checked-in module-definition
// file is the only export surface; this private anchor gives CMake a concrete
// translation unit and enforces the DLL's /MD toolchain contract.
extern "C" int er_engawaruntimeutils_private_link_anchor() noexcept
{
    return 0;
}
