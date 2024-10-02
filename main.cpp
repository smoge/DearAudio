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

#include <SDL_opengl.h>

#include <jack/jack.h> 

struct AudioData {
  std::deque<float> buffer;
  std::mutex mutex;
  size_t max_size = 44100; // Buffer size for 1 second 

  void push(float sample) {
    std::lock_guard<std::mutex> lock(mutex);
    if (buffer.size() >= max_size) {
      buffer.pop_front();
    }
    buffer.push_back(sample);
  }
};

AudioData audioData;
jack_client_t *client;   // JACK client
jack_port_t *input_port; // JACK input port


// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
// !JACK audio callback function

int jackCallback(jack_nframes_t nframes, void *arg) {
  jack_default_audio_sample_t *in =
      (jack_default_audio_sample_t *)jack_port_get_buffer(input_port, nframes);
  AudioData *data = (AudioData *)arg;

  // Process incoming audio frames
  for (unsigned int i = 0; i < nframes; i++) {
    data->push(in[i]);
  }

  return 0;
}

// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!


// Test function to generate sample audio data
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

int main(int, char **) {
  // Setup SDL
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) !=
      0) {
    printf("Error: %s\n", SDL_GetError());
    return -1;
  }

  // Decide GL+GLSL versions
#if defined(IMGUI_IMPL_OPENGL_ES2)
  const char *glsl_version = "#version 100";
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#elif defined(__APPLE__)
  const char *glsl_version = "#version 150";
  SDL_GL_SetAttribute(
      SDL_GL_CONTEXT_FLAGS,
      SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG); // Always required on Mac
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
#else
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
  ImFont *font =
      io.Fonts->AddFontFromFileTTF("../fonts/Roboto-Medium.ttf", 16.0f);
  IM_ASSERT(font != NULL);

  io.ConfigFlags |=
      ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
  io.ConfigFlags |=
      ImGuiConfigFlags_NavEnableGamepad;              // Enable Gamepad Controls
  io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;   // Enable Docking
  io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable; // Enable Multi-Viewport

  io.FontDefault = font;
  io.Fonts->Build();

  ////////////////////////////
  // Setup ImPlot context
  ImPlot::CreateContext();
  /////////////////////////////

  // Setup Dear ImGui style
  ImGui::StyleColorsDark();

  // Setup Platform/Renderer backends
  ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
  ImGui_ImplOpenGL3_Init(glsl_version);

  // Initialize JACK
  const char *client_name = "imgui_audio";
  jack_options_t options = JackNullOption;
  client = jack_client_open(client_name, options, nullptr);

  if (!client) {
    fprintf(stderr, "Could not create JACK client\n");
    return 1;
  }

  jack_set_process_callback(client, jackCallback, &audioData);

  input_port = jack_port_register(client, "input", JACK_DEFAULT_AUDIO_TYPE,
                                  JackPortIsInput, 0);
  if (!input_port) {
    fprintf(stderr, "Could not register input port\n");
    jack_client_close(client);
    return 1;
  }

  if (jack_activate(client)) {
    fprintf(stderr, "Cannot activate client\n");
    jack_client_close(client);
    return 1;
  }

  // ! Connect input port to the system's input
  const char **ports = jack_get_ports(client, nullptr, nullptr,
                                      JackPortIsPhysical | JackPortIsOutput);
  if (!ports) {
    fprintf(stderr, "No available physical ports\n");
    jack_client_close(client);
    return 1;
  }

  if (jack_connect(client, ports[0], jack_port_name(input_port))) {
    fprintf(stderr, "Cannot connect input port\n");
  }

  free(ports);




  // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
  // ! Main loop
  bool done = false;

  while (!done) {
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

    // Show the demo window
    static bool show_demo_window = true;
    if (show_demo_window)
      ImGui::ShowDemoWindow(&show_demo_window);

    // Show the audio visualizer and sine wave example
    ImGui::Begin("Audio Visualizer");
    ShowAudioWaveform();
    ImGui::End();

    ImGui::Begin("Sine Wave Example");
    ShowMovingSineWave();
    ImGui::End();

    // Rendering
    ImGui::Render();
    glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
    glClearColor(0.1f, 0.1f, 0.1f, 1.00f);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    // Handle multiple viewports
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
      SDL_Window *backup_current_window = SDL_GL_GetCurrentWindow();
      SDL_GLContext backup_current_context = SDL_GL_GetCurrentContext();
      ImGui::UpdatePlatformWindows();
      ImGui::RenderPlatformWindowsDefault();
      SDL_GL_MakeCurrent(backup_current_window, backup_current_context);
    }

    SDL_GL_SwapWindow(window);
  }

  // Cleanup JACK
  jack_client_close(client);

  // Cleanup SDL and ImGui
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplSDL2_Shutdown();
  ImPlot::DestroyContext();
  ImGui::DestroyContext();

  SDL_GL_DeleteContext(gl_context);
  SDL_DestroyWindow(window);
  SDL_Quit();

  return 0;
}

// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!