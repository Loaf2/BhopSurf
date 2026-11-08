cmake_minimum_required(VERSION 3.24)
project(AimTrainer LANGUAGES C CXX)

include(FetchContent)
set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(ANDROID)
    set(FT_DISABLE_ZLIB ON CACHE BOOL "" FORCE)
    set(FT_DISABLE_BZIP2 ON CACHE BOOL "" FORCE)
    set(FT_DISABLE_PNG ON CACHE BOOL "" FORCE)
    set(FT_DISABLE_HARFBUZZ ON CACHE BOOL "" FORCE)
    set(FT_DISABLE_BROTLI ON CACHE BOOL "" FORCE)
    FetchContent_Declare(freetype GIT_REPOSITORY https://gitlab.freedesktop.org/freetype/freetype.git GIT_TAG VER-2-13-3 GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(freetype)
    add_library(Freetype::Freetype ALIAS freetype)
else()
    find_package(glfw3 CONFIG REQUIRED)
    find_package(Freetype REQUIRED)
    find_package(CURL CONFIG REQUIRED)
    if(NOT TARGET CURL::libcurl)
        add_library(CURL::libcurl ALIAS CURL::libcurl_shared)
    endif()
    find_package(OpenGL REQUIRED)
endif()

FetchContent_Declare(miniaudio GIT_REPOSITORY https://github.com/mackron/miniaudio.git GIT_TAG 0.11.21 GIT_SHALLOW TRUE)
FetchContent_GetProperties(miniaudio)
if(NOT miniaudio_POPULATED)
    FetchContent_Populate(miniaudio)
endif()

find_package(Threads REQUIRED)

set(AIMTRAINER_SOURCES main.cpp Camera.cpp EBO.cpp glad.c Mesh.cpp Model.cpp shaderClass.cpp stb.cpp Texture.cpp VAO.cpp VBO.cpp)
if(ANDROID)
    list(REMOVE_ITEM AIMTRAINER_SOURCES glad.c)
    list(APPEND AIMTRAINER_SOURCES glad_gles3.c glad_egl.c PlatformWindowAndroid.cpp "${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c")
else()
    list(APPEND AIMTRAINER_SOURCES PlatformWindowDesktop.cpp)
    if(WIN32)
        list(APPEND AIMTRAINER_SOURCES Assets.rc)
    endif()
endif()
if(ANDROID)
    add_library(AimTrainer SHARED ${AIMTRAINER_SOURCES})
else()
    add_executable(AimTrainer ${AIMTRAINER_SOURCES})
endif()
target_include_directories(AimTrainer PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include" "${miniaudio_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}")
if(ANDROID)
    target_include_directories(AimTrainer PRIVATE "${ANDROID_NDK}/sources/android/native_app_glue" "${CMAKE_CURRENT_SOURCE_DIR}/include/glad_gles3")
endif()
target_compile_definitions(AimTrainer PRIVATE GLFW_INCLUDE_NONE GLM_ENABLE_EXPERIMENTAL)
if(ANDROID)
    target_compile_definitions(AimTrainer PRIVATE AIMTRAINER_ANDROID)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        target_compile_definitions(AimTrainer PRIVATE AIMTRAINER_OFFLINE_BYPASS)
    endif()
    target_link_libraries(AimTrainer PRIVATE Freetype::Freetype android EGL GLESv3 log Threads::Threads)
else()
    target_link_libraries(AimTrainer PRIVATE glfw Freetype::Freetype CURL::libcurl OpenGL::GL Threads::Threads)
endif()

if(ANDROID)
    get_filename_component(ANDROID_SDK_ROOT "${ANDROID_NDK}/../.." ABSOLUTE)
    set(ANDROID_BUILD_TOOLS "${ANDROID_SDK_ROOT}/build-tools/34.0.0")
    set(ANDROID_PLATFORM_JAR "${ANDROID_SDK_ROOT}/platforms/android-34/android.jar")
    set(APK_STAGE "${CMAKE_CURRENT_BINARY_DIR}/apk")
    set(APK_UNSIGNED "${CMAKE_CURRENT_BINARY_DIR}/AimTrainer-unsigned.apk")
    set(APK_ALIGNED "${CMAKE_CURRENT_BINARY_DIR}/AimTrainer-aligned.apk")
    set(APK_FINAL "${CMAKE_CURRENT_BINARY_DIR}/AimTrainer.apk")
    set(APK_KEYSTORE "${CMAKE_CURRENT_BINARY_DIR}/debug.keystore")
    set(APK_ABI_DIR "${ANDROID_ABI}")
    add_custom_command(TARGET AimTrainer POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${APK_STAGE}/lib/${APK_ABI_DIR}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:AimTrainer>" "${APK_STAGE}/lib/${APK_ABI_DIR}/libAimTrainer.so"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/Resources" "${APK_STAGE}/assets/Resources"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/default.vert" "${APK_STAGE}/assets/default.vert"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/default.frag" "${APK_STAGE}/assets/default.frag"
        COMMAND "${ANDROID_BUILD_TOOLS}/aapt" package -f -M "${CMAKE_CURRENT_SOURCE_DIR}/android/AndroidManifest.xml" -I "${ANDROID_PLATFORM_JAR}" -A "${APK_STAGE}/assets" --min-sdk-version 24 --target-sdk-version 34 -F "${APK_UNSIGNED}"
        COMMAND ${CMAKE_COMMAND} -E chdir "${APK_STAGE}" "${ANDROID_BUILD_TOOLS}/aapt" add "${APK_UNSIGNED}" lib/${APK_ABI_DIR}/libAimTrainer.so
        COMMAND "${ANDROID_BUILD_TOOLS}/zipalign" -f -p 4 "${APK_UNSIGNED}" "${APK_ALIGNED}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${APK_ALIGNED}" "${APK_FINAL}"
        COMMAND "${ANDROID_BUILD_TOOLS}/apksigner" sign --ks "${APK_KEYSTORE}" --ks-pass pass:android --key-pass pass:android --ks-key-alias androiddebugkey "${APK_FINAL}"
        COMMENT "Packaging AimTrainer.apk")
elseif(APPLE)
    target_link_libraries(AimTrainer PRIVATE "-framework Cocoa" "-framework IOKit" "-framework CoreVideo"
        "-framework AudioToolbox" "-framework AudioUnit" "-framework CoreAudio" "-framework CoreFoundation")
elseif(NOT WIN32)
    find_package(ALSA REQUIRED)
    target_link_libraries(AimTrainer PRIVATE ALSA::ALSA)
else()
    target_link_libraries(AimTrainer PRIVATE winhttp winmm)
endif()
target_link_libraries(AimTrainer PRIVATE ${CMAKE_DL_LIBS})

add_custom_command(TARGET AimTrainer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/default.vert" "$<TARGET_FILE_DIR:AimTrainer>/default.vert"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/default.frag" "$<TARGET_FILE_DIR:AimTrainer>/default.frag"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/Resources" "$<TARGET_FILE_DIR:AimTrainer>/Resources")

install(TARGETS AimTrainer RUNTIME DESTINATION bin)
install(FILES default.vert default.frag DESTINATION share/AimTrainer)
install(DIRECTORY Resources DESTINATION share/AimTrainer)

if(MSVC)
    target_compile_options(AimTrainer PRIVATE /W4 /permissive-)
else()
    target_compile_options(AimTrainer PRIVATE -Wall -Wextra -Wpedantic)
endif()
