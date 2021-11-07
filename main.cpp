#include "app.h"
#include "ui/text.h"
#include <cstdlib>
#include <exception>
#include <glad/glad.h>
#include <SDL2/SDL.h>

#define OPENGL_DEBUG

// TODO: remove this
extern "C" 
{
    // request dedicated graphics card for Nvidia and AMD
    // for eg. laptops with two graphics chipsets
    // https://stackoverflow.com/a/39047129
    __declspec(dllexport) unsigned long NvOptimusEnablement = 0x00000001;
    __declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}

std::ostream & operator<<(std::ostream &o, SDL_version version);
void messageCallback(GLenum source, GLenum type, GLuint id, GLenum severity,
                     GLsizei length, const GLchar *msg, const void *data);

int main(int argc, char *argv[])
{
    SDL_version compiled, linked;
    SDL_VERSION(&compiled);
    SDL_GetVersion(&linked);
    cout << "SDL version: " <<compiled<< " (compiled), "
        <<linked<< " (linked)\n";
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "SDL Error",
                                 SDL_GetError(), nullptr);
        return EXIT_FAILURE;
    }

#ifdef OPENGL_DEBUG
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);
#else
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
#endif
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,
                        SDL_GL_CONTEXT_PROFILE_COMPATIBILITY); // TODO remove
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 5);
    // must be set before creating window
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4);

    SDL_Window *window = SDL_CreateWindow("chromatracker",
        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        800, 600,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);  // TODO highdpi?
    if (!window) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "SDL Error",
                                 SDL_GetError(), nullptr);
        SDL_Quit();
        return EXIT_FAILURE;
    }

    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    if (!gl_context) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "SDL OpenGL Error",
                                 SDL_GetError(), window);
        SDL_Quit();
        return EXIT_FAILURE;
    }
    SDL_GL_SetSwapInterval(1);  // enable vsync

    if (!gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress)) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "glad Error",
                                 "Error initializing OpenGL context", window);
        SDL_Quit();
        return EXIT_FAILURE;
    }

    cout << "OpenGL renderer: " <<glGetString(GL_RENDERER)<< "\n";
    cout << "OpenGL version: " <<glGetString(GL_VERSION)<< "\n";

#ifdef OPENGL_DEBUG
    // requires KHR_debug or OpenGL 4.3+
    glEnable(GL_DEBUG_OUTPUT);
    glDebugMessageCallback(messageCallback, NULL);
    glDebugMessageControl(GL_DONT_CARE, GL_DEBUG_TYPE_OTHER,
        GL_DONT_CARE, 0, nullptr, GL_FALSE);
#endif

    try {
        chromatracker::ui::initText();
    } catch (std::exception e) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "FreeType Error",
                                 e.what(), window);
        SDL_Quit();
        return EXIT_FAILURE;
    }

    vector<string> args;
    args.reserve(argc);
    for (int i = 0; i < argc; i++)
        args.emplace_back(argv[i]);

    int result = EXIT_SUCCESS;
    try {
        chromatracker::App app(window);
        app.main(args);
    } catch (std::exception e) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error running app",
                                 e.what(), window);
        result = EXIT_FAILURE;
    }

    SDL_DestroyWindow(window);
    SDL_Quit();
    chromatracker::ui::closeText();
    return result;
}

std::ostream & operator<<(std::ostream &o, SDL_version version)
{
    return o <<(int)version.major<< "." <<(int)version.minor<< "."
        <<(int)version.patch;
}

void messageCallback(GLenum source, GLenum type, GLuint id, GLenum severity,
                     GLsizei length, const GLchar *msg, const void *data)
{
    cout << "[OpenGL] " <<msg<< "\n";
}
