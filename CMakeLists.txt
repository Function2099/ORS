cmake_minimum_required(VERSION 3.20)

project(ORS
    VERSION 0.1.0
    DESCRIPTION "Open Recording Software"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(MSVC)
    add_compile_options(/utf-8 /W4)
else()
    add_compile_options(-Wall -Wextra)
endif()

find_package(Qt6 REQUIRED COMPONENTS Core Widgets LinguistTools)

set(ORS_SOURCES
    src/main.cpp
    src/ui/MainWindow.cpp
    src/ui/MainWindow.h
    src/ui/RegionOverlay.cpp
    src/ui/RegionOverlay.h
    src/ui/ToolbarIcons.cpp
    src/ui/ToolbarIcons.h
    src/ui/SettingsDialog.cpp
    src/ui/SettingsDialog.h
    src/ui/HotkeyRegistrar.cpp
    src/ui/HotkeyRegistrar.h
    src/core/Config.cpp
    src/core/Config.h
    src/core/FilenameTemplate.cpp
    src/core/FilenameTemplate.h
    src/core/FrameQueue.h
    src/core/WatermarkOverlay.cpp
    src/core/WatermarkOverlay.h
    src/core/RecordingSession.cpp
    src/core/RecordingSession.h
    src/core/VideoFrame.h
    src/core/AudioFrame.h
    src/core/EncodedPacket.h
    src/capture/ICapture.h
    src/audio/IAudioCapture.h
    src/audio/AudioMix.h
    src/audio/AudioDevices.h
    src/mux/IMuxer.h
    src/mux/GifEncoder.cpp
    src/mux/GifEncoder.h
    src/encode/IEncoder.h
    src/encode/ColorConvert.h
    src/encode/MFEncoder.cpp
    src/encode/MFEncoder.h
    resources/ors.qrc
)

qt_add_executable(ORS WIN32 ${ORS_SOURCES})

target_include_directories(ORS PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(ORS PRIVATE
    Qt6::Widgets
)

if(WIN32)
    target_sources(ORS PRIVATE
        src/capture/CaptureWin.cpp
        src/capture/CaptureWin.h
        src/audio/AudioCaptureWin.cpp
        src/audio/AudioCaptureWin.h
        src/mux/Mp4Muxer.cpp
        src/mux/Mp4Muxer.h
    )
    target_link_libraries(ORS PRIVATE
        dxgi
        d3d11
        mf
        mfplat
        mfreadwrite
        mfuuid
        ole32
        uuid
        user32
        gdi32
        strmiids
        advapi32
        powrprof
    )
endif()

set_source_files_properties(
    ${CMAKE_SOURCE_DIR}/resources/i18n/ors_zh_TW.ts
    ${CMAKE_SOURCE_DIR}/resources/i18n/ors_en.ts
    PROPERTIES OUTPUT_LOCATION ${CMAKE_CURRENT_BINARY_DIR}/i18n
)
qt_add_lrelease(ORS
    TS_FILES
        ${CMAKE_SOURCE_DIR}/resources/i18n/ors_zh_TW.ts
        ${CMAKE_SOURCE_DIR}/resources/i18n/ors_en.ts
    QM_FILES_OUTPUT_VARIABLE ORS_QM_FILES
)
qt_add_resources(ORS ors_i18n
    PREFIX "/i18n"
    BASE ${CMAKE_CURRENT_BINARY_DIR}/i18n
    FILES ${ORS_QM_FILES}
)

if(WIN32)
    get_target_property(_ors_qmake Qt6::qmake IMPORTED_LOCATION)
    if(_ors_qmake)
        get_filename_component(_ors_qt_bin "${_ors_qmake}" DIRECTORY)
        find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${_ors_qt_bin}")
        if(WINDEPLOYQT_EXECUTABLE)
            add_custom_command(TARGET ORS POST_BUILD
                COMMAND "${WINDEPLOYQT_EXECUTABLE}"
                    --no-translations
                    --no-system-d3d-compiler
                    "$<TARGET_FILE:ORS>"
                COMMENT "Deploying Qt runtime next to ORS.exe"
            )
        endif()
    endif()
endif()

enable_testing()

include(FetchContent)
FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.8.1
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(Catch2)

add_executable(ors_tests
    tests/main.cpp
    tests/test_config.cpp
    tests/test_frame_queue.cpp
    tests/test_color_convert.cpp
    tests/test_filename_template.cpp
    tests/test_audio_mix.cpp
    tests/test_gif_encoder.cpp
    tests/test_hotkey.cpp
    tests/test_watermark_overlay.cpp
    tests/test_clock.cpp
    src/core/Config.cpp
    src/core/FilenameTemplate.cpp
    src/core/WatermarkOverlay.cpp
    src/mux/GifEncoder.cpp
    src/ui/HotkeyRegistrar.cpp
)

target_include_directories(ors_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(ors_tests PRIVATE
    Catch2::Catch2
    Qt6::Core
    Qt6::Gui
)

if(WIN32)
    target_link_libraries(ors_tests PRIVATE user32)
endif()

add_test(NAME ors_tests COMMAND ors_tests)
