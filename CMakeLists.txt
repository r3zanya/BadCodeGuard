cmake_minimum_required(VERSION 3.21)
project(BadCodeGuard VERSION 0.3.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

if(MSVC)
    add_compile_options(/utf-8 /W4)
endif()

find_package(Qt6 REQUIRED COMPONENTS Widgets Svg Network)

# --- Third-party code (vendored; see third_party/*/LICENSE|COPYING) ---------
# miniz (MIT): reads .zip archives in memory.
add_library(miniz STATIC third_party/miniz/miniz.c)
target_include_directories(miniz PUBLIC third_party/miniz)
if(MSVC)
    target_compile_options(miniz PRIVATE /W0)
endif()

# YARA 4.x (BSD-3): pattern-matching rules. Built without OpenSSL, so the pe
# module has no Authenticode fields; hashes use the Windows crypto API.
set(YARA_DIR third_party/yara/libyara)
add_library(yara STATIC
    ${YARA_DIR}/ahocorasick.c ${YARA_DIR}/arena.c ${YARA_DIR}/atoms.c ${YARA_DIR}/base64.c
    ${YARA_DIR}/bitmask.c ${YARA_DIR}/compiler.c ${YARA_DIR}/exec.c ${YARA_DIR}/exefiles.c
    ${YARA_DIR}/filemap.c ${YARA_DIR}/grammar.c ${YARA_DIR}/hash.c ${YARA_DIR}/hex_grammar.c
    ${YARA_DIR}/hex_lexer.c ${YARA_DIR}/lexer.c ${YARA_DIR}/libyara.c ${YARA_DIR}/mem.c
    ${YARA_DIR}/modules.c ${YARA_DIR}/notebook.c ${YARA_DIR}/object.c ${YARA_DIR}/parser.c
    ${YARA_DIR}/proc.c ${YARA_DIR}/re.c ${YARA_DIR}/re_grammar.c ${YARA_DIR}/re_lexer.c
    ${YARA_DIR}/rules.c ${YARA_DIR}/scan.c ${YARA_DIR}/scanner.c ${YARA_DIR}/sizedstr.c
    ${YARA_DIR}/stack.c ${YARA_DIR}/stopwatch.c ${YARA_DIR}/stream.c ${YARA_DIR}/strutils.c
    ${YARA_DIR}/threading.c ${YARA_DIR}/simple_str.c
    ${YARA_DIR}/proc/windows.c
    ${YARA_DIR}/tlshc/tlsh_util.c ${YARA_DIR}/tlshc/tlsh_impl.c ${YARA_DIR}/tlshc/tlsh.c
    ${YARA_DIR}/modules/console/console.c ${YARA_DIR}/modules/dotnet/dotnet.c
    ${YARA_DIR}/modules/elf/elf.c ${YARA_DIR}/modules/hash/hash.c
    ${YARA_DIR}/modules/math/math.c ${YARA_DIR}/modules/pe/pe.c
    ${YARA_DIR}/modules/pe/pe_utils.c ${YARA_DIR}/modules/string/string.c
    ${YARA_DIR}/modules/time/time.c
)
target_include_directories(yara PUBLIC ${YARA_DIR}/include PRIVATE ${YARA_DIR})
target_compile_definitions(yara PUBLIC YR_BUILDING_STATIC_LIB
    PRIVATE _CRT_SECURE_NO_WARNINGS USE_WINDOWS_PROC HASH_MODULE DOTNET_MODULE HAVE_WINCRYPT_H
            BUCKETS_128 CHECKSUM_1B)
target_link_libraries(yara PUBLIC crypt32 ws2_32 advapi32)
if(MSVC)
    target_compile_options(yara PRIVATE /W0)
endif()

# Scanning engine: no UI dependencies, only QtCore.
file(GLOB ENGINE_SOURCES CONFIGURE_DEPENDS src/engine/*.cpp src/engine/*.h)
add_library(bcg_engine STATIC ${ENGINE_SOURCES})
target_include_directories(bcg_engine PUBLIC src)
target_link_libraries(bcg_engine PUBLIC Qt6::Core yara miniz PRIVATE wintrust crypt32)

# System layer: registry, elevation, console tools, tweaks, system information.
file(GLOB SYS_SOURCES CONFIGURE_DEPENDS src/sys/*.cpp src/sys/*.h)
add_library(bcg_sys STATIC ${SYS_SOURCES})
target_include_directories(bcg_sys PUBLIC src)
target_link_libraries(bcg_sys PUBLIC Qt6::Core Qt6::Network bcg_engine PRIVATE advapi32 shell32 user32 crypt32)

# Engine and system-layer tests (console only): build target bcg_tests, run bcg_tests.exe.
add_executable(bcg_tests tests/engine_tests.cpp)
target_link_libraries(bcg_tests PRIVATE bcg_sys bcg_engine Qt6::Core)
# Needs internet and writes to the real data folder: build it on purpose.
add_executable(bcg_nettests EXCLUDE_FROM_ALL tests/net_tests.cpp)
target_link_libraries(bcg_nettests PRIVATE bcg_sys bcg_engine Qt6::Core Qt6::Network)

file(GLOB_RECURSE UI_SOURCES CONFIGURE_DEPENDS src/ui/*.cpp src/ui/*.h)

add_executable(BadCodeGuard WIN32
    src/main.cpp
    ${UI_SOURCES}
    resources/resources.qrc
    resources/app.rc
    resources/BadCodeGuard.manifest
)
target_include_directories(BadCodeGuard PRIVATE src)
target_compile_definitions(BadCodeGuard PRIVATE APP_VERSION="${PROJECT_VERSION}")
target_link_libraries(BadCodeGuard PRIVATE bcg_engine bcg_sys Qt6::Widgets Qt6::Svg)
