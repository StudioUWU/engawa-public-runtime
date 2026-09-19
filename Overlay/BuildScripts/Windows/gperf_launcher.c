#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <errno.h>
#include <process.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include <wctype.h>

static int is_absolute_windows_path(const wchar_t* path)
{
    if (!path || !path[0])
        return 0;
    if (path[0] == L'\\' && path[1] == L'\\')
        return 1;
    return iswalpha(path[0]) && path[1] == L':'
        && (path[2] == L'\\' || path[2] == L'/');
}

int wmain(int argc, wchar_t** argv)
{
    size_t required = 0;
    if (_wgetenv_s(&required, NULL, 0, L"ER_GPERF_EXECUTABLE") || required < 2) {
        fwprintf(stderr,
            L"gperf-launcher: ER_GPERF_EXECUTABLE must name the explicit gperf executable\n");
        return 127;
    }

    wchar_t* gperf = calloc(required, sizeof(*gperf));
    if (!gperf) {
        fwprintf(stderr, L"gperf-launcher: allocation failed\n");
        return 127;
    }
    if (_wgetenv_s(&required, gperf, required, L"ER_GPERF_EXECUTABLE")
        || !is_absolute_windows_path(gperf)) {
        fwprintf(stderr,
            L"gperf-launcher: ER_GPERF_EXECUTABLE must be an absolute Windows path\n");
        free(gperf);
        return 127;
    }

    DWORD attributes = GetFileAttributesW(gperf);
    if (attributes == INVALID_FILE_ATTRIBUTES
        || (attributes & FILE_ATTRIBUTE_DIRECTORY)) {
        fwprintf(stderr,
            L"gperf-launcher: explicit gperf executable is missing (Windows error %lu)\n",
            GetLastError());
        free(gperf);
        return 127;
    }

    wchar_t** childArguments = calloc((size_t)argc + 1, sizeof(*childArguments));
    if (!childArguments) {
        fwprintf(stderr, L"gperf-launcher: allocation failed\n");
        free(gperf);
        return 127;
    }
    childArguments[0] = gperf;
    for (int index = 1; index < argc; ++index)
        childArguments[index] = argv[index];

    intptr_t result = _wspawnv(
        _P_WAIT, gperf, (const wchar_t* const*)childArguments);
    int spawnError = errno;
    free(childArguments);
    if (result == -1) {
        fwprintf(stderr,
            L"gperf-launcher: spawn failed (errno %d, Windows error %lu)\n",
            spawnError, GetLastError());
        free(gperf);
        return 127;
    }
    free(gperf);
    return (int)result;
}
