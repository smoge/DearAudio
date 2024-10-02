// Dear ImGui: SDL2 + OpenGL
// - FAQ                  https://dearimgui.com/faq
// - Getting Started      https://dearimgui.com/getting-started
// - Documentation        https://dearimgui.com/docs (same as your local docs/
// folder).
// - Introduction, links and more at the top of imgui.cpp

#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_sdl2.h"
#include "implot.h"
#include <SDL.h>
#include <cmath>
#include <stdio.h>
#include <vector>

#include <deque>
#include <mutex>
#include <portaudio.h>

#include <SDL_opengl.h>

// This example can also compile and run with Emscripten! See
// 'Makefile.emscripten' for details. #ifdef __EMSCRIPTEN__ #include
// "../libs/emscripten/emscripten_mainloop_stub.h" #endif

struct AudioData {
  std::deque<float> buffer;
  std::mutex mutex;
  size_t max_size = 44100; // 1 second of audio at 44.1kHz

  void push(float sample) {
    std::lock_guard<std::mutex> lock(mutex);
    if (buffer.size() >= max_size) {
      buffer.pop_front();
    }
    buffer.push_back(sample);
  }
};

AudioData audioData;

static int paCallback(const void *inputBuffer, void *outputBuffer,
                      unsigned long framesPerBuffer,
                      const PaStreamCallbackTimeInfo *timeInfo,
                      PaStreamCallbackFlags statusFlags, void *userData) {
  float *in = (float *)inputBuffer;
  AudioData *data = (AudioData *)userData;

  for (unsigned int i = 0; i < framesPerBuffer; i++) {
    data->push(in[i]);
  }

  return paContinue;
}

// Function to generate sample audio data (replace with your actual audio input)
float generateAudioSample(float time) {
  return 0.5f * std::sin(2 * M_PI * 440 * time) +
         0.25f * std::sin(2 * M_PI * 880 * time);
}

void ShowAudioWaveform() {
  static float t = ImGui::GetTime();
  static float history = 0.1f; // Show 0.1 seconds of history

  if (ImPlot::BeginPlot("Audio Waveform")) {
    ImPlot::SetupAxisLimits(ImAxis_X1, t - history, t, ImGuiCond_Always);
    ImPlot::SetupAxisLimits(ImAxis_Y1, -1, 1);

    std::vector<float> x, y;
    {
      std::lock_guard<std::mutex> lock(audioData.mutex);
      size_t num_samples =
          std::min(audioData.buffer.size(), size_t(history * 44100));
      x.resize(num_samples);
      y.resize(num_samples);
      for (size_t i = 0; i < num_samples; ++i) {
        x[i] = t - (num_samples - i) * (1.0f / 44100);
        y[i] = audioData.buffer[audioData.buffer.size() - num_samples + i];
      }
    }

    ImPlot::PlotLine("Waveform", x.data(), y.data(), x.size());
    ImPlot::EndPlot();
  }
  t = ImGui::GetTime();
}

const int SAMPLE_COUNT = 1000;
std::vector<double> xs(SAMPLE_COUNT);
std::vector<double> ys(SAMPLE_COUNT);
double t = 0;

// Parameters for the sine wave
static float frequency = 2.0f;
static float amplitude = 1.0f;
static float speed = 0.05f;

void GenerateSineWave() {
  for (int i = 0; i < SAMPLE_COUNT; ++i) {
    xs[i] = i * (10.0 / SAMPLE_COUNT);
    ys[i] = amplitude * std::sin(2 * M_PI * frequency * (xs[i] - t));
  }
  t += speed; // This creates the movement effect
}

void ShowMovingSineWave() {
  // Controls for frequency and amplitude
  ImGui::SliderFloat("Frequency", &frequency, 0.1f, 10.0f, "%.1f Hz");
  ImGui::SliderFloat("Amplitude", &amplitude, 0.1f, 2.0f, "%.1f");
  ImGui::SliderFloat("Speed", &speed, 0.01f, 0.2f, "%.2f");

  GenerateSineWave();

  if (ImPlot::BeginPlot("Moving Sine Wave")) {
    ImPlot::SetupAxes("Time", "Amplitude");
    ImPlot::SetupAxisLimits(ImAxis_X1, 0, 10, ImGuiCond_Always);
    ImPlot::SetupAxisLimits(ImAxis_Y1, -2, 2, ImGuiCond_Always);
    ImPlot::PlotLine("Sine Wave", xs.data(), ys.data(), SAMPLE_COUNT);
    ImPlot::EndPlot();
  }
}

// Main code
int main(int, char **) {
  // Setup SDL
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) !=
      0) {
    printf("Error: %s\n", SDL_GetError());
    return -1;
  }

// Decide GL+GLSL versions
#if defined(IMGUI_IMPL_OPENGL_ES2)
  // GL ES 2.0 + GLSL 100
  const char *glsl_version = "#version 100";
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#elif defined(__APPLE__)
  // GL 3.2 Core + GLSL 150
  const char *glsl_version = "#version 150";
  SDL_GL_SetAttribute(
      SDL_GL_CONTEXT_FLAGS,
      SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG); // Always required on Mac
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
#else
  // GL 3.0 + GLSL 130
  const char *glsl_version = "#version 130";
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#endif

  SDL_SetHint(SDL_HINT_IME_SHOW_UI, "1");

  // Create window with graphics context
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
  SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
  SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
  SDL_WindowFlags window_flags =
      (SDL_WindowFlags)(SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE |
                        SDL_WINDOW_ALLOW_HIGHDPI);
  SDL_Window *window = SDL_CreateWindow(
      "Dear ImGui SDL2+OpenGL3 example", SDL_WINDOWPOS_CENTERED,
      SDL_WINDOWPOS_CENTERED, 1280, 720, window_flags);
  if (window == nullptr) {
    printf("Error: SDL_CreateWindow(): %s\n", SDL_GetError());
    return -1;
  }
  SDL_GLContext gl_context = SDL_GL_CreateContext(window);
  SDL_GL_MakeCurrent(window, gl_context);
  SDL_GL_SetSwapInterval(1); // Enable vsync

  // Setup Dear ImGui context
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO &io = ImGui::GetIO();
  (void)io;

  // Load custom font
  ImFont *font = io.Fonts->AddFontFromFileTTF(
      "/home/smoge/tmp/imgui/misc/fonts/Roboto-Medium.ttf", 16.0f);
  IM_ASSERT(font != NULL);

  io.ConfigFlags |=
      ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
  io.ConfigFlags |=
      ImGuiConfigFlags_NavEnableGamepad;              // Enable Gamepad Controls
  io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;   // Enable Docking
  io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable; // Enable Multi-Viewport
                                                      // / Platform Windows
  // io.ConfigViewportsNoAutoMerge = true;
  // io.ConfigViewportsNoTaskBarIcon = true;

  io.FontDefault = font;
  io.Fonts->Build(); // Note the semicolon here

  ////////////////////////////
  // Setup ImPlot context
  ImPlot::CreateContext();
  /////////////////////////////

  // Setup Dear ImGui style
  ImGui::StyleColorsDark();
  // ImGui::StyleColorsLight();

  // Modify colors
  ImGuiStyle &style = ImGui::GetStyle();
  style.Colors[ImGuiCol_Header] = ImVec4(0.2f, 0.2f, 0.2f, 1.0f); // Dark gray
  style.Colors[ImGuiCol_HeaderHovered] =
      ImVec4(0.3f, 0.3f, 0.3f, 1.0f); // Slightly lighter gray
  style.Colors[ImGuiCol_HeaderActive] =
      ImVec4(0.4f, 0.4f, 0.4f, 1.0f); // Even lighter gray
  //
  // If you want to change other blue accents, you might want to modify these
  // as well:
  style.Colors[ImGuiCol_Button] = ImVec4(0.2f, 0.2f, 0.2f, 1.0f);
  style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.3f, 0.3f, 0.3f, 1.0f);
  style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.4f, 0.4f, 0.4f, 1.0f);
  style.Colors[ImGuiCol_FrameBg] = ImVec4(0.2f, 0.2f, 0.2f, 1.0f);
  style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.3f, 0.3f, 0.3f, 1.0f);
  style.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.4f, 0.4f, 0.4f, 1.0f);

  // When viewports are enabled we tweak WindowRounding/WindowBg so platform
  // windows can look identical to regular ones. ImGuiStyle& style =
  // ImGui::GetStyle(); if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
  // {
  //     style.WindowRounding = 0.3f;
  //     style.Colors[ImGuiCol_WindowBg].w = 0.5f;
  // }

  // Setup Platform/Renderer backends
  ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
  ImGui_ImplOpenGL3_Init(glsl_version);

  // Load Fonts
  // - If no fonts are loaded, dear imgui will use the default font. You can
  // also load multiple fonts and use ImGui::PushFont()/PopFont() to select
  // them.
  // - AddFontFromFileTTF() will return the ImFont* so you can store it if you
  // need to select the font among multiple.
  // - If the file cannot be loaded, the function will return a nullptr. Please
  // handle those errors in your application (e.g. use an assertion, or display
  // an error and quit).
  // - The fonts will be rasterized at a given size (w/ oversampling) and stored
  // into a texture when calling ImFontAtlas::Build()/GetTexDataAsXXXX(), which
  // ImGui_ImplXXXX_NewFrame below will call.
  // - Use '#define IMGUI_ENABLE_FREETYPE' in your imconfig file to use Freetype
  // for higher quality font rendering.
  // - Read 'docs/FONTS.md' for more instructions and details.
  // - Remember that in C/C++ if you want to include a backslash \ in a string
  // literal you need to write a double backslash \\ !
  // - Our Emscripten build process allows embedding fonts to be accessible at
  // runtime from the "fonts/" folder. See Makefile.emscripten for details.
  // io.Fonts->AddFontDefault();
  // io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\segoeui.ttf", 18.0f);
  // io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf", 16.0f);
  // io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf", 16.0f);
  // io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf", 15.0f);
  // ImFont* font =
  // io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf", 18.0f,
  // nullptr, io.Fonts->GetGlyphRangesJapanese()); IM_ASSERT(font != nullptr);

  // Initialize PortAudio
  PaError err;
  PaStream *stream = nullptr;

  err = Pa_Initialize();
  if (err == paNoError) {
    err = Pa_OpenDefaultStream(&stream,
                               1,         // mono input
                               0,         // no output
                               paFloat32, // 32 bit floating point output
                               44100,     // sample rate
                               256,       // frames per buffer
                               paCallback, &audioData);
    if (err == paNoError) {
      err = Pa_StartStream(stream);
    }
  }

  if (err != paNoError) {
    fprintf(stderr, "PortAudio error: %s\n", Pa_GetErrorText(err));
    // Cleanup PortAudio if needed
    if (stream) {
      Pa_CloseStream(stream);
    }
    Pa_Terminate();
    // ... (cleanup SDL and ImGui)
    return 1;
  }

  // Our state
  bool show_demo_window = true;
  bool show_another_window = false;
  ImVec4 clear_color = ImVec4(0.1f, 0.1f, 0.1f, 1.00f);

  // Main loop
  bool done = false;
  // #ifdef __EMSCRIPTEN__
  //   // For an Emscripten build we are disabling file-system access, so let's
  //   not
  //   // attempt to do a fopen() of the imgui.ini file. You may manually call
  //   // LoadIniSettingsFromMemory() to load settings from your own storage.
  //   io.IniFilename = nullptr;
  //   EMSCRIPTEN_MAINLOOP_BEGIN
  // #else
  while (!done)
  // #endif
  {
    // Poll and handle events (inputs, window resize, etc.)
    // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to
    // tell if dear imgui wants to use your inputs.
    // - When io.WantCaptureMouse is true, do not dispatch mouse input data to
    // your main application, or clear/overwrite your copy of the mouse data.
    // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input
    // data to your main application, or clear/overwrite your copy of the
    // keyboard data. Generally you may always pass all inputs to dear imgui,
    // and hide them from your application based on those two flags.
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      ImGui_ImplSDL2_ProcessEvent(&event);
      if (event.type == SDL_QUIT)
        done = true;
      if (event.type == SDL_WINDOWEVENT &&
          event.window.event == SDL_WINDOWEVENT_CLOSE &&
          event.window.windowID == SDL_GetWindowID(window))
        done = true;
    }
    if (SDL_GetWindowFlags(window) & SDL_WINDOW_MINIMIZED) {
      SDL_Delay(10);
      continue;
    }

    // Start the Dear ImGui frame
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();

    // 1. Show the big demo window (Most of the sample code is in
    // ImGui::ShowDemoWindow()! You can browse its code to learn more about Dear
    // ImGui!).
    if (show_demo_window)
      ImGui::ShowDemoWindow(&show_demo_window);

    // 2. Show a simple window that we create ourselves. We use a Begin/End pair
    // to create a named window.
    {
      static float f = 0.0f;
      static int counter = 0;

      ImGui::Begin("Hello, world!"); // Create a window called "Hello, world!"
                                     // and append into it.

      ImGui::Text("This is some useful text."); // Display some text (you can
                                                // use a format strings too)
      ImGui::Checkbox(
          "Demo Window",
          &show_demo_window); // Edit bools storing our window open/close state
      ImGui::Checkbox("Another Window", &show_another_window);

      ImGui::SliderFloat("float", &f, 0.0f,
                         1.0f); // Edit 1 float using a slider from 0.0f to 1.0f
      ImGui::ColorEdit3(
          "clear color",
          (float *)&clear_color); // Edit 3 floats representing a color

      if (ImGui::Button("Button")) // Buttons return true when clicked (most
                                   // widgets return true when edited/activated)
        counter++;
      ImGui::SameLine();
      ImGui::Text("counter = %d", counter);

      ImGui::Text("Application average %.3f ms/frame (%.1f FPS)",
                  1000.0f / io.Framerate, io.Framerate);
      ImGui::End();
    }

    // 3. Show another simple window.
    if (show_another_window) {
      ImGui::Begin(
          "Another Window",
          &show_another_window); // Pass a pointer to our bool variable (the
                                 // window will have a closing button that will
                                 // clear the bool when clicked)
      ImGui::Text("Hello from another window!");
      if (ImGui::Button("Close Me"))
        show_another_window = false;
      ImGui::End();
    }

    // ImPlot example and Audio Visualizer
    {
      ImGui::Begin("ImPlot Example");
      if (ImPlot::BeginPlot("My Plot")) {
        static float x[] = {1, 2, 3, 4, 5};
        static float y[] = {1, 2, 4, 8, 16};
        ImPlot::PlotLine("Line", x, y, 5);
        ImPlot::EndPlot();
      }
      ImGui::End();

      ImGui::Begin("Audio Visualizer");
      ShowAudioWaveform();
      ImGui::End();
    }

    // Rendering
    ImGui::Render();
    glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
    glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w,
                 clear_color.z * clear_color.w, clear_color.w);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    // Update and Render additional Platform Windows
    // (Platform functions may change the current OpenGL context, so we
    // save/restore it to make it easier to paste this code elsewhere.
    //  For this specific demo app we could also call SDL_GL_MakeCurrent(window,
    //  gl_context) directly)
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
      SDL_Window *backup_current_window = SDL_GL_GetCurrentWindow();
      SDL_GLContext backup_current_context = SDL_GL_GetCurrentContext();
      ImGui::UpdatePlatformWindows();
      ImGui::RenderPlatformWindowsDefault();
      SDL_GL_MakeCurrent(backup_current_window, backup_current_context);
    }

    SDL_GL_SwapWindow(window);
  }
#ifdef __EMSCRIPTEN__
  EMSCRIPTEN_MAINLOOP_END;
#endif

  // Cleanup
  err = Pa_StopStream(stream);
  if (err != paNoError) {
    fprintf(stderr, "PortAudio error: %s\n", Pa_GetErrorText(err));
  }
  err = Pa_CloseStream(stream);
  if (err != paNoError) {
    fprintf(stderr, "PortAudio error: %s\n", Pa_GetErrorText(err));
  }
  err = Pa_Terminate();
  if (err != paNoError) {
    fprintf(stderr, "PortAudio error: %s\n", Pa_GetErrorText(err));
  }

  // Cleanup
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplSDL2_Shutdown();
  ImPlot::DestroyContext();
  ImGui::DestroyContext();

  SDL_GL_DeleteContext(gl_context);
  SDL_DestroyWindow(window);
  SDL_Quit();

  return 0;
}
