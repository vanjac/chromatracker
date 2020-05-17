g++ -g `sdl2-config --cflags` -Iimgui -Iimgui/examples -Iimgui/examples/libs/gl3w \
    -ffast-math \
    instrument.c pattern.c song.c playback.c load_mod.c guimain.cpp test.cpp \
    imgui/imgui*.cpp imgui/examples/imgui_impl_opengl3.cpp imgui/examples/imgui_impl_sdl.cpp imgui/examples/libs/gl3w/GL/gl3w.c \
    -lSDL2 -lGL -ldl
