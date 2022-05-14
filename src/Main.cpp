#include <iostream>
#include <vector>

#include <glad/gl.h>

#include <glm/glm.hpp>
#include <glm/ext.hpp>
#include <glm/gtx/string_cast.hpp>

#include <Windows.h>

#define GLUE_(x, y) x##y
#define GLUE(x, y)  GLUE_(x, y)

#define defer(x)                                              \
    [[maybe_unused]] auto GLUE(_defer, __COUNTER__) = [&]() { \
        auto lambda = [&]() -> void {                         \
            x;                                                \
        };                                                    \
        class Defer {                                         \
        public:                                               \
            decltype(lambda) func;                            \
            Defer(decltype(lambda) func) : func(func) {}      \
            ~Defer() { func(); }                              \
        };                                                    \
        return Defer{ lambda };                               \
    }()

enum struct MouseButton {
    Left,
    Middle,
    Right,
};

struct Circle {
    glm::vec2 Position;
    glm::vec2 PrevPosition;
    float Radius;
    float Mass;
    glm::vec3 Color;
    bool HasPhysics = true;
};

class GameState {
public:
    bool Running = true;

    void Init() {
        GLuint vertexArray;
        glGenVertexArrays(1, &vertexArray);
        glBindVertexArray(vertexArray);

        CircleShader = CreateShaderProgram(CircleVertexSource, CircleFragmentSource);

        // Background
        Circles.emplace_back(Circle{
            .Position     = { 0.0f, 0.0f },
            .PrevPosition = { 0.0f, 0.0f },
            .Radius       = 1.0f,
            .Color        = { 0.4f, 0.4f, 0.4f },
            .HasPhysics   = false,
        });

        for (std::size_t i = 0; i < 50; i++) {
            auto randFloat = []() {
                return static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
            };
            Circle circle{};
            circle.Radius       = randFloat() * 0.1f + 0.01f;
            circle.Mass         = glm::pi<float>() * circle.Radius * circle.Radius;
            circle.Position     = { randFloat() - 0.5f, randFloat() - 0.5f },
            circle.PrevPosition = circle.Position - glm::vec2{ randFloat() - 0.5f, randFloat() - 0.5f } * 0.02f;
            circle.Color        = { randFloat(), randFloat(), randFloat() };
            Circles.emplace_back(circle);
        }
    }

    void DeInit() {
        glDeleteProgram(CircleShader);
    }

    void Update(float dt) {
        time += dt;
        constexpr float FixedUpdateTime = 1.0f / 60.0f;
        constexpr float Gravity         = 0.1f;
        while (time >= FixedUpdateTime) {
            for (std::size_t i = 0; i < Circles.size(); i++) {
                Circle& circle = Circles[i];
                if (!circle.HasPhysics)
                    continue;

                glm::vec2 velocity  = circle.Position - circle.PrevPosition;
                circle.PrevPosition = circle.Position;
                circle.Position += velocity;

                // Gravity
                circle.Position.y -= Gravity * FixedUpdateTime;
            }

            constexpr std::size_t ConstraintIterations = 8;
            for (std::size_t constraintIteration = 0; constraintIteration < ConstraintIterations; constraintIteration++) {
                if (SelectedCircle != nullptr) {
                    SelectedCircle->Position = GetMouseWorldPos() + SelectedCircleOffset;
                }

                for (std::size_t i = 0; i < Circles.size(); i++) {
                    Circle& circleA = Circles[i];
                    if (!circleA.HasPhysics)
                        continue;

                    // Constraint
                    constexpr float ConstraintRadius = 1.0f;
                    if (float length = glm::length(circleA.Position); length >= ConstraintRadius - circleA.Radius) {
                        circleA.Position /= length + circleA.Radius;
                    }

                    for (std::size_t j = i + 1; j < Circles.size(); j++) {
                        Circle& circleB = Circles[j];
                        if (!circleA.HasPhysics)
                            continue;

                        float minimumDistance = circleA.Radius + circleB.Radius;
                        if (float distance = glm::length(circleB.Position - circleA.Position); distance < minimumDistance) {
                            glm::vec2 aToB = glm::normalize(circleB.Position - circleA.Position);
                            if (circleA.Mass >= circleB.Mass) {
                                float ratio = circleB.Mass / circleA.Mass;
                                circleA.Position -= aToB * (minimumDistance - distance) * (0.0f + ratio * 0.5f);
                                circleB.Position += aToB * (minimumDistance - distance) * (1.0f - ratio * 0.5f);
                            } else {
                                float ratio = circleA.Mass / circleB.Mass;
                                circleA.Position -= aToB * (minimumDistance - distance) * (1.0f - ratio * 0.5f);
                                circleB.Position += aToB * (minimumDistance - distance) * (0.0f + ratio * 0.5f);
                            }
                        }
                    }
                }
            }

            time -= FixedUpdateTime;
        }
    }

    void Render() {
        glClearColor(0.1f, 0.1f, 0.1f, 1.0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

        glm::mat4 viewMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(CameraPosition, 0.0f));

        glUseProgram(CircleShader);
        glProgramUniformMatrix4fv(CircleShader, ProjectionMatrixLocation, 1, GL_FALSE, glm::value_ptr(ProjectionMatrix));
        glProgramUniformMatrix4fv(CircleShader, ViewMatrixLocation, 1, GL_FALSE, glm::value_ptr(viewMatrix));
        for (const auto& circle : Circles) {
            glm::mat4 modelMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(circle.Position, 0.0f));
            modelMatrix           = glm::scale(modelMatrix, glm::vec3(circle.Radius, circle.Radius, 0.0f));
            glProgramUniformMatrix4fv(CircleShader, ModelMatrixLocation, 1, GL_FALSE, glm::value_ptr(modelMatrix));
            glProgramUniform4f(CircleShader, ColorLocation, circle.Color.r, circle.Color.g, circle.Color.b, 1.0f);
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        }
    }

    void OnWindowResize(std::size_t width, std::size_t height) {
        Width  = width;
        Height = height;
        glViewport(0, 0, width, height);
        RecalculateProjectionMatrix();
    }

    glm::vec2 GetMouseWorldPos() {
        glm::vec2 screenPos{ MouseX, Height - MouseY };
        glm::mat4 viewProjection =
            glm::inverse(ProjectionMatrix * glm::translate(glm::mat4(1.0f), glm::vec3(CameraPosition, 0.0f)));
        return glm::vec2(viewProjection * glm::vec4(screenPos / glm::vec2{ Width, Height } * 2.0f - 1.0f, 0.0f, 1.0f));
    }

    void OnMouseButton(MouseButton button, bool pressed) {
        if (button == MouseButton::Left) {
            if (pressed) {
                for (Circle& circle : Circles) {
                    if (!circle.HasPhysics)
                        continue;
                    glm::vec2 difference = circle.Position - GetMouseWorldPos();
                    if (glm::length(difference) <= circle.Radius) {
                        SelectedCircle       = &circle;
                        SelectedCircleOffset = difference;
                        break;
                    }
                }
            } else {
                SelectedCircle = nullptr;
            }
        }
    }

    void OnMouseMove(std::size_t mouseX, std::size_t mouseY) {
        MouseX = mouseX;
        MouseY = mouseY;
    }
private:
    std::size_t Width, Height;
    float time = 0.0f;
    std::size_t MouseX, MouseY;
    glm::mat4 ProjectionMatrix;
    glm::vec2 CameraPosition;
    float CameraScale = 1;
    std::vector<Circle> Circles;
    GLuint CircleShader;
    Circle* SelectedCircle = nullptr;
    glm::vec2 SelectedCircleOffset;

    void RecalculateProjectionMatrix() {
        float aspect     = static_cast<float>(Width) / static_cast<float>(Height);
        ProjectionMatrix = glm::orthoLH(-aspect * CameraScale, aspect * CameraScale, -CameraScale, CameraScale, -1.0f, 1.0f);
    }

    static GLuint CreateShaderProgram(const char* vertexSource, const char* fragmentSource) {
        auto createShader = [](const char* source, GLenum type) -> GLuint {
            GLuint shader = glCreateShader(type);
            glShaderSource(shader, 1, &source, nullptr);
            glCompileShader(shader);
            GLint compiled = GL_FALSE;
            glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
            if (compiled == GL_FALSE) {
                GLint logSize = 0;
                glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logSize);
                std::unique_ptr<char[]> log = std::make_unique<char[]>(logSize);
                glGetShaderInfoLog(shader, logSize, nullptr, log.get());
                std::cerr << log.get() << std::endl;
            }
            return shader;
        };

        GLuint vertexShader = createShader(vertexSource, GL_VERTEX_SHADER);
        defer(glDeleteShader(vertexShader));
        GLuint fragmentShader = createShader(fragmentSource, GL_FRAGMENT_SHADER);
        defer(glDeleteShader(fragmentShader));

        GLuint shader = glCreateProgram();
        glAttachShader(shader, vertexShader);
        glAttachShader(shader, fragmentShader);
        glLinkProgram(shader);

        GLint linked = GL_FALSE;
        glGetProgramiv(shader, GL_LINK_STATUS, &linked);
        if (linked == GL_FALSE) {
            GLint logSize = 0;
            glGetProgramiv(shader, GL_INFO_LOG_LENGTH, &logSize);
            std::unique_ptr<char[]> log = std::make_unique<char[]>(logSize);
            glGetProgramInfoLog(shader, logSize, nullptr, log.get());
            std::cerr << log.get() << std::endl;
        }

        glDetachShader(shader, vertexShader);
        glDetachShader(shader, fragmentShader);
        return shader;
    }

    static constexpr GLint ProjectionMatrixLocation   = 0;
    static constexpr GLint ViewMatrixLocation         = 1;
    static constexpr GLint ModelMatrixLocation        = 2;
    static constexpr GLint ColorLocation              = 3;
    static constexpr const char* CircleVertexSource   = R"###(
#version 440 core

layout(location = 0) uniform mat4 u_ProjectionMatrix;
layout(location = 1) uniform mat4 u_ViewMatrix;
layout(location = 2) uniform mat4 u_ModelMatrix;

layout(location = 0) out vec2 v_UV;

void main() {
    vec2 uv = vec2(
        (gl_VertexID >> 0) & 1,
        (gl_VertexID >> 1) & 1
    );
    v_UV = uv;
    gl_Position = u_ProjectionMatrix * u_ViewMatrix * u_ModelMatrix * vec4(uv * 2.0 - 1.0, 0.0, 1.0);
}
)###";
    static constexpr const char* CircleFragmentSource = R"###(
#version 440 core

layout(location = 0) out vec4 o_Color;

layout(location = 0) in vec2 v_UV;

layout(location = 3) uniform vec4 u_Color;

void main() {
    vec2 pos = v_UV * 2.0 - 1.0;
    if (dot(pos, pos) > 1.0) {
        discard;
    }
    o_Color = u_Color;
}
)###";
};

int main(int, char**) {
    constexpr const char* WindowClassName     = "VerletPhysicsTest";
    constexpr const char* WindowTitle         = "Verlet Physics";
    constexpr DWORD WindowStyle               = WS_OVERLAPPEDWINDOW;
    constexpr DWORD WindowStyleEx             = 0;
    constexpr std::size_t InitialWindowWidth  = 640;
    constexpr std::size_t InitialWindowHeight = 480;

    GameState state{};

    HINSTANCE instance = GetModuleHandleA(nullptr);
    WNDCLASSEXA windowClass{
        .cbSize      = sizeof(windowClass),
        .style       = CS_OWNDC,
        .lpfnWndProc = static_cast<WNDPROC>([](HWND window, UINT message, WPARAM wParam, LPARAM lParam) -> LRESULT {
            if (message == WM_NCCREATE) {
                CREATESTRUCTA* createStruct = reinterpret_cast<CREATESTRUCTA*>(lParam);
                SetWindowLongPtrA(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(createStruct->lpCreateParams));
                return DefWindowProcA(window, message, wParam, lParam);
            }
            GameState* state = reinterpret_cast<GameState*>(GetWindowLongPtrA(window, GWLP_USERDATA));
            if (state == nullptr) {
                return DefWindowProcA(window, message, wParam, lParam);
            }
            LRESULT result = 0;
            switch (message) {
                case WM_QUIT:
                case WM_CLOSE: {
                    state->Running = false;
                } break;

                case WM_SIZE: {
                    RECT windowRect;
                    GetClientRect(window, &windowRect);
                    DWORD width  = windowRect.right - windowRect.left;
                    DWORD height = windowRect.bottom - windowRect.top;
                    if (width > 0 && height > 0) {
                        state->OnWindowResize(static_cast<std::size_t>(width), static_cast<std::size_t>(height));
                    }
                } break;

                case WM_LBUTTONDOWN:
                case WM_MBUTTONDOWN:
                case WM_RBUTTONDOWN:
                case WM_LBUTTONUP:
                case WM_MBUTTONUP:
                case WM_RBUTTONUP: {
                    bool pressed = message == WM_LBUTTONDOWN || message == WM_MBUTTONDOWN || message == WM_RBUTTONDOWN;
                    MouseButton button;
                    switch (message) {
                        case WM_LBUTTONDOWN:
                        case WM_LBUTTONUP: {
                            button = MouseButton::Left;
                        } break;

                        case WM_MBUTTONDOWN:
                        case WM_MBUTTONUP: {
                            button = MouseButton::Middle;
                        } break;

                        case WM_RBUTTONDOWN:
                        case WM_RBUTTONUP: {
                            button = MouseButton::Right;
                        } break;
                    }
                    state->OnMouseButton(button, pressed);
                } break;

                case WM_MOUSEMOVE: {
                    POINTS point = MAKEPOINTS(lParam);
                    state->OnMouseMove(point.x, point.y);
                } break;

                default: {
                    result = DefWindowProcA(window, message, wParam, lParam);
                } break;
            }
            return result;
        }),
        .cbClsExtra    = 0,
        .cbWndExtra    = 0,
        .hInstance     = instance,
        .hIcon         = nullptr,
        .hCursor       = LoadCursor(nullptr, IDC_ARROW),
        .hbrBackground = nullptr,
        .lpszMenuName  = nullptr,
        .lpszClassName = WindowClassName,
        .hIconSm       = nullptr,
    };
    if (auto windowClassAtom = RegisterClassExA(&windowClass); windowClassAtom == 0) {
        std::cerr << "RegisterClassExA: " << GetLastError() << std::endl;
        return 1;
    }
    defer(UnregisterClassA(WindowClassName, instance));

    RECT windowRect   = {};
    windowRect.left   = 100;
    windowRect.right  = windowRect.left + static_cast<DWORD>(InitialWindowWidth);
    windowRect.top    = 100;
    windowRect.bottom = windowRect.top + static_cast<DWORD>(InitialWindowHeight);
    if (auto adjustWindowRectResult = AdjustWindowRectEx(&windowRect, WindowStyle, FALSE, WindowStyleEx);
        adjustWindowRectResult == FALSE) {
        std::cerr << "AdjustWindowRectEx: " << GetLastError() << std::endl;
        return 1;
    }

    HWND window = CreateWindowExA(WindowStyleEx,
                                  WindowClassName,
                                  WindowTitle,
                                  WindowStyle,
                                  CW_USEDEFAULT,
                                  CW_USEDEFAULT,
                                  windowRect.right - windowRect.left,
                                  windowRect.bottom - windowRect.top,
                                  nullptr,
                                  nullptr,
                                  instance,
                                  &state);
    defer(DestroyWindow(window));

    HDC dc = GetDC(window);

    PIXELFORMATDESCRIPTOR pixelFormatDescriptor{
        .nSize           = sizeof(pixelFormatDescriptor),
        .nVersion        = 1,
        .dwFlags         = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
        .iPixelType      = PFD_TYPE_RGBA,
        .cColorBits      = 32,
        .cRedBits        = 0,
        .cRedShift       = 0,
        .cGreenBits      = 0,
        .cGreenShift     = 0,
        .cBlueBits       = 0,
        .cBlueShift      = 0,
        .cAlphaBits      = 0,
        .cAlphaShift     = 0,
        .cAccumBits      = 0,
        .cAccumRedBits   = 0,
        .cAccumGreenBits = 0,
        .cAccumBlueBits  = 0,
        .cAccumAlphaBits = 0,
        .cDepthBits      = 24,
        .cStencilBits    = 8,
        .cAuxBuffers     = 0,
        .iLayerType      = PFD_MAIN_PLANE,
        .bReserved       = 0,
        .dwLayerMask     = 0,
        .dwVisibleMask   = 0,
        .dwDamageMask    = 0,
    };

    int format = ChoosePixelFormat(dc, &pixelFormatDescriptor);
    if (format == 0) {
        std::cerr << "ChoosePixelFormat: " << GetLastError() << std::endl;
        return 1;
    }

    if (auto pixelFormatResult = SetPixelFormat(dc, format, &pixelFormatDescriptor); pixelFormatResult == FALSE) {
        std::cerr << "SetPixelFormat: " << GetLastError() << std::endl;
        return 1;
    }

    HGLRC tempContext = wglCreateContext(dc);
    if (tempContext == nullptr) {
        std::cerr << "wglCreateContext: " << GetLastError() << std::endl;
        return 1;
    }

    if (auto makeContextCurrentResult = wglMakeCurrent(dc, tempContext); makeContextCurrentResult == FALSE) {
        std::cerr << "wglMakeCurrent: " << GetLastError() << std::endl;
        return 1;
    }

    HGLRC(*wglCreateContextAttribsARB)
    (HDC hDC, HGLRC hshareContext, const int* attribList) =
        reinterpret_cast<decltype(wglCreateContextAttribsARB)>(wglGetProcAddress("wglCreateContextAttribsARB"));

    const int attribList[] = {
        0x2091, // WGL_CONTEXT_MAJOR_VERSION_ARB,
        4,
        0x2092, // WGL_CONTEXT_MINOR_VERSION_ARB,
        5,
        0x9126, // WGL_CONTEXT_PROFILE_MASK_ARB,
        0x00000001, 0,
    };
    HGLRC context = wglCreateContextAttribsARB(dc, nullptr, attribList);
    if (context == nullptr) {
        std::cerr << "wglCreateContextAttribsARB: " << GetLastError() << std::endl;
        return 1;
    }
    defer({
        wglMakeCurrent(nullptr, nullptr);
        wglDeleteContext(context);
    });
    if (auto makeContextCurrentResult = wglMakeCurrent(dc, context); makeContextCurrentResult == FALSE) {
        std::cerr << "wglMakeCurrent: " << GetLastError() << std::endl;
        return 1;
    }
    wglDeleteContext(tempContext);
    tempContext = nullptr;

    HMODULE glLibrary = LoadLibraryA("OpenGL32.dll");
    defer(FreeLibrary(glLibrary));
    if (auto gladLoadResult = gladLoadGLUserPtr(
            [](void*userPtr, const char*name) -> GLADapiproc {
                GLADapiproc proc = reinterpret_cast<GLADapiproc>(wglGetProcAddress(name));
                if (proc == nullptr) {
                    proc = reinterpret_cast<GLADapiproc>(GetProcAddress(reinterpret_cast<HMODULE>(userPtr), name));
                }
                return proc;
            },
            glLibrary);
        gladLoadResult == 0) {
        std::cerr << "gladLoadGL failed" << std::endl;
        return 1;
    }

    LARGE_INTEGER frequency;
    QueryPerformanceFrequency(&frequency);
    LARGE_INTEGER lastTime;
    QueryPerformanceCounter(&lastTime);

    state.Init();
    ShowWindow(window, SW_SHOW);
    while (state.Running) {
        MSG message;
        while (PeekMessageA(&message, window, 0, 0, PM_REMOVE) != 0) {
            TranslateMessage(&message);
            DispatchMessageA(&message);
        }

        LARGE_INTEGER time;
        QueryPerformanceCounter(&time);
        float deltaTime =
            static_cast<float>(static_cast<double>(time.QuadPart - lastTime.QuadPart) / static_cast<double>(frequency.QuadPart));
        lastTime = time;

        state.Update(deltaTime);
        state.Render();

        SwapBuffers(dc);
    }
    ShowWindow(window, SW_HIDE);
    state.DeInit();

    return 0;
}
