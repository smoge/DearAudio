#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <stdio.h>
#define GL_SILENCE_DEPRECATION
#if defined(IMGUI_IMPL_OPENGL_ES2)
#include <GLES2/gl2.h>
#endif
#include <GLFW/glfw3.h>

#include "implot.h"

#include <atomic>
#include <cmath>
#include <jack/jack.h>
#include <stdio.h>
#include <vector>

template <typename T> class LockFreeCircularBuffer {
public:
  explicit LockFreeCircularBuffer(size_t size)
      : buffer(size), max_size(size), head(0), tail(0) {}

  // Push to buffer, overwriting old data if full
  void push(T value) {
    size_t next = (head + 1) % max_size;
    if (next != tail.load()) {
      buffer[head] = value;
      head = next;
    } else {
      // Overwrite the oldest value if the buffer is full
      tail.store((tail + 1) % max_size);
      buffer[head] = value;
      head = next;
    }
  }

  // Pop from buffer, return false if empty
  bool pop(T &value) {
    if (head == tail.load()) {
      return false; // Buffer empty
    }
    value = buffer[tail];
    tail.store((tail + 1) % max_size);
    return true;
  }

  // Peek at buffer, return value at position without consuming it
  bool peek(size_t index, T &value) const {
    if (index >= size()) {
      return false; // Index out of bounds
    }
    value = buffer[(tail + index) % max_size];
    return true;
  }

  size_t size() const { return (head + max_size - tail.load()) % max_size; }

private:
  std::vector<T> buffer;
  size_t max_size;
  size_t head;
  std::atomic<size_t> tail;
};

struct AudioData {
  LockFreeCircularBuffer<float> buffer; // Lock-free circular buffer
  explicit AudioData(size_t size) : buffer(size) {}
};

AudioData audioData(44100 *
                    10); // Buffer size for 10 seconds of audio at 44100hz
jack_client_t *client;
jack_port_t *input_port;

// Process callback for JACK (real-time thread)
int jackCallback(jack_nframes_t nframes, void *arg) {
  jack_default_audio_sample_t *in =
      (jack_default_audio_sample_t *)jack_port_get_buffer(input_port, nframes);

  for (jack_nframes_t i = 0; i < nframes; ++i) {
    audioData.buffer.push(in[i]); // Lock-free push
  }

  return 0;
}

void ShowAudioWaveform() {
  static float t = ImGui::GetTime();
  static float history = 5.0f; //  5 seconds / history

  if (ImPlot::BeginPlot("Audio Waveform")) {
    ImPlot::SetupAxisLimits(ImAxis_X1, t - history, t,
                            ImGuiCond_Always);       // X-axis: Time
    ImPlot::SetupAxisLimits(ImAxis_Y1, -1.0f, 1.0f); // Y-axis: Amplitude

    std::vector<float> x, y;
    float sample;
    size_t buffer_size = audioData.buffer.size(); // Check the buffer size

    // Read data from the buffer using peek()
    for (size_t i = 0; i < buffer_size; ++i) {
      if (audioData.buffer.peek(i, sample)) { // Safely peek data
        float time = t - ((buffer_size - i) /
                          44100.0f); // Calculate time for each sample
        x.push_back(time);           // X values are the timestamps
        y.push_back(sample);         // Y values are the waveform samples
      }
    }

    // Plot  waveform
    ImPlot::PlotLine("Waveform", x.data(), y.data(), x.size());
    ImPlot::EndPlot();
  }

  t = ImGui::GetTime(); // Update time
}

// GLFW error callback
static void glfw_error_callback(int error, const char *description) {
  fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

int main(int, char **) {
  // Setup GLFW
  glfwSetErrorCallback(glfw_error_callback);
  if (!glfwInit())
    return 1;

    // Setup OpenGL
// Decide GL+GLSL versions
#if defined(IMGUI_IMPL_OPENGL_ES2)
  // GL ES 2.0 + GLSL 100
  const char *glsl_version = "#version 100";
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
  glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
#elif defined(__APPLE__)
  // GL 3.2 + GLSL 150
  const char *glsl_version = "#version 150";
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE); // 3.2+ only
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // Required on Mac
#else
  // GL 3.2 + GLSL 150 for cross-platform (Linux/Windows)
  const char *glsl_version = "#version 150";
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3); // Set to 3.2
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
  glfwWindowHint(
      GLFW_OPENGL_PROFILE,
      GLFW_OPENGL_CORE_PROFILE); // Ensure it's using the core profile
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // Mac compatibility
#endif

  // Create window
  GLFWwindow *window =
      glfwCreateWindow(1280, 720, "Audio Waveform", nullptr, nullptr);
  if (window == nullptr)
    return 1;
  glfwMakeContextCurrent(window);
  glfwSwapInterval(1); // Enable vsync

  // Setup ImGui context
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO &io = ImGui::GetIO();
  (void)io;
  io.ConfigFlags |=
      ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
  io.ConfigFlags |=
      ImGuiConfigFlags_NavEnableGamepad;              // Enable Gamepad Controls
  io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;   // Enable Docking
  io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable; // Enable Multi-Viewport /
                                                      // Platform Windows

  // Setup Dear ImGui style
  ImGui::StyleColorsDark();

  // Setup Platform/Renderer backends
  ImGui_ImplGlfw_InitForOpenGL(window, true);
  ImGui_ImplOpenGL3_Init(glsl_version);

  // Setup ImPlot context
  ImPlot::CreateContext();

  // Initialize JACK
  const char *client_name = "audio_waveform";
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

  // Main loop
  while (!glfwWindowShouldClose(window)) {
    glfwPollEvents();

    // Start the ImGui frame
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    // Show the audio waveform
    ImGui::Begin("Audio Waveform Visualizer");
    ShowAudioWaveform();
    ImGui::End();

    // Rendering
    ImGui::Render();
    int display_w, display_h;
    glfwGetFramebufferSize(window, &display_w, &display_h);
    glViewport(0, 0, display_w, display_h);
    glClearColor(0.1f, 0.1f, 0.1f, 1.00f);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    // Update and Render additional Platform Windows
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
      GLFWwindow *backup_current_context = glfwGetCurrentContext();
      ImGui::UpdatePlatformWindows();
      ImGui::RenderPlatformWindowsDefault();
      glfwMakeContextCurrent(backup_current_context);
    }

    glfwSwapBuffers(window);
  }

  // Cleanup JACK
  jack_client_close(client);

  // Cleanup ImGui
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImPlot::DestroyContext();
  ImGui::DestroyContext();

  glfwDestroyWindow(window);
  glfwTerminate();

  return 0;
}
