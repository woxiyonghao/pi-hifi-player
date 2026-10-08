cmake_minimum_required(VERSION 3.20)
project(audio_engine LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 源码列表
set(AUDIO_ENGINE_SOURCES
    src/AudioEngine.cpp
    src/DsfParser.cpp
    src/decoders/FlacDecoder.cpp
    src/decoders/WavDecoder.cpp
    src/decoders/Mp3Decoder.cpp
    src/sinks/SdlAudioSink.cpp
    src/sinks/AlsaAudioSink.cpp
)

if(APPLE)
    list(APPEND AUDIO_ENGINE_SOURCES src/decoders/AppleAudioDecoder.cpp)
endif()

# 生成发烧音频引擎静态库
add_library(audio_engine STATIC ${AUDIO_ENGINE_SOURCES})

target_include_directories(audio_engine
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party
        ${SDL2_INCLUDE_DIRS}
)

if(APPLE)
    target_link_libraries(audio_engine PRIVATE
        ${SDL2_LIBRARIES}
        "-framework CoreAudio"
        "-framework AudioToolbox"
        "-framework CoreFoundation"
    )
else()
    target_link_libraries(audio_engine PRIVATE
        pthread
        ${SDL2_LIBRARIES}
    )
    # 树莓派 5 生产部署时若安装了 libasound2-dev 则自动链接 ALSA
    find_package(PkgConfig QUIET)
    if(PKG_CONFIG_FOUND)
        pkg_check_modules(ALSA QUIET alsa)
        if(ALSA_FOUND)
            target_link_libraries(audio_engine PRIVATE ${ALSA_LIBRARIES})
            target_include_directories(audio_engine PRIVATE ${ALSA_INCLUDE_DIRS})
        endif()
    endif()
endif()
