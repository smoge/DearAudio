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
#include <cstddef>
#include <jack/jack.h>
#include <span>
#include <stdio.h>
#include <type_traits>
#include <vector>

template <typename T>
class lock_free_circular_buffer
{
public:
    explicit lock_free_circular_buffer(std::size_t size)
        : buffer(size), max_size(size), head(0), tail(0)
    {
        static_assert(std::is_arithmetic<T>::value, "T must be a numeric type");
    }

    // Push to buffer, overwriting old data if full
    void push(T value)
    {
        std::size_t next = (head + 1) % max_size;
        if (next != tail.load()) {
            buffer[head] = value;
            head = next;
        } else {
            // Overwrite the oldest value if the buffer is full
            tail.store((tail.load() + 1) % max_size);
            buffer[head] = value;
            head = next;
        }
    }

    // Pop from buffer, return false if empty
    bool pop(T& value)
    {
        if (head == tail.load()) {
            return false; // Buffer empty
        }
        value = buffer[tail];
        tail.store((tail.load() + 1) % max_size);
        return true;
    }

    // Peek at buffer, return value at position without consuming it
    bool peek(std::size_t index, T& value) const
    {
        if (index >= size()) {
            return false; // Index out of bounds
        }
        value = buffer[(tail.load() + index) % max_size];
        return true;
    }

    std::size_t size() const
    {
        return (head + max_size - tail.load()) % max_size;
    }

    std::span<const T> get_span() const
    {
        std::size_t current_tail = tail.load();
        return std::span<const T>(buffer.data() + current_tail, size());
    }

private:
    std::vector<T> buffer;
    std::size_t max_size;
    std::size_t head;
    std::atomic<std::size_t> tail;
};

struct AudioData {
    lock_free_circular_buffer<float> buffer; // Lock-free circular buffer
    explicit AudioData(std::size_t size) : buffer(size) {}
};

AudioData audio_data(44100 * 10); // Buffer size for 10 seconds of audio at 44100hz
jack_client_t* client;
jack_port_t* input_port;

// Process callback for JACK (real-time thread)
int jack_callback(jack_nframes_t nframes, void* arg)
{
    jack_default_audio_sample_t* in =
        (jack_default_audio_sample_t*)jack_port_get_buffer(input_port, nframes);

    for (jack_nframes_t i = 0; i < nframes; ++i) {
        audio_data.buffer.push(in[i]); // Lock-free push
    }

    return 0;
}

// Declare these variables outside the function
static std::vector<float> x;
static std::vector<float> y;
static float time_offset = 0.0f; // Keeps track of the global time offset

void show_audio_waveform()
{
    // Instead of using ImGui::GetTime(), maintain a consistent time based on the
    // sample rate
    static float history = 5.0f; // 5 seconds of history

    // Increment time offset by the time difference based on the audio buffer
    std::size_t buffer_size = audio_data.buffer.size();
    float time_increment = buffer_size / 44100.0f;
    time_offset += time_increment;

    if (ImPlot::BeginPlot("Audio Waveform")) {
        ImPlot::SetupAxisLimits(ImAxis_X1, time_offset - history, time_offset, ImGuiCond_Always);
        ImPlot::SetupAxisLimits(ImAxis_Y1, -1.0f, 1.0f);

        // Resize the vectors if necessary
        x.resize(buffer_size);
        y.resize(buffer_size);

        float sample;
        for (std::size_t i = 0; i < buffer_size; ++i) {
            if (audio_data.buffer.peek(i, sample)) {
                // Align the time with the buffer and the sample rate
                float time = time_offset - ((buffer_size - i) / 44100.0f);
                x[i] = time;
                y[i] = sample;
            }
        }

        // Plot the waveform
        ImPlot::PlotLine("Waveform", x.data(), y.data(), x.size());
        ImPlot::EndPlot();
    }
}

// GLFW error callback
static void glfw_error_callback(int error, const char* description)
{
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

int main(int, char**)
{
    // Setup GLFW
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit())
        return 1;

        // Setup OpenGL
// Decide GL+GLSL versions
#if defined(IMGUI_IMPL_OPENGL_ES2)
    // GL ES 2.0 + GLSL 100
    const char* glsl_version = "#version 100";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
#elif defined(__APPLE__)
    // GL 3.2 + GLSL 150
    const char* glsl_version = "#version 150";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE); // 3.2+ only
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);           // Required on Mac
#else
    // GL 3.2 + GLSL 150 for cross-platform (Linux/Windows)
    const char* glsl_version = "#version 150";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3); // Set to 3.2
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE,
                   GLFW_OPENGL_CORE_PROFILE);            // Ensure it's using the core profile
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // Mac compatibility
#endif

    // Create window
    GLFWwindow* window = glfwCreateWindow(1280, 720, "Audio Waveform", nullptr, nullptr);
    if (window == nullptr)
        return 1;
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable vsync

    // Setup ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;  // Enable Gamepad Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;     // Enable Docking
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;   // Enable Multi-Viewport /
                                                          // Platform Windows

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    // Setup ImPlot context
    ImPlot::CreateContext();

    // Initialize JACK
    const char* client_name = "audio_waveform";
    jack_options_t options = JackNullOption;
    client = jack_client_open(client_name, options, nullptr);
    if (!client) {
        fprintf(stderr, "Could not create JACK client\n");
        return 1;
    }

    jack_set_process_callback(client, jack_callback, &audio_data);
    input_port = jack_port_register(client, "input", JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);
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

    const char** ports =
        jack_get_ports(client, nullptr, nullptr, JackPortIsPhysical | JackPortIsOutput);
    if (!ports) {
        fprintf(stderr, "No available physical ports\n");
        jack_client_close(client);
        return 1;
    }

    if (jack_connect(client, ports[0], jack_port_name(input_port))) {
        fprintf(stderr, "Cannot connect input port\n");
    }

    jack_free(ports);

    // Main loop
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        // Start the ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Show the audio waveform
        ImGui::Begin("Audio Waveform Visualizer");
        show_audio_waveform();
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
            GLFWwindow* backup_current_context = glfwGetCurrentContext();
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