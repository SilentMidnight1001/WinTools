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

# 导出宏定义
add_compile_definitions(EXDLL)



#---------------------------------------
# 查找依赖包
#---------------------------------------
find_package(OpenCV REQUIRED)
set(Interception_DIR "D:/Interception")
set(RadiOcr_DIR "D:/CPPLibrary/radioOCR")

#---------------------------------------
# 构建输出目录
# MinGW  : <build>/source/src/lib
# MSVC   : <build>/<Config>/source/src/lib
#   dll    : 运行期 DLL
#   link   : 链接 DLL 使用的导入库
#   static : 静态链接库
#---------------------------------------
if(CMAKE_CONFIGURATION_TYPES)
    set(WTL_OUTPUT_ROOT "${CMAKE_BINARY_DIR}/$<CONFIG>")
else()
    set(WTL_OUTPUT_ROOT "${CMAKE_BINARY_DIR}")
endif()
set(WTL_PACKAGE_SOURCE_DIR "${WTL_OUTPUT_ROOT}/source")
set(WTL_BIN_DIR "${WTL_PACKAGE_SOURCE_DIR}/src/lib")
set(WTL_DLL_DIR "${WTL_BIN_DIR}/dll")
set(WTL_LINK_DIR "${WTL_BIN_DIR}/link")
set(WTL_STATIC_DIR "${WTL_BIN_DIR}/static")

#---------------------------------------
# RapidOCR (OcrLite C API, 跨编译器)
#---------------------------------------
# radioOCR 根目录下 mingwVersion / vsVersion 是两份独立构建, 头文件与库文件按当前
# 编译器各取一份 (用法与 CPPTestProject/CPPProgram/ocrProgram/radioocr 一致):
#   GNU  : build/lib/libRapidOcrOnnxClib.dll.a + build/bin/libRapidOcrOnnxClib.dll
#   MSVC : build/lib/RapidOcrOnnx.lib          + build/bin/RapidOcrOnnx.dll
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(RadiOcr_VERSION_DIR "${RadiOcr_DIR}/mingwVersion")
    set(RadiOcr_LIB "${RadiOcr_VERSION_DIR}/build/lib/libRapidOcrOnnxClib.dll.a")
    set(RadiOcr_DLL "${RadiOcr_VERSION_DIR}/build/bin/libRapidOcrOnnxClib.dll")
    # MinGW 版的 RapidOCR 依赖共享 OpenCV, 与 WinTools 用的是同一份 libopencv_world4110.dll
    set(RadiOcr_OPENCV_DLL "${RadiOcr_VERSION_DIR}/deps/opencv/x64/mingw/bin/libopencv_world4110.dll")
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    set(RadiOcr_VERSION_DIR "${RadiOcr_DIR}/vsVersion")
    set(RadiOcr_LIB "${RadiOcr_VERSION_DIR}/build/lib/RapidOcrOnnx.lib")
    set(RadiOcr_DLL "${RadiOcr_VERSION_DIR}/build/bin/RapidOcrOnnx.dll")
    # RapidOcrOnnx.dll 也导入 opencv_world4110.dll, 取本工程已用的那份 OpenCV 4.11
    set(RadiOcr_OPENCV_DLL "${OpenCV_DIR}/x64/vc16/bin/opencv_world4110.dll")
endif ()

set(RadiOcr_INCLUDE_DIR "${RadiOcr_VERSION_DIR}/include")
# 模型随本工程一起分发: 打包在 <工程根>/model (原始来源 ${RadiOcr_VERSION_DIR}/models)
set(RadiOcr_MODELS_DIR "${CMAKE_SOURCE_DIR}/model" CACHE PATH "RapidOCR 模型目录")
# onnxruntime.dll 是官方二进制(C API), 两套编译器共用同一份
set(RadiOcr_ORT_DLL "${RadiOcr_DIR}/mingwVersion/deps/onnxruntime-win-x64/bin/onnxruntime.dll")

if(NOT EXISTS "${RadiOcr_INCLUDE_DIR}/OcrLiteCApi.h")
    message(FATAL_ERROR "未找到 RapidOCR 头文件: ${RadiOcr_INCLUDE_DIR}/OcrLiteCApi.h")
endif()
if(NOT EXISTS "${RadiOcr_LIB}")
    message(FATAL_ERROR "未找到 RapidOCR 库: ${RadiOcr_LIB}")
endif()
if(NOT EXISTS "${RadiOcr_MODELS_DIR}/PP-OCRv6_rec_small.onnx")
    message(FATAL_ERROR "未找到 RapidOCR 模型目录: ${RadiOcr_MODELS_DIR}")
endif()

# ocr::Ocr 未显式指定 models 目录时的默认路径 (三个 onnx + ppocr_keys_v1.txt),
# 只加在本库自身的两个 target 上, 见下面各自的 target_compile_definitions
#---------------------------------------
# 第三方运行期依赖 (本库功能所需, 随库分发)
#---------------------------------------
# 只列运行期必须的第三方 DLL; 编译器运行时(libgcc_s_seh-1 / libstdc++-6 /
# libwinpthread-1)属于可执行文件自己的事, 由开发者自行准备, 这里不复制
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(WTL_THIRDPARTY_DLLS
            "${OpenCV_DIR}/bin/libopencv_world4110.dll"
            "${OpenCV_DIR}/bin/opencv_videoio_ffmpeg4110_64.dll"
            "${Interception_DIR}/library/x64/interception.dll"
            "${Zlib_DIR}/buildMinGW/libzlib.dll"
    )
    set(WTL_THIRDPARTY_LINK_LIBS
            "${OpenCV_DIR}/lib/libopencv_world4110.dll.a"
            "${Interception_DIR}/library/x64/interception.lib"
            "${Zlib_DIR}/buildMinGW/libzlib.dll.a"
            "${RadiOcr_LIB}"
    )
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    set(WTL_THIRDPARTY_DLLS
            "${OpenCV_DIR}/x64/vc16/bin/opencv_world4110.dll"
            "${OpenCV_DIR}/x64/vc16/bin/opencv_videoio_ffmpeg4110_64.dll"
            "${Interception_DIR}/library/x64/interception.dll"
            "${Zlib_DIR}/buildVS/zlib.dll"
    )
    set(WTL_THIRDPARTY_LINK_LIBS
            "${OpenCV_DIR}/x64/vc16/lib/opencv_world4110.lib"
            "${Interception_DIR}/library/x64/interception.lib"
            "${Zlib_DIR}/buildVS/zlib.lib"
            "${RadiOcr_LIB}"
    )
endif()

# OCR (RapidOCR) 的运行期依赖: 库 DLL + onnxruntime.dll (+ MinGW 版的共享 OpenCV)
list(APPEND WTL_THIRDPARTY_DLLS "${RadiOcr_DLL}" "${RadiOcr_ORT_DLL}")
if(RadiOcr_OPENCV_DLL)
    list(APPEND WTL_THIRDPARTY_DLLS "${RadiOcr_OPENCV_DLL}")
endif()
list(REMOVE_DUPLICATES WTL_THIRDPARTY_DLLS)

#---------------------------------------
# 依赖部署目的地
# 不再往 <版本目录>(vsVersion / mingwVersion) 里写 bin, 那里只放源码与头文件
#---------------------------------------
if(WIN32)
    set(WTL_DEPLOY_DIRS "${WTL_DLL_DIR}")
endif()

#---------------------------------------
# 包含目录
#---------------------------------------
include_directories(
        ${WTL_DIR}/include
        ${OpenCV_INCLUDE_DIRS}
        "${Interception_DIR}/library"
        ${Zlib_DIR}
        ${Zlib_DIR}/contrib/minizip
)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# 设置Release模式的编译选项
if(MSVC)
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /O2")
else()
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")
endif()

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
            ${WTL_DIR}/src/source/wtlStringCode.cpp
            ${WTL_DIR}/src/source/wtlFileManagement.cpp
            ${WTL_DIR}/src/source/wtlMediaToolkit.cpp
            ${WTL_DIR}/src/source/wtlTime.cpp
            ${WTL_DIR}/src/source/wtlOcr.cpp
            ${WTL_DIR}/src/source/wtlKeyBoard.cpp
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
            ${WTL_DIR}/src/source/wtlStringCode.cpp
            ${WTL_DIR}/src/source/wtlFileManagement.cpp
            ${WTL_DIR}/src/source/wtlMediaToolkit.cpp
            ${WTL_DIR}/src/source/wtlTime.cpp
            ${WTL_DIR}/src/source/wtlOcr.cpp
            ${WTL_DIR}/src/source/wtlKeyBoard.cpp
            ${WTL_DIR}/src/source/wtlMouse.cpp
            ${WTL_DIR}/include/winToolsH.hpp
            ${WTL_DIR}/include/wtlFileManagement.hpp
            ${WTL_DIR}/include/wtlMediaToolkit.hpp
            ${WTL_DIR}/include/wtlTime.hpp
            ${WTL_DIR}/include/wtlZip.hpp
            ${WTL_DIR}/include/wtlRegex.hpp
    )

    # RapidOCR 头文件与模型目录只对本库的编译单元可见 (第三方库不进公共头文件)
    target_include_directories(${PROJECT_NAME} PRIVATE "${RadiOcr_INCLUDE_DIR}")
    target_include_directories(${PROJECT_NAME}Static PRIVATE "${RadiOcr_INCLUDE_DIR}")
    target_compile_definitions(${PROJECT_NAME} PRIVATE RADIOOCR_MODELS_DIR="${RadiOcr_MODELS_DIR}")
    target_compile_definitions(${PROJECT_NAME}Static PRIVATE RADIOOCR_MODELS_DIR="${RadiOcr_MODELS_DIR}")

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
            ole32
            mfplay
            mfplat
            mfuuid
            shlwapi
            uuid
            # RapidOCR (OcrLite C API)
            ${RadiOcr_LIB}
            ws2_32
    )
    #---------------------------------------
    # Windows平台依赖处理
    #---------------------------------------
    if(WIN32)
        # 第三方DLL复制
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E make_directory
                "${WTL_DLL_DIR}"
                "${WTL_LINK_DIR}"
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${OpenCV_DIR}/3rdparty/ffmpeg/opencv_videoio_ffmpeg_64.dll"
                # opencv自带的ffmpeg在bin目录 #
                "${OpenCV_DIR}/bin/opencv_videoio_ffmpeg4110_64.dll"
                "${OpenCV_DIR}/bin/libopencv_world4110.dll"
                "${Interception_DIR}/library/x64/interception.dll"
                ${Zlib_DIR}/buildMinGW/libzlib.dll
                "${WTL_DLL_DIR}"
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${OpenCV_DIR}/lib/libopencv_world4110.dll.a"
                "${Interception_DIR}/library/x64/interception.lib"
                ${Zlib_DIR}/buildMinGW/libzlib.dll.a
                "${WTL_LINK_DIR}"
        )


        # 复制整个文件夹 #
        add_custom_target(${PROJECT_NAME}Dir ALL
                COMMAND ${CMAKE_COMMAND} -E copy_directory
                ${WTL_DIR}/include
                "${WTL_PACKAGE_SOURCE_DIR}/include"
                COMMAND ${CMAKE_COMMAND} -E copy_directory
                ${WTL_DIR}/src
                "${WTL_PACKAGE_SOURCE_DIR}/src"
        )

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
            ${WTL_DIR}/src/source/wtlStringCode.cpp
            ${WTL_DIR}/src/source/wtlFileManagement.cpp
            ${WTL_DIR}/src/source/wtlMediaToolkit.cpp
            ${WTL_DIR}/src/source/wtlTime.cpp
            ${WTL_DIR}/src/source/wtlOcr.cpp
            ${WTL_DIR}/src/source/wtlKeyBoard.cpp
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
            ${WTL_DIR}/src/source/wtlStringCode.cpp
            ${WTL_DIR}/src/source/wtlFileManagement.cpp
            ${WTL_DIR}/src/source/wtlMediaToolkit.cpp
            ${WTL_DIR}/src/source/wtlTime.cpp
            ${WTL_DIR}/src/source/wtlOcr.cpp
            ${WTL_DIR}/src/source/wtlKeyBoard.cpp
            ${WTL_DIR}/src/source/wtlMouse.cpp
            ${WTL_DIR}/include/winToolsH.hpp
            ${WTL_DIR}/include/wtlFileManagement.hpp
            ${WTL_DIR}/include/wtlMediaToolkit.hpp
            ${WTL_DIR}/include/wtlTime.hpp
            ${WTL_DIR}/include/wtlZip.hpp
            ${WTL_DIR}/include/wtlRegex.hpp
    )

    target_compile_definitions(WinTools PRIVATE wtl_EXPORTS)

    # RapidOCR 头文件与模型目录只对本库的编译单元可见 (第三方库不进公共头文件)
    target_include_directories(WinTools PRIVATE "${RadiOcr_INCLUDE_DIR}")
    target_include_directories(WinToolsStatic PRIVATE "${RadiOcr_INCLUDE_DIR}")
    target_compile_definitions(WinTools PRIVATE RADIOOCR_MODELS_DIR="${RadiOcr_MODELS_DIR}")
    target_compile_definitions(WinToolsStatic PRIVATE RADIOOCR_MODELS_DIR="${RadiOcr_MODELS_DIR}")

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
            ole32
            mfplay
            mfplat
            mfuuid
            shlwapi
            uuid
            # RapidOCR (OcrLite C API)
            ${RadiOcr_LIB}
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
                COMMAND ${CMAKE_COMMAND} -E make_directory
                "${WTL_DLL_DIR}"
                "${WTL_LINK_DIR}"
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${OpenCV_DIR}/bin/opencv_videoio_ffmpeg4110_64.dll"
                "${OpenCV_DIR}/x64/vc16/bin/opencv_world4110.dll"
                "${Interception_DIR}/library/x64/interception.dll"
                ${Zlib_DIR}/buildVS/zlib.dll
                "${WTL_DLL_DIR}"
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${OpenCV_DIR}/x64/vc16/lib/opencv_world4110.lib"
                "${Interception_DIR}/library/x64/interception.lib"
                ${Zlib_DIR}/buildVS/zlib.lib
                "${WTL_LINK_DIR}"
        )

        # 复制整个文件夹 #
        add_custom_target(${PROJECT_NAME}Dir ALL
                COMMAND ${CMAKE_COMMAND} -E copy_directory
                ${WTL_DIR}/include
                "${WTL_PACKAGE_SOURCE_DIR}/include"
                COMMAND ${CMAKE_COMMAND} -E copy_directory
                ${WTL_DIR}/src
                "${WTL_PACKAGE_SOURCE_DIR}/src"
        )

    endif()
endif ()

#---------------------------------------
# 第三方依赖部署 (两套编译器共用)
# 目的地: 构建输出目录的 source/src/lib
#---------------------------------------
if(WIN32)
    foreach(wtl_dep IN LISTS WTL_THIRDPARTY_DLLS)
        if(NOT EXISTS "${wtl_dep}")
            message(WARNING "缺少第三方运行期依赖: ${wtl_dep}")
            continue()
        endif()
        foreach(wtl_dest IN LISTS WTL_DEPLOY_DIRS)
            add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${wtl_dep}"
                    "${wtl_dest}"
            )
        endforeach()
    endforeach()
endif()

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
# 独立模块库（动态库 + 静态库）
#---------------------------------------
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(wtl_ZLIB_LIB "${Zlib_DIR}/buildMinGW/libzlib.dll.a")
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    set(wtl_ZLIB_LIB "${Zlib_DIR}/buildVS/zlib.lib")
endif()

add_library(wtlThrowError INTERFACE)
target_include_directories(wtlThrowError INTERFACE ${WTL_DIR}/include)

add_library(wtlTypeName INTERFACE)
target_include_directories(wtlTypeName INTERFACE ${WTL_DIR}/include)

add_library(
        wtlDict SHARED
        ${WTL_DIR}/src/source/wtlDict.cpp
        ${WTL_DIR}/include/wtlDict.hpp
)

add_library(
        wtlDictStatic STATIC
        ${WTL_DIR}/src/source/wtlDict.cpp
        ${WTL_DIR}/include/wtlDict.hpp
)

add_library(
        wtlPrint SHARED
        ${WTL_DIR}/src/source/wtlPrint.cpp
        ${WTL_DIR}/include/wtlPrintFormat.hpp
)

add_library(
        wtlPrintStatic STATIC
        ${WTL_DIR}/src/source/wtlPrint.cpp
        ${WTL_DIR}/include/wtlPrintFormat.hpp
)

add_library(
        wtlStringCode SHARED
        ${WTL_DIR}/src/source/wtlStringCode.cpp
        ${WTL_DIR}/include/wtlStringCode.hpp
)

add_library(
        wtlStringCodeStatic STATIC
        ${WTL_DIR}/src/source/wtlStringCode.cpp
        ${WTL_DIR}/include/wtlStringCode.hpp
)

add_library(
        wtlFileManagement SHARED
        ${WTL_DIR}/src/source/wtlFileManagement.cpp
        ${WTL_DIR}/include/wtlFileManagement.hpp
)

add_library(
        wtlFileManagementStatic STATIC
        ${WTL_DIR}/src/source/wtlFileManagement.cpp
        ${WTL_DIR}/include/wtlFileManagement.hpp
)

add_library(
        wtlMediaToolkit SHARED
        ${WTL_DIR}/src/source/wtlMediaToolkit.cpp
        ${WTL_DIR}/include/wtlMediaToolkit.hpp
)

add_library(
        wtlMediaToolkitStatic STATIC
        ${WTL_DIR}/src/source/wtlMediaToolkit.cpp
        ${WTL_DIR}/include/wtlMediaToolkit.hpp
)

add_library(
        wtlOcr SHARED
        ${WTL_DIR}/src/source/wtlOcr.cpp
        ${WTL_DIR}/include/wtlOcr.hpp
)

add_library(
        wtlOcrStatic STATIC
        ${WTL_DIR}/src/source/wtlOcr.cpp
        ${WTL_DIR}/include/wtlOcr.hpp
)

add_library(
        wtlTime SHARED
        ${WTL_DIR}/src/source/wtlTime.cpp
        ${WTL_DIR}/include/wtlTime.hpp
)

add_library(
        wtlTimeStatic STATIC
        ${WTL_DIR}/src/source/wtlTime.cpp
        ${WTL_DIR}/include/wtlTime.hpp
)

add_library(
        wtlKeyBoard SHARED
        ${WTL_DIR}/src/source/wtlKeyBoard.cpp
        ${WTL_DIR}/include/wtlKeyBoard.hpp
)

add_library(
        wtlKeyBoardStatic STATIC
        ${WTL_DIR}/src/source/wtlKeyBoard.cpp
        ${WTL_DIR}/include/wtlKeyBoard.hpp
)

add_library(
        wtlMouse SHARED
        ${WTL_DIR}/src/source/wtlMouse.cpp
        ${WTL_DIR}/include/wtlMouse.hpp
)

add_library(
        wtlMouseStatic STATIC
        ${WTL_DIR}/src/source/wtlMouse.cpp
        ${WTL_DIR}/include/wtlMouse.hpp
)

add_library(
        wtlZip SHARED
        ${WTL_DIR}/src/source/wtlZip.cpp
        ${WTL_DIR}/include/wtlZip.hpp
)

add_library(
        wtlZipStatic STATIC
        ${WTL_DIR}/src/source/wtlZip.cpp
        ${WTL_DIR}/include/wtlZip.hpp
)

add_library(
        wtlOpen SHARED
        ${WTL_DIR}/src/source/wtlOpen.cpp
        ${WTL_DIR}/include/wtlOpen.hpp
)

add_library(
        wtlOpenStatic STATIC
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

set(wtl_SHARED_TARGETS
        wtlDict
        wtlPrint
        wtlStringCode
        wtlFileManagement
        wtlMediaToolkit
        wtlOcr
        wtlTime
        wtlZip
        wtlOpen
        wtlRegex
        wtlKeyBoard
        wtlMouse
)

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    foreach(wtl_target IN LISTS wtl_SHARED_TARGETS)
        target_link_options(${wtl_target} PRIVATE -Wl,--export-all-symbols)
    endforeach()
elseif (MSVC)
    set_target_properties(${wtl_SHARED_TARGETS} PROPERTIES
            WINDOWS_EXPORT_ALL_SYMBOLS ON
    )
endif()

target_link_libraries(wtlDict PUBLIC wtlThrowError)
target_link_libraries(wtlDictStatic PUBLIC wtlThrowError)

target_link_libraries(wtlPrint PUBLIC wtlDictStatic wtlTypeName wtlThrowError)
target_link_libraries(wtlPrintStatic PUBLIC wtlDictStatic wtlTypeName wtlThrowError)

target_link_libraries(wtlFileManagement PRIVATE wtlStringCodeStatic)
target_link_libraries(wtlFileManagementStatic PUBLIC wtlStringCodeStatic)

target_link_libraries(wtlZip PRIVATE ${wtl_ZLIB_LIB})
target_link_libraries(wtlZipStatic PUBLIC ${wtl_ZLIB_LIB})

target_link_libraries(wtlOpen PUBLIC wtlStringCodeStatic wtlDictStatic)
target_link_libraries(wtlOpenStatic PUBLIC wtlStringCodeStatic wtlDictStatic)
target_link_libraries(wtlRegex PRIVATE wtlStringCodeStatic)
target_link_libraries(wtlRegexStatic PUBLIC wtlStringCodeStatic)

target_link_libraries(wtlKeyBoard PRIVATE
        wtlStringCodeStatic
        wtlTimeStatic
        "${Interception_DIR}/library/x64/interception.lib"
        user32
)
target_link_libraries(wtlKeyBoardStatic PUBLIC
        wtlStringCodeStatic
        wtlTimeStatic
        "${Interception_DIR}/library/x64/interception.lib"
        user32
)
target_link_libraries(wtlMouse PRIVATE
        wtlTimeStatic
        "${Interception_DIR}/library/x64/interception.lib"
        user32
)
target_link_libraries(wtlMouseStatic PUBLIC
        wtlTimeStatic
        "${Interception_DIR}/library/x64/interception.lib"
        user32
)

set(wtlMediaToolkit_DEPS
        wtlPrintStatic
        wtlFileManagementStatic
        wtlRegexStatic
        ${OpenCV_LIBS}
        gdiplus
        Shcore
        Windowscodecs
        ole32
        mfplay
        mfplat
        mfuuid
        shlwapi
        uuid
)

target_link_libraries(wtlMediaToolkit PRIVATE ${wtlMediaToolkit_DEPS})
target_link_libraries(wtlMediaToolkitStatic PUBLIC ${wtlMediaToolkit_DEPS})

#---------------------------------------
# OCR 模块依赖
#---------------------------------------
target_include_directories(wtlOcr PRIVATE "${RadiOcr_INCLUDE_DIR}")
target_include_directories(wtlOcrStatic PRIVATE "${RadiOcr_INCLUDE_DIR}")
target_compile_definitions(wtlOcr PRIVATE RADIOOCR_MODELS_DIR="${RadiOcr_MODELS_DIR}")
target_compile_definitions(wtlOcrStatic PRIVATE RADIOOCR_MODELS_DIR="${RadiOcr_MODELS_DIR}")

set(wtlOcr_DEPS
        ${OpenCV_LIBS}
        ${RadiOcr_LIB}
        user32
        gdi32
        ws2_32
)

if(MSVC)
    list(APPEND wtlOcr_DEPS
            crypt32
            secur32
            xmllite
            iphlpapi
    )
endif()

target_link_libraries(wtlOcr PRIVATE ${wtlOcr_DEPS})
target_link_libraries(wtlOcrStatic PUBLIC ${wtlOcr_DEPS})

# OCR 独立构建时也把运行期 DLL 放到 wtlOcr.dll 旁边。
if(WIN32)
    foreach(wtl_ocr_runtime_dll IN ITEMS
            "${RadiOcr_DLL}"
            "${RadiOcr_ORT_DLL}"
            "${RadiOcr_OPENCV_DLL}"
    )
        if(EXISTS "${wtl_ocr_runtime_dll}")
            add_custom_command(TARGET wtlOcr POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${wtl_ocr_runtime_dll}"
                    "$<TARGET_FILE_DIR:wtlOcr>"
            )
        endif()
    endforeach()
endif()

#---------------------------------------
# 构建输出中的 source/src/lib
# 统一存放模块二进制与第三方运行库
#---------------------------------------
if(WIN32)
    set(WTL_BIN_TARGETS
            WinTools
            WinToolsStatic
            wtlDict
            wtlDictStatic
            wtlPrint
            wtlPrintStatic
            wtlStringCode
            wtlStringCodeStatic
            wtlFileManagement
            wtlFileManagementStatic
            wtlMediaToolkit
            wtlMediaToolkitStatic
            wtlOcr
            wtlOcrStatic
            wtlTime
            wtlTimeStatic
            wtlZip
            wtlZipStatic
            wtlOpen
            wtlOpenStatic
            wtlRegex
            wtlRegexStatic
            wtlKeyBoard
            wtlKeyBoardStatic
            wtlMouse
            wtlMouseStatic
    )

    set(WTL_SHARED_OUTPUT_TARGETS
            WinTools
            ${wtl_SHARED_TARGETS}
    )

    set(WTL_STATIC_OUTPUT_TARGETS
            WinToolsStatic
            wtlDictStatic
            wtlPrintStatic
            wtlStringCodeStatic
            wtlFileManagementStatic
            wtlMediaToolkitStatic
            wtlOcrStatic
            wtlTimeStatic
            wtlZipStatic
            wtlOpenStatic
            wtlRegexStatic
            wtlKeyBoardStatic
            wtlMouseStatic
    )

    foreach(wtl_target IN LISTS WTL_SHARED_OUTPUT_TARGETS)
        set_target_properties(${wtl_target} PROPERTIES
                RUNTIME_OUTPUT_DIRECTORY "${WTL_DLL_DIR}"
                ARCHIVE_OUTPUT_DIRECTORY "${WTL_LINK_DIR}"
                LIBRARY_OUTPUT_DIRECTORY "${WTL_DLL_DIR}"
        )
    endforeach()

    foreach(wtl_target IN LISTS WTL_STATIC_OUTPUT_TARGETS)
        set_target_properties(${wtl_target} PROPERTIES
                ARCHIVE_OUTPUT_DIRECTORY "${WTL_STATIC_DIR}"
        )
    endforeach()

    add_custom_target(WinToolsBinDir ALL
            COMMAND ${CMAKE_COMMAND} -E make_directory
            "${WTL_DLL_DIR}"
            "${WTL_LINK_DIR}"
            "${WTL_STATIC_DIR}"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${WTL_THIRDPARTY_DLLS}
            "${WTL_DLL_DIR}"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${WTL_THIRDPARTY_LINK_LIBS}
            "${WTL_LINK_DIR}"
            DEPENDS ${WTL_BIN_TARGETS}
    )
endif()
