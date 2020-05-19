g++ -g `sdl2-config --cflags` -Iimgui -Iimgui/examples -Iimgui/examples/libs/gl3w \
    -ffast-math \
    instrument.cpp pattern.cpp song.cpp playback.cpp playback_lut.cpp load_mod.cpp guimain.cpp test.cpp \
    imgui/imgui*.cpp imgui/examples/imgui_impl_opengl3.cpp imgui/examples/imgui_impl_sdl.cpp imgui/examples/libs/gl3w/GL/gl3w.c \
    -lSDL2 -lGL -ldl
