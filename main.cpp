#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <atomic>
#include <cmath>
#include <cstddef>
#include <memory>
#include <span>
#include <stdio.h>
#include <type_traits>
#include <vector>
#if defined(IMGUI_IMPL_OPENGL_ES2)
#include <GLES2/gl2.h>
#endif
#include "implot.h"
#include <GLFW/glfw3.h>
#include <jack/jack.h>

template <typename T>
class lock_free_circular_buffer {
public:
    explicit lock_free_circular_buffer(std::size_t size)
        : buffer_(size), max_size_(size), head_(0), tail_(0) {
        static_assert(std::is_arithmetic<T>::value, "T must be a numeric type");
    }

    void push(T value) {
        std::size_t head = head_.load(std::memory_order_relaxed);
        std::size_t next = (head + 1) % max_size_;
        if (next != tail_.load(std::memory_order_acquire)) {
            buffer_[head] = value;
            head_.store(next, std::memory_order_release);
        } else {
            // Buffer is full, overwrite the oldest value
            std::size_t tail = tail_.load(std::memory_order_relaxed);
            std::size_t new_tail = (tail + 1) % max_size_;
            tail_.store(new_tail, std::memory_order_release);
            buffer_[head] = value;
            head_.store(next, std::memory_order_release);
        }
    }

    bool pop(T& value) noexcept {
        std::size_t tail = tail_.load(std::memory_order_relaxed);
        if (head_.load(std::memory_order_acquire) == tail) {
            return false; // Buffer empty
        }
        value = buffer_[tail];
        tail_.store((tail + 1) % max_size_, std::memory_order_release);
        return true;
    }

    bool peek(std::size_t index, T& value) const noexcept {
        if (index >= size()) {
            return false; // Index out of bounds
        }
        value = buffer_[(tail_.load(std::memory_order_acquire) + index) % max_size_];
        return true;
    }

    std::size_t size() const noexcept {
        std::size_t head = head_.load(std::memory_order_relaxed);
        std::size_t tail = tail_.load(std::memory_order_relaxed);
        return (head + max_size_ - tail) % max_size_;
    }

    std::size_t capacity() const noexcept { return max_size_; }

    bool is_contiguous() const noexcept {
        return head_.load(std::memory_order_relaxed) >= tail_.load(std::memory_order_relaxed);
    }

    void push_back(const T* first, const T* last) {
        while (first != last) {
            push(*first++);
        }
    }

    std::span<const T> get_span() const {
        std::size_t current_tail = tail_.load(std::memory_order_acquire);
        return std::span<const T>(buffer_.data() + current_tail, size());
    }

private:
    std::vector<T> buffer_;
    std::size_t max_size_;
    std::atomic<std::size_t> head_;
    std::atomic<std::size_t> tail_;
};

constexpr float HISTORY_DURATION_SEC = 5.0F;

struct audio_data {
    lock_free_circular_buffer<float> buffer_;
    std::atomic<uint64_t> total_samples_{0};
    int sample_rate_;

    audio_data(std::size_t size, int sr) : buffer_(size), sample_rate_(sr) {}
};

std::unique_ptr<audio_data> global_audio_data;

jack_client_t* client_;
jack_port_t* input_port_;

int jack_callback(jack_nframes_t nframes, void* arg) {
    auto* audio = static_cast<audio_data*>(arg);
    const auto* in =
        static_cast<const jack_default_audio_sample_t*>(jack_port_get_buffer(input_port_, nframes));

    const size_t available_space = audio->buffer_.capacity() - audio->buffer_.size();
    const size_t frames_to_copy = std::min(static_cast<size_t>(nframes), available_space);

    if (audio->buffer_.is_contiguous() && frames_to_copy == nframes) {
        audio->buffer_.push_back(in, in + frames_to_copy);
    } else {
        for (size_t i = 0; i < frames_to_copy; ++i) {
            audio->buffer_.push(in[i]);
        }
    }

    audio->total_samples_.fetch_add(frames_to_copy, std::memory_order_relaxed);

    return (frames_to_copy < static_cast<size_t>(nframes)) ? 1 : 0;
}

static std::vector<float> x_vals;
static std::vector<float> y_vals;

void show_audio_waveform() {
    if (!global_audio_data) {
        return;
    }

    uint64_t total_samples = global_audio_data->total_samples_.load(std::memory_order_relaxed);
    float current_time = static_cast<float>(total_samples) / global_audio_data->sample_rate_;

    auto buffer_span = global_audio_data->buffer_.get_span();
    std::size_t buffer_size = buffer_span.size();

    if (ImPlot::BeginPlot("Audio Waveform")) {
        ImPlot::SetupAxisLimits(ImAxis_X1, current_time - HISTORY_DURATION_SEC, current_time,
                                ImGuiCond_Always);
        ImPlot::SetupAxisLimits(ImAxis_Y1, -1.0f, 1.0f);

        if (x_vals.size() < buffer_size) {
            x_vals.resize(buffer_size);
            y_vals.resize(buffer_size);
        }

        std::size_t valid_samples = 0;
        for (std::size_t i = 0; i < buffer_size; ++i) {
            float sample;
            if (global_audio_data->buffer_.peek(i, sample)) {
                float sample_time = current_time - (static_cast<float>(buffer_size - i) /
                                                    global_audio_data->sample_rate_);
                x_vals[valid_samples] = sample_time;
                y_vals[valid_samples] = sample;
                ++valid_samples;
            } else {
                break;
            }
        }

        ImPlot::PlotLine("Waveform", x_vals.data(), y_vals.data(), valid_samples);
        ImPlot::EndPlot();
    }
}

static void glfw_error_callback(int error, const char* description) {
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

int main(int, char**) {
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit()) {
        return 1;
    }

#if defined(IMGUI_IMPL_OPENGL_ES2)
    const char* glsl_version = "#version 100";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
#elif defined(__APPLE__)
    const char* glsl_version = "#version 150";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#else
    const char* glsl_version = "#version 150";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    GLFWwindow* window = glfwCreateWindow(1280, 720, "Audio Waveform", nullptr, nullptr);
    if (window == nullptr) {
        return 1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);
    ImPlot::CreateContext();

    const char* client_name = "audio_waveform";
    jack_options_t options = JackNullOption;
    jack_status_t status;
    client_ = jack_client_open(client_name, options, &status, nullptr);
    if (!client_) {
        fprintf(stderr, "Could not create JACK client\n");
        return 1;
    }

    int sample_rate = jack_get_sample_rate(client_);
    std::size_t buffer_size = sample_rate * HISTORY_DURATION_SEC;

    try {
        global_audio_data = std::make_unique<audio_data>(buffer_size, sample_rate);
    } catch (const std::bad_alloc& e) {
        fprintf(stderr, "Failed to allocate AudioData: %s\n", e.what());
        jack_client_close(client_);
        return 1;
    }

    jack_set_process_callback(client_, jack_callback, global_audio_data.get());
    input_port_ = jack_port_register(client_, "input", JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);
    if (!input_port_) {
        fprintf(stderr, "Could not register input port\n");
        jack_client_close(client_);
        return 1;
    }

    if (jack_activate(client_)) {
        fprintf(stderr, "Cannot activate client\n");
        jack_client_close(client_);
        return 1;
    }

    const char** ports =
        jack_get_ports(client_, nullptr, nullptr, JackPortIsPhysical | JackPortIsOutput);
    if (!ports) {
        fprintf(stderr, "No available physical ports\n");
        jack_client_close(client_);
        return 1;
    }

    if (jack_connect(client_, ports[0], jack_port_name(input_port_))) {
        fprintf(stderr, "Cannot connect input port\n");
    }

    jack_free(ports);

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::Begin("Audio Waveform Visualizer");
        show_audio_waveform();
        ImGui::End();

        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.1f, 0.1f, 0.1f, 1.00f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
            GLFWwindow* backup_current_context = glfwGetCurrentContext();
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            glfwMakeContextCurrent(backup_current_context);
        }

        glfwSwapBuffers(window);
    }

    jack_client_close(client_);

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImPlot::DestroyContext();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
