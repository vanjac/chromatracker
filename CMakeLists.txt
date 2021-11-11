cmake_minimum_required(VERSION 3.20)
project(chromatracker)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

file(TO_CMAKE_PATH $ENV{CHROMA_INCLUDE} CHROMA_INCLUDE)
file(TO_CMAKE_PATH $ENV{CHROMA_SRC} CHROMA_SRC)
file(TO_CMAKE_PATH $ENV{CHROMA_LIB} CHROMA_LIB)

link_directories(${CHROMA_LIB})

add_executable(chromatracker
    main.cpp
    app.cpp
    cursor.cpp
    edit/songops.cpp
    event.cpp
    file/chromaloader.cpp
    file/chromawriter.cpp
    file/itloader.cpp
    file/types.cpp
    glad/glad.c
    play/jam.cpp
    play/sampleplay.cpp
    play/songplay.cpp
    play/trackplay.cpp
    song.cpp
    ui/draw.cpp
    ui/layout.cpp
    ui/panels/browser.cpp
    ui/panels/eventkeyboard.cpp
    ui/text.cpp
    units.cpp)

target_include_directories(chromatracker PRIVATE
    .
    ${CHROMA_INCLUDE})

# TODO static vs shared?
target_link_libraries(chromatracker SDL2 SDL2main freetype)

add_custom_command(TARGET chromatracker POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CHROMA_LIB}/SDL2.dll
        $<TARGET_FILE_DIR:chromatracker>)
# SDL2_ttf
add_custom_command(TARGET chromatracker POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CHROMA_LIB}/freetype.dll
        $<TARGET_FILE_DIR:chromatracker>)
# data files
add_custom_command(TARGET chromatracker POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${PROJECT_SOURCE_DIR}/assets/NotoSansMono-Bold.ttf
        $<TARGET_FILE_DIR:chromatracker>)
