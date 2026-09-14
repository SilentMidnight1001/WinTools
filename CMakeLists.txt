cmake_minimum_required(VERSION 3.27)
project(WinTools)

#---------------------------------------
# 基础配置
#---------------------------------------
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 禁止 windows.h 定义 min/max 宏。
# 该宏会把 std::max(a,b) 展开成非法的 std::(...)，在 MSVC 下报 C2589
# （"::"右边的非法标记）。MinGW 的 minwindef.h 用 #ifndef __cplusplus 挡住了，
# 故仅 MSVC 受影响，容易在单一工具链上验证时漏掉。
add_compile_definitions(NOMINMAX)

set(Zlib_DIR "D:/Zlib/zlib1.31")

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(OpenCV_DIR "D:/OpenCV/opencv/newbuild")                # OpenCV构建目录
    set(WTL_DIR "D:/WinTools/mingwVersion")
    set(VCPKG_INSTALLED "D:/Vcpkg/vcpkg/installed/x64-mingw-static")
    # 添加头文件的搜索路径 #
    include_directories(
            ${Zlib_DIR}/buildMinGW
    )
    message("use mingw")
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    # 启用自动符号导出（VS 2019及以上版本）
    set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
    set(OpenCV_DIR "D:/OpenCVVs411/opencv/build")              # OpenCV构建目录
    set(WTL_DIR "D:/WinTools/vsVersion")
    set(VCPKG_INSTALLED "D:/Vcpkg/vcpkg/installed/x64-windows")
    # 添加头文件的搜索路径 #
    include_directories(
            ${Zlib_DIR}/buildVS
    )
    # 设置vs为utf8 #
    add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")
    message("use msvc")
endif ()

# 配置Tesseract/Leptonica库变量
set(TESSERACT_INCLUDE_DIR "${VCPKG_INSTALLED}/include")

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(TESSERACT_LIB_NAME "libtesseract55.a")
    set(LEPTONICA_LIB_NAME "libleptonica.a")
    add_compile_definitions(EXDLL)
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    set(TESSERACT_LIB_NAME "tesseract55.lib")
    set(LEPTONICA_LIB_NAME "leptonica-1.85.0.lib")
    add_compile_definitions(EXDLL)
endif ()



#---------------------------------------
# 查找依赖包
#---------------------------------------
find_package(OpenCV REQUIRED)
set(Interception_DIR "D:/Interception")

#---------------------------------------
# 包含目录
#---------------------------------------
include_directories(
        ${WTL_DIR}/include
        ${OpenCV_INCLUDE_DIRS}
        "${Interception_DIR}/library"
        ${Zlib_DIR}
        ${Zlib_DIR}/contrib/minizip
        ${TESSERACT_INCLUDE_DIR}
)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# 设置Release模式的编译选项
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    # 编译库文件 #
    add_library(
            ${PROJECT_NAME} SHARED
            ${WTL_DIR}/src/source/winTools.cpp
            ${WTL_DIR}/src/source/wtlPrint.cpp
            ${WTL_DIR}/src/source/wtlDict.cpp
            ${WTL_DIR}/src/source/wtlRegex.cpp
            ${WTL_DIR}/src/source/wtlOpen.cpp
            ${WTL_DIR}/src/source/wtlZip.cpp
            ${WTL_DIR}/src/source/wtlFiledialog.cpp
            ${WTL_DIR}/src/source/wtlImage.cpp
            ${WTL_DIR}/src/source/wtlStringCode.cpp
            ${WTL_DIR}/src/source/wtlVideo.cpp
            ${WTL_DIR}/src/source/wtlAudio.cpp
            ${WTL_DIR}/src/source/wtlOcr.cpp
            ${WTL_DIR}/src/source/wtlKeyboard.cpp
            ${WTL_DIR}/src/source/wtlMouse.cpp
    )

    target_link_options(${PROJECT_NAME} PRIVATE -Wl,--export-all-symbols)

    add_library(
            ${PROJECT_NAME}Static STATIC
            ${WTL_DIR}/src/source/winTools.cpp
            ${WTL_DIR}/src/source/wtlPrint.cpp
            ${WTL_DIR}/src/source/wtlDict.cpp
            ${WTL_DIR}/src/source/wtlRegex.cpp
            ${WTL_DIR}/src/source/wtlOpen.cpp
            ${WTL_DIR}/src/source/wtlZip.cpp
            ${WTL_DIR}/src/source/wtlFiledialog.cpp
            ${WTL_DIR}/src/source/wtlImage.cpp
            ${WTL_DIR}/src/source/wtlStringCode.cpp
            ${WTL_DIR}/src/source/wtlVideo.cpp
            ${WTL_DIR}/src/source/wtlAudio.cpp
            ${WTL_DIR}/src/source/wtlOcr.cpp
            ${WTL_DIR}/src/source/wtlKeyboard.cpp
            ${WTL_DIR}/src/source/wtlMouse.cpp
            ${WTL_DIR}/include/winToolsH.hpp
            ${WTL_DIR}/include/wtlZip.hpp
            ${WTL_DIR}/include/wtlRegex.hpp
    )

    #---------------------------------------
    # 链接库配置
    #---------------------------------------
    target_link_libraries(${PROJECT_NAME} PRIVATE
            # OpenCV库
            ${OpenCV_LIBS}
            # 第三方键盘库
            "${OpenCV_DIR}/lib/libopencv_world4110.dll.a"
            "${Interception_DIR}/library/x64/interception.lib"
            ${Zlib_DIR}/buildMinGW/libzlib.dll.a
            gdiplus.lib
            Shcore.lib
            Windowscodecs.lib
            # Tesseract + Leptonica
            ${VCPKG_INSTALLED}/lib/${TESSERACT_LIB_NAME}
            ${VCPKG_INSTALLED}/lib/${LEPTONICA_LIB_NAME}
            # Leptonica依赖
            ${VCPKG_INSTALLED}/lib/libpng16.a
            ${VCPKG_INSTALLED}/lib/libjpeg.a
            ${VCPKG_INSTALLED}/lib/libtiff.a
            ${VCPKG_INSTALLED}/lib/libwebp.a
            ${VCPKG_INSTALLED}/lib/libwebpmux.a
            ${VCPKG_INSTALLED}/lib/libsharpyuv.a
            ${VCPKG_INSTALLED}/lib/libgif.a
            ${VCPKG_INSTALLED}/lib/libopenjp2.a
            ${VCPKG_INSTALLED}/lib/liblzma.a
            ${VCPKG_INSTALLED}/lib/libzlib.a
            ws2_32
    )
    #---------------------------------------
    # Windows平台依赖处理
    #---------------------------------------
    if(WIN32)
        # 第三方DLL复制
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy
                "${OpenCV_DIR}/3rdparty/ffmpeg/opencv_videoio_ffmpeg_64.dll"
                # opencv自带的ffmpeg在bin目录 #
                "${OpenCV_DIR}/bin/opencv_videoio_ffmpeg4110_64.dll"
                "${OpenCV_DIR}/bin/libopencv_world4110.dll"
                "${OpenCV_DIR}/lib/libopencv_world4110.dll.a"
                "${Interception_DIR}/library/x64/interception.dll"
                "${Interception_DIR}/library/x64/interception.lib"
                ${Zlib_DIR}/buildMinGW/libzlib.dll
                ${Zlib_DIR}/buildMinGW/libzlib.dll.a
                "$<TARGET_FILE_DIR:WinTools>"
        )


        # 复制整个文件夹 #
        add_custom_target(${PROJECT_NAME}Dir ALL
                COMMAND ${CMAKE_COMMAND} -E copy_directory
                ${WTL_DIR}/include
                $<TARGET_FILE_DIR:WinTools>/source/include
                COMMAND ${CMAKE_COMMAND} -E copy_directory
                ${WTL_DIR}/src
                $<TARGET_FILE_DIR:WinTools>/source/src
        )

        #    # MinGW运行时DLL
        #    add_custom_command(TARGET WinTools POST_BUILD
        #            COMMAND ${CMAKE_COMMAND} -E copy_if_different
        #            "C:/Program Files/mingw64/bin/libgcc_s_seh-1.dll"
        #            "C:/Program Files/mingw64/bin/libstdc++-6.dll"
        #            "C:/Program Files/mingw64/bin/libwinpthread-1.dll"
        #            "$<TARGET_FILE_DIR:WinTools>"
        #    )
    endif()

elseif (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    # 确保这两行存在
    add_definitions(-Dwtl_EXPORTS)
    set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

    # 编译库文件
    add_library(
            WinTools SHARED
            ${WTL_DIR}/src/source/winTools.cpp
            ${WTL_DIR}/src/source/wtlPrint.cpp
            ${WTL_DIR}/src/source/wtlDict.cpp
            ${WTL_DIR}/src/source/wtlRegex.cpp
            ${WTL_DIR}/src/source/wtlOpen.cpp
            ${WTL_DIR}/src/source/wtlZip.cpp
            ${WTL_DIR}/src/source/wtlFiledialog.cpp
            ${WTL_DIR}/src/source/wtlImage.cpp
            ${WTL_DIR}/src/source/wtlStringCode.cpp
            ${WTL_DIR}/src/source/wtlVideo.cpp
            ${WTL_DIR}/src/source/wtlAudio.cpp
            ${WTL_DIR}/src/source/wtlOcr.cpp
            ${WTL_DIR}/src/source/wtlKeyboard.cpp
            ${WTL_DIR}/src/source/wtlMouse.cpp
    )

    add_library(
            WinToolsStatic STATIC
            ${WTL_DIR}/src/source/winTools.cpp
            ${WTL_DIR}/src/source/wtlPrint.cpp
            ${WTL_DIR}/src/source/wtlDict.cpp
            ${WTL_DIR}/src/source/wtlRegex.cpp
            ${WTL_DIR}/src/source/wtlOpen.cpp
            ${WTL_DIR}/src/source/wtlZip.cpp
            ${WTL_DIR}/src/source/wtlFiledialog.cpp
            ${WTL_DIR}/src/source/wtlImage.cpp
            ${WTL_DIR}/src/source/wtlStringCode.cpp
            ${WTL_DIR}/src/source/wtlVideo.cpp
            ${WTL_DIR}/src/source/wtlAudio.cpp
            ${WTL_DIR}/src/source/wtlOcr.cpp
            ${WTL_DIR}/src/source/wtlKeyboard.cpp
            ${WTL_DIR}/src/source/wtlMouse.cpp
            ${WTL_DIR}/include/winToolsH.hpp
            ${WTL_DIR}/include/wtlZip.hpp
            ${WTL_DIR}/include/wtlRegex.hpp
    )

    target_compile_definitions(WinTools PRIVATE wtl_EXPORTS)

    #---------------------------------------
    # 链接库配置
    #---------------------------------------
    target_link_libraries(${PROJECT_NAME} PRIVATE
            ${OpenCV_LIBS}
            ${Interception_DIR}/library/x64/interception.lib
            ${Zlib_DIR}/buildVS/zlib.lib
            gdiplus.lib
            Shcore.lib
            Windowscodecs.lib
            # Tesseract + Leptonica
            ${VCPKG_INSTALLED}/lib/${TESSERACT_LIB_NAME}
            ${VCPKG_INSTALLED}/lib/${LEPTONICA_LIB_NAME}
            # Leptonica依赖
            ${VCPKG_INSTALLED}/lib/libpng16.lib
            ${VCPKG_INSTALLED}/lib/jpeg.lib
            ${VCPKG_INSTALLED}/lib/tiff.lib
            ${VCPKG_INSTALLED}/lib/libwebp.lib
            ${VCPKG_INSTALLED}/lib/libwebpmux.lib
            ${VCPKG_INSTALLED}/lib/libsharpyuv.lib
            ${VCPKG_INSTALLED}/lib/gif.lib
            ${VCPKG_INSTALLED}/lib/openjp2.lib
            ${VCPKG_INSTALLED}/lib/lzma.lib
            ${VCPKG_INSTALLED}/lib/zlib.lib
            # Tesseract传递依赖 (curl + libarchive)
            ${VCPKG_INSTALLED}/lib/archive.lib
            ${VCPKG_INSTALLED}/lib/libcurl.lib
            ${VCPKG_INSTALLED}/lib/libssl.lib
            ${VCPKG_INSTALLED}/lib/libcrypto.lib
            ${VCPKG_INSTALLED}/lib/zstd.lib
            ${VCPKG_INSTALLED}/lib/lz4.lib
            ${VCPKG_INSTALLED}/lib/bz2.lib
            ws2_32.lib
            crypt32.lib
            secur32.lib
            xmllite.lib
            iphlpapi.lib
    )

    #---------------------------------------
    # Windows平台依赖处理
    #---------------------------------------
    if(WIN32)
        # 只复制必要的 OpenCV DLL
        add_custom_command(TARGET WinTools POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${OpenCV_DIR}/bin/opencv_videoio_ffmpeg4110_64.dll"
                "${OpenCV_DIR}/x64/vc16/bin/opencv_world4110.dll"
                "${OpenCV_DIR}/x64/vc16/lib/opencv_world4110.lib"
                "${OpenCV_DIR}/x64/vc16/bin/opencv_videoio_ffmpeg4110_64.dll"
                "${Interception_DIR}/library/x64/interception.dll"
                "${Interception_DIR}/library/x64/interception.lib"
                ${Zlib_DIR}/buildVS/zlib.dll
                ${Zlib_DIR}/buildVS/zlib.lib
                "$<TARGET_FILE_DIR:WinTools>"
        )

        # 复制整个文件夹 #
        add_custom_target(${PROJECT_NAME}Dir ALL
                COMMAND ${CMAKE_COMMAND} -E copy_directory
                ${WTL_DIR}/include
                $<TARGET_FILE_DIR:WinTools>/source/include
                COMMAND ${CMAKE_COMMAND} -E copy_directory
                ${WTL_DIR}/src
                $<TARGET_FILE_DIR:WinTools>/source/src
        )

    endif()
endif ()

# 强制重新生成导出符号
if(MSVC)
    set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
    add_compile_definitions(wtl_EXPORTS EXDLL)

    # 清除旧的导出库
    add_custom_command(TARGET ${PROJECT_NAME} PRE_BUILD
            COMMAND ${CMAKE_COMMAND} -E remove
            "${CMAKE_CURRENT_BINARY_DIR}/WinTools.exports"
    )
endif()

#---------------------------------------
# 独立模块静态库（简写命名）
#---------------------------------------
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(wtl_ZLIB_LIB "${Zlib_DIR}/buildMinGW/libzlib.dll.a")
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    set(wtl_ZLIB_LIB "${Zlib_DIR}/buildVS/zlib.lib")
endif()

add_library(
        wtlDict STATIC
        ${WTL_DIR}/src/source/wtlDict.cpp
        ${WTL_DIR}/include/wtlDict.hpp
)

add_library(
        wtlPrint STATIC
        ${WTL_DIR}/src/source/wtlPrint.cpp
        ${WTL_DIR}/include/wtlPrintFormat.hpp
)

add_library(
        wtlStringCode STATIC
        ${WTL_DIR}/src/source/wtlStringCode.cpp
        ${WTL_DIR}/include/wtlStringCode.hpp
)

add_library(
        wtlZip STATIC
        ${WTL_DIR}/src/source/wtlZip.cpp
        ${WTL_DIR}/include/wtlZip.hpp
)

add_library(
        wtlOpen SHARED
        ${WTL_DIR}/src/source/wtlOpen.cpp
        ${WTL_DIR}/include/wtlOpen.hpp
)

add_library(
        wtlRegex SHARED
        ${WTL_DIR}/src/source/wtlRegex.cpp
        ${WTL_DIR}/include/wtlRegex.hpp
)

add_library(
        wtlRegexStatic STATIC
        ${WTL_DIR}/src/source/wtlRegex.cpp
        ${WTL_DIR}/include/wtlRegex.hpp
)

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_options(wtlOpen PRIVATE -Wl,--export-all-symbols)
    target_link_options(wtlRegex PRIVATE -Wl,--export-all-symbols)
endif()

target_link_libraries(wtlOpen PRIVATE wtlStringCode wtlDict)
target_link_libraries(wtlRegex PRIVATE wtlStringCode)
target_link_libraries(wtlRegexStatic PUBLIC wtlStringCode)
target_link_libraries(wtlPrint PUBLIC wtlDict)
target_link_libraries(wtlZip PUBLIC ${wtl_ZLIB_LIB})
