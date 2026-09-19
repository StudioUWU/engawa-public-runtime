if (APPLE)
    include("${CMAKE_CURRENT_LIST_DIR}/PlatformMac.cmake")

    if (ER_ENABLE_INPROCESS_SHARED_LIBRARIES)
        # PlatformCocoa signs the WebCore framework after linking. The Engawa
        # monolithic graph intentionally builds WebCore as a static archive,
        # so there is no framework binary to sign.
        set(WebCore_POST_BUILD_COMMAND)

        # PlatformCocoa tolerates unresolved symbols when WebGPU is disabled
        # because its normal framework graph supplies them at load time. The
        # Engawa runtime is the final monolithic image, so unresolved WebCore
        # symbols must fail its link instead of surviving until dyld.
        list(REMOVE_ITEM WebCore_PRIVATE_LIBRARIES
            "-Wl,-undefined,dynamic_lookup"
        )
    endif ()

    if (NOT ENABLE_VIDEO_PRESENTATION_MODE)
        # PlatformCocoa schedules this implementation independently of the
        # presentation-mode types it consumes.
        list(REMOVE_ITEM WebCore_SOURCES
            platform/cocoa/WebAVPlayerLayer.mm
        )
    endif ()

    if (NOT ENABLE_VIDEO)
        # This explicit PlatformCocoa source consumes VideoToolbox types whose
        # declarations are unavailable at the Engawa VIDEO=OFF boundary. The
        # remaining entries also occur in SourcesCocoa, so the matching
        # unified exclusions below are required to remove both occurrences.
        list(REMOVE_ITEM WebCore_SOURCES
            platform/graphics/avfoundation/AVTrackPrivateAVFObjCImpl.mm
            platform/graphics/avfoundation/InbandMetadataTextTrackPrivateAVF.cpp
            platform/graphics/avfoundation/InbandTextTrackPrivateAVF.cpp
            platform/graphics/avfoundation/MediaPlayerPrivateAVFoundation.cpp
            platform/graphics/avfoundation/MediaSelectionGroupAVFObjC.mm
            platform/graphics/avfoundation/objc/AVAssetTrackUtilities.mm
            platform/graphics/avfoundation/objc/AudioTrackPrivateAVFObjC.mm
            platform/graphics/avfoundation/objc/InbandTextTrackPrivateAVFObjC.mm
            platform/graphics/avfoundation/objc/MediaPlayerPrivateAVFoundationObjC.mm
            platform/graphics/avfoundation/objc/VideoTrackPrivateAVFObjC.cpp
            platform/graphics/avfoundation/objc/WebCoreAVFResourceLoader.mm
            platform/graphics/cv/GraphicsContextGLCVCocoa.mm
            platform/mac/SerializedPlatformDataCueMac.mm
            platform/network/cocoa/WebCoreNSURLSession.mm
        )

        # These implementation files and their declarations have matching
        # VIDEO / VIDEO_PRESENTATION_MODE guards. Do not schedule empty or
        # structurally unavailable translation units for the locked-off graph.
        list(APPEND WebCore_UNIFIED_SOURCE_EXCLUDES
            "^page/CaptionUserPreferencesMediaAF\\.cpp"
            "^page/cocoa/CaptionUserPreferencesMediaAFCocoa\\.mm"
            "^platform/VideoFrame\\.mm"
            "^platform/cocoa/PlaybackSessionModelMediaElement\\.mm"
            "^platform/cocoa/SerializedPlatformDataCueValue\\.mm"
            "^platform/cocoa/VideoPresentationLayerProvider\\.mm"
            "^platform/cocoa/VideoPresentationModelVideoElement\\.mm"
            "^platform/graphics/avfoundation/AVTrackPrivateAVFObjCImpl\\.mm"
            "^platform/graphics/avfoundation/InbandMetadataTextTrackPrivateAVF\\.cpp"
            "^platform/graphics/avfoundation/InbandTextTrackPrivateAVF\\.cpp"
            "^platform/graphics/avfoundation/MediaPlayerPrivateAVFoundation\\.cpp"
            "^platform/graphics/avfoundation/MediaSelectionGroupAVFObjC\\.mm"
            "^platform/graphics/avfoundation/objc/AVAssetTrackUtilities\\.mm"
            "^platform/graphics/avfoundation/objc/AudioTrackPrivateAVFObjC\\.mm"
            "^platform/graphics/avfoundation/objc/InbandChapterTrackPrivateAVFObjC\\.mm"
            "^platform/graphics/avfoundation/objc/InbandTextTrackPrivateAVFObjC\\.mm"
            "^platform/graphics/avfoundation/objc/MediaPlayerPrivateAVFoundationObjC\\.mm"
            "^platform/graphics/avfoundation/objc/VideoTrackPrivateAVFObjC\\.cpp"
            "^platform/graphics/avfoundation/objc/WebCoreAVFResourceLoader\\.mm"
            "^platform/graphics/cocoa/MediaPlayerCocoa\\.mm"
            "^platform/graphics/cocoa/TextTrackRepresentationCocoa\\.mm"
            "^platform/graphics/cv/GraphicsContextGLCVCocoa\\.mm"
            "^platform/graphics/cv/VideoFrameCV\\.mm"
            "^platform/mac/PlaybackSessionInterfaceMac\\.mm"
            "^platform/mac/SerializedPlatformDataCueMac\\.mm"
            "^platform/mac/VideoPresentationInterfaceMac\\.mm"
            "^platform/mac/WebPlaybackControlsManager\\.mm"
        )

        # SourcesCocoa also schedules these platform implementations from
        # predicates independent of ENABLE_VIDEO. Each consumes a type family
        # removed by the Engawa profile. RangeResponseGenerator is paired with
        # WebCoreNSURLSession because it exists solely to serve that session.
        list(APPEND WebCore_UNIFIED_SOURCE_EXCLUDES
            "^platform/graphics/avfoundation/AudioVideoRendererAVFObjC\\.mm"
            "^platform/graphics/cocoa/PlatformMediaEngineConfigurationFactoryCocoa\\.cpp"
            "^platform/graphics/cocoa/VideoMediaSampleRenderer\\.mm"
            "^platform/graphics/cv/ImageTransferSessionVT\\.mm"
            "^platform/ios/PlaybackSessionInterfaceAVKitLegacy\\.mm"
            "^platform/ios/PlaybackSessionInterfaceIOS\\.mm"
            "^platform/ios/PlaybackSessionInterfaceTVOS\\.cpp"
            "^platform/ios/VideoPresentationInterfaceAVKitLegacy\\.mm"
            "^platform/ios/VideoPresentationInterfaceIOS\\.mm"
            "^platform/ios/VideoPresentationInterfaceTVOS\\.mm"
            "^platform/ios/WebAVPlayerController\\.mm"
            "^platform/ios/WebVideoFullscreenControllerAVKit\\.mm"
            "^platform/network/cocoa/RangeResponseGenerator\\.mm"
            "^platform/network/cocoa/WebCoreNSURLSession\\.mm"
        )
    endif ()

    if (NOT ENABLE_MEDIA_STREAM)
        # The Cocoa source list is shared by ports where media capture is
        # normally enabled. These implementations inherit exclusively from
        # the locked-off RealtimeMediaSource / capture type family.
        list(APPEND WebCore_UNIFIED_SOURCE_EXCLUDES
            "^platform/mediastream/cocoa/AVCaptureDeviceManager\\.mm"
            "^platform/mediastream/cocoa/AVVideoCaptureSource\\.mm"
            "^platform/mediastream/cocoa/AudioMediaStreamTrackRendererCocoa\\.cpp"
            "^platform/mediastream/cocoa/AudioMediaStreamTrackRendererInternalUnit\\.cpp"
            "^platform/mediastream/cocoa/AudioMediaStreamTrackRendererUnit\\.cpp"
            "^platform/mediastream/cocoa/BaseAudioCaptureUnit\\.cpp"
            "^platform/mediastream/cocoa/CoreAudioCaptureDevice\\.cpp"
            "^platform/mediastream/cocoa/CoreAudioCaptureDeviceManager\\.cpp"
            "^platform/mediastream/cocoa/CoreAudioCaptureSource\\.cpp"
            "^platform/mediastream/cocoa/CoreAudioCaptureUnit\\.cpp"
            "^platform/mediastream/cocoa/DisplayCaptureManagerCocoa\\.cpp"
            "^platform/mediastream/cocoa/DisplayCaptureSourceCocoa\\.cpp"
            "^platform/mediastream/cocoa/IncomingAudioMediaStreamTrackRendererUnit\\.cpp"
            "^platform/mediastream/cocoa/MediaStreamTrackAudioSourceProviderCocoa\\.cpp"
            "^platform/mediastream/cocoa/MockAudioCaptureUnit\\.mm"
            "^platform/mediastream/cocoa/MockRealtimeVideoSourceCocoa\\.mm"
            "^platform/mediastream/cocoa/RealtimeIncomingAudioSourceCocoa\\.cpp"
            "^platform/mediastream/cocoa/RealtimeIncomingVideoSourceCocoa\\.mm"
            "^platform/mediastream/cocoa/RealtimeMediaSourceCenterCocoa\\.cpp"
            "^platform/mediastream/cocoa/RealtimeMediaSourceCenterCocoa\\.mm"
            "^platform/mediastream/cocoa/RealtimeOutgoingAudioSourceCocoa\\.cpp"
            "^platform/mediastream/cocoa/RealtimeOutgoingVideoSourceCocoa\\.cpp"
            "^platform/mediastream/cocoa/ScreenCaptureKitCaptureSource\\.mm"
            "^platform/mediastream/cocoa/ScreenCaptureKitSharingSessionManager\\.mm"
            "^platform/mediastream/cocoa/WebAudioSourceProviderCocoa\\.mm"
            "^platform/mediastream/ios/AVAudioSessionCaptureDevice\\.mm"
            "^platform/mediastream/ios/AVAudioSessionCaptureDeviceManager\\.mm"
            "^platform/mediastream/ios/CoreAudioCaptureSourceIOS\\.mm"
            "^platform/mediastream/libwebrtc/LibWebRTCProviderCocoa\\.cpp"
        )

        # PlatformCocoa also appends these five sources directly rather than
        # solely through SourcesCocoa. Remove those parallel occurrences.
        list(REMOVE_ITEM WebCore_SOURCES
            platform/mediastream/cocoa/CoreAudioCaptureUnit.mm
            platform/mediastream/cocoa/MockRealtimeVideoSourceCocoa.mm
            platform/mediastream/cocoa/RealtimeOutgoingVideoSourceCocoa.cpp
            platform/mediastream/libwebrtc/LibWebRTCAudioModule.cpp
            platform/mediastream/libwebrtc/LibWebRTCDav1dDecoder.cpp
        )
    endif ()

    if (NOT ENABLE_WEB_AUDIO)
        list(APPEND WebCore_UNIFIED_SOURCE_EXCLUDES
            "^platform/audio/cocoa/AudioSampleBufferList\\.cpp"
            "^platform/audio/cocoa/AudioSampleDataConverter\\.mm"
            "^platform/audio/cocoa/AudioSampleDataSource\\.mm"
        )
    endif ()

elseif (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    include("${CMAKE_CURRENT_LIST_DIR}/PlatformWPE.cmake")
else ()
    include("${CMAKE_CURRENT_LIST_DIR}/PlatformWin.cmake")
endif ()

if (ER_ENABLE_INPROCESS_ENGINE)
    list(APPEND WebCore_SOURCES
        platform/engawa/EngawaInProcessWebCorePrototype.cpp
        platform/engawa/EngawaWebCoreBridge.cpp
    )
    list(APPEND WebCore_PRIVATE_INCLUDE_DIRECTORIES "${CMAKE_SOURCE_DIR}/SDK/include")
    list(APPEND WebCore_PRIVATE_DEFINITIONS ERWC_BUILDING_WEBCORE=1)
    list(APPEND WebCore_PRIVATE_FRAMEWORK_HEADERS
        platform/engawa/EngawaInProcessWebCorePrototype.h
    )
endif ()

if (WIN32)
    # The Win port normally receives these through feature combinations that
    # the Engawa boundary deliberately disables (notably media). The remaining
    # Curl networking and DirectWrite-backed Skia font paths still use them.
    list(APPEND WebCore_LIBRARIES
        dwrite
        ws2_32
    )
endif ()
