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
    event.cpp
    file/itloader.cpp
    glad/glad.c
    play/sampleplay.cpp
    play/songplay.cpp
    play/trackplay.cpp
    song.cpp
    ui/font.cpp
    ui/text.cpp
    units.cpp)

target_include_directories(chromatracker PRIVATE
    .
    ${CHROMA_INCLUDE})

# TODO static vs shared?
target_link_libraries(chromatracker SDL2 SDL2main)

add_custom_command(TARGET chromatracker POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CHROMA_LIB}/SDL2.dll
        $<TARGET_FILE_DIR:chromatracker>)
