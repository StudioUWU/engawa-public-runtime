/* Copyright (C) 2026 EngawaRuntime contributors.
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Independent C interface to the replaceable WebCore library. This header
 * contains no WebKit or C++ declarations. All integers have fixed widths;
 * strings are length-delimited UTF-8. Callbacks run synchronously on the
 * creating thread and must not throw or re-enter this API.
 */
#ifndef ENGAWA_WEBCORE_H
#define ENGAWA_WEBCORE_H
#include <stddef.h>
#include <stdint.h>

#if defined(_WIN32)
#define ERWC_CALL __cdecl
#if defined(ERWC_BUILDING_WEBCORE)
#define ERWC_EXPORT __declspec(dllexport)
#else
#define ERWC_EXPORT __declspec(dllimport)
#endif
#else
#define ERWC_CALL
#define ERWC_EXPORT __attribute__((visibility("default")))
#endif
#ifdef __cplusplus
extern "C" {
#endif

#define ERWC_ABI_VERSION 1u
typedef struct ERWC_ViewImpl* ERWC_View;
typedef struct ERWC_Bytes { const void* data; size_t size; } ERWC_Bytes;
/* release(owner) executes in the allocator's library. It is called exactly
 * once, including failed operations. Null owner requires null release. */
typedef struct ERWC_OwnedBytes {
    ERWC_Bytes bytes;
    void* owner;
    void (ERWC_CALL *release)(void* owner);
} ERWC_OwnedBytes;
typedef struct ERWC_Resource {
    ERWC_Bytes mount, mime_type, text_encoding, bytes;
    uint64_t content_length;
    void* owner;
    void (ERWC_CALL *release)(void* owner);
} ERWC_Resource;
/* read_bundle: purpose 0=main document, 1=subresource; method 0=GET, 1=HEAD.
 * Return 0=success, 1=invalid request, 2=not found, 3=unavailable. */
typedef struct ERWC_Callbacks {
    void* context;
    uint32_t (ERWC_CALL *read_bundle)(void*, ERWC_Bytes url,
        ERWC_Bytes required_mount, uint32_t purpose, uint32_t method,
        ERWC_Resource* output);
    uint32_t (ERWC_CALL *post_message)(void*, ERWC_Bytes json);
    uint32_t (ERWC_CALL *read_clipboard)(void*, ERWC_OwnedBytes* output);
    uint32_t (ERWC_CALL *write_clipboard)(void*, ERWC_Bytes text);
} ERWC_Callbacks;
typedef struct ERWC_ViewConfig {
    uint32_t struct_size;
    uint32_t logical_width, logical_height;
    float device_scale;
    uint32_t max_frames_per_second;
    uint32_t transparent;
    ERWC_Callbacks callbacks;
} ERWC_ViewConfig;

enum ERWC_Operation {
    ERWC_LOAD_HTML = 1, ERWC_LOAD_BUNDLE, ERWC_EVALUATE,
    ERWC_FOCUS, ERWC_MOUSE, ERWC_WHEEL, ERWC_KEY, ERWC_INSERT_TEXT,
    ERWC_POST_MESSAGE, ERWC_STOP, ERWC_RESIZE, ERWC_FRAME_RATE,
    ERWC_COMMIT_FRAME, ERWC_ROLLBACK_FRAME, ERWC_PUMP
};
/* Stable command transport. Unused fields must be zero. Pointer inputs are
 * borrowed only for the call. Mouse/key/wheel enum order and modifier bits
 * match the documented EngawaRuntime input ABI, with zero-based event types
 * (mouse move/down/up/enter/leave, key down/up). */
typedef struct ERWC_Command {
    uint32_t struct_size, operation;
    ERWC_Bytes first, second;
    uint64_t transaction;
    double x, y;
    float delta_x, delta_y, device_scale;
    uint32_t width, height, value, type, button, buttons, modifiers;
    uint32_t click_count, delta_mode, phase, location, flags;
} ERWC_Command;
/* Result bytes remain valid until release(owner), even across later calls.
 * changed is replacementStarted for loads and didWork for pump. */
typedef struct ERWC_Response {
    uint32_t success, changed;
    ERWC_Bytes value, error;
    void* owner;
    void (ERWC_CALL *release)(void* owner);
} ERWC_Response;
typedef struct ERWC_Frame {
    uint32_t width, height, row_bytes;
    const uint8_t* pixels;
    size_t pixel_byte_count;
    int32_t damage_x, damage_y;
    uint32_t damage_width, damage_height, opacity_known, fully_opaque;
    uint64_t transaction;
    void* owner;
    void (ERWC_CALL *release)(void* owner);
} ERWC_Frame;
typedef struct ERWC_ViewState {
    uint32_t invalidated, cursor, editable_focus;
} ERWC_ViewState;

/* The negotiated immutable table lives as long as the loaded WebCore module.
 * Destroy all views and release every frame/result before unloading it.
 * WebCore owns all WebKit types, allocations, and destruction. The host owns
 * its policy, callbacks, queues, and copies. No compiler C++ ABI crosses this
 * boundary. Compatible user-rebuilt libraries are admitted by ABI, not hash. */
typedef struct ERWC_API {
    uint32_t struct_size, abi_version;
    ERWC_View (ERWC_CALL *create)(const ERWC_ViewConfig*, ERWC_Response*);
    void (ERWC_CALL *destroy)(ERWC_View);
    void (ERWC_CALL *execute)(ERWC_View, const ERWC_Command*, ERWC_Response*);
    void (ERWC_CALL *snapshot)(ERWC_View, ERWC_Frame*, ERWC_Response*);
    ERWC_ViewState (ERWC_CALL *view_state)(ERWC_View);
    void (ERWC_CALL *clear_invalidation)(ERWC_View);
    uint64_t (ERWC_CALL *invalidation_generation)(void);
    void (ERWC_CALL *destroy_thread_state)(void);
} ERWC_API;
ERWC_EXPORT uint32_t ERWC_CALL er_webcore_get_api(uint32_t abi_version,
    uint32_t minimum_table_size, const ERWC_API** output);
#ifdef __cplusplus
}
#endif
#endif
