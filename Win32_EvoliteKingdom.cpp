#include <windows.h>
#include <cstdint>
#include <xinput.h>

#define internal static
#define persistant static
#define global static

typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

struct Win32_OffscreenBuffer {
    BITMAPINFO info;
    void *memory;
    int width;
    int height;
    int pitch;
};

struct Win32_WindowDimension {
    int width;
    int height;
};

// XInputGetState
#define X_INPUT_GET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_STATE* pState)
typedef X_INPUT_GET_STATE(x_input_get_state);
X_INPUT_GET_STATE(XInputGetStateStub) { return ERROR_DEVICE_NOT_CONNECTED; }
global x_input_get_state* XInputGetState_ = XInputGetStateStub;
#define XInputGetState XInputGetState_

// XInputSetState
#define X_INPUT_SET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_VIBRATION* pVibration)
typedef X_INPUT_SET_STATE(x_input_set_state);
X_INPUT_SET_STATE(XInputSetStateStub) { return ERROR_DEVICE_NOT_CONNECTED; }
global x_input_set_state* XInputSetState_ = XInputSetStateStub;
#define XInputSetState XInputSetState_

internal void Win32LoadXInput(void) {
    HMODULE xInputLibrary = LoadLibrary("xinput1_3.dll");
    if(xInputLibrary == nullptr) {
        // Todo(Sora): Library could not be found
        return;
    }

    XInputGetState = (x_input_get_state*)GetProcAddress(xInputLibrary, "XInputGetState");
    if(XInputGetState == nullptr) { XInputGetState = XInputGetStateStub; }
    XInputSetState = (x_input_set_state*)GetProcAddress(xInputLibrary, "XInputSetState");
    if (XInputSetState == nullptr) { XInputSetState = XInputSetStateStub; }
}

global bool g_running;
global Win32_OffscreenBuffer g_backbuffer;

Win32_WindowDimension Win32GetWindowDimension(HWND window) {
    Win32_WindowDimension res;

    RECT clientRect;

    GetClientRect(window, &clientRect);
    res.width = clientRect.right - clientRect.left;
    res.height = clientRect.bottom - clientRect.top;

    return res;
}

internal void RenderWeirdGradient(Win32_OffscreenBuffer buffer, int blueOffset, int greenOffset) {
    uint8 *row = (uint8 *) buffer.memory;

    for (int y = 0; y < buffer.height; y++) {
        uint32 *pixel = (uint32 *) row;
        for (int x = 0; x < buffer.width; x++) {
            uint8 blue = (x + blueOffset);
            uint8 green = (y + greenOffset);

            *pixel++ = (green << 8) | blue;
        }
        row += buffer.pitch;
    }
}

internal void Win32ResizeDIBSection(Win32_OffscreenBuffer *buffer, int width, int height) {
    if (buffer->memory != nullptr)
        VirtualFree(buffer->memory, 0, MEM_RELEASE);

    buffer->width = width;
    buffer->height = height;

    buffer->info.bmiHeader.biSize = sizeof(buffer->info.bmiHeader);
    buffer->info.bmiHeader.biWidth = width;
    buffer->info.bmiHeader.biHeight = height;
    buffer->info.bmiHeader.biPlanes = 1;
    buffer->info.bmiHeader.biBitCount = 32;
    buffer->info.bmiHeader.biCompression = BI_RGB;

    int bytesPerPixel = 4;
    int bitmapMemorySize = (buffer->width * buffer->height) * bytesPerPixel;
    buffer->memory = VirtualAlloc(0, bitmapMemorySize, MEM_COMMIT, PAGE_READWRITE);
    buffer->pitch = width * bytesPerPixel;

}

internal void
Win32DisplayBufferInWindow(HDC deviceContext, int windowWidth, int windowHeight, Win32_OffscreenBuffer buffer) {
    StretchDIBits(deviceContext,
                  0, 0, windowWidth, windowHeight,
                  0, 0, buffer.width, buffer.height,
                  buffer.memory, &buffer.info,
                  DIB_RGB_COLORS, SRCCOPY);
}

LRESULT CALLBACK MainWindowCallback(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    LRESULT result = 0;

    switch (message) {
        case WM_CLOSE : {
            g_running = false;
        }
            break;

        case WM_DESTROY : {
            g_running = false;
        }
            break;

        case WM_PAINT : {
            PAINTSTRUCT paint;
            HDC deviceContext = BeginPaint(window, &paint);
            Win32_WindowDimension dimension = Win32GetWindowDimension(window);
            Win32DisplayBufferInWindow(deviceContext, dimension.width, dimension.height, g_backbuffer);
            EndPaint(window, &paint);
        }
            break;

        case WM_SYSKEYDOWN :
        case WM_SYSKEYUP :
        case WM_KEYDOWN :
        case WM_KEYUP : {
            uint32 vkCode = wParam;
            bool wasDown = ((lParam & (1 << 30)) != 0);
            bool isDown = ((lParam & (1 << 31)) == 0);
            if(wasDown != isDown) {
                if(vkCode == VK_ESCAPE)
                    g_running = false;
            }
        } break;


        default: {
            result = DefWindowProc(window, message, wParam, lParam);
        }
            break;
    }
    return result;
}

int CALLBACK WinMain(HINSTANCE instance, HINSTANCE prevInstance, LPSTR commandLine, int showCode) {

    Win32LoadXInput();

    HRESULT result;

    WNDCLASS windowClass = {};

    Win32ResizeDIBSection(&g_backbuffer, 1280, 720);

    windowClass.lpfnWndProc = MainWindowCallback;
    windowClass.hInstance = instance;
    windowClass.lpszClassName = "EvoliteKingdomWindowClass";

    result = RegisterClass(&windowClass);
    if (result == NULL) {
        // Todo(Sora): Failed to register window class
        return -1;
    }

    HWND window = CreateWindowEx(
            NULL,
            windowClass.lpszClassName,
            "Evolite Kingdom",
            WS_OVERLAPPEDWINDOW | WS_VISIBLE,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            nullptr,
            nullptr,
            instance,
            nullptr
    );

    if (window == nullptr) {
        // Todo(Sora): Failed to create window
        return -1;
    }

    HDC deviceContext = GetDC(window);

    int xOffset = 0;
    int yOffset = 0;

    g_running = true;

    while (g_running) {
        MSG message;
        while (PeekMessage(&message, 0, 0, 0, PM_REMOVE)) {
            if (message.message == WM_QUIT)
                g_running = false;
            TranslateMessage(&message);
            DispatchMessage(&message);
        }

        for(DWORD controllerIndex = 0; controllerIndex < XUSER_MAX_COUNT; controllerIndex++) {
            XINPUT_STATE controllerState;
            result = XInputGetState(controllerIndex, &controllerState);
            if(result != ERROR_SUCCESS) {
                // Todo(Sora): Could not obtain controller state for given index
                continue;
            }

            XINPUT_GAMEPAD* pad = &controllerState.Gamepad;

            bool up = (pad->wButtons & XINPUT_GAMEPAD_DPAD_UP);
            bool down = (pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN);
            bool left = (pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT);
            bool right = (pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT);
            bool start = (pad->wButtons & XINPUT_GAMEPAD_START);
            bool back = (pad->wButtons & XINPUT_GAMEPAD_BACK);
            bool lShoulder = (pad->wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER);
            bool rShoulder = (pad->wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER);
            bool aButton = (pad->wButtons & XINPUT_GAMEPAD_A);
            bool bButton = (pad->wButtons & XINPUT_GAMEPAD_B);
            bool xButton = (pad->wButtons & XINPUT_GAMEPAD_X);
            bool yButton = (pad->wButtons & XINPUT_GAMEPAD_Y);

            int16 stickX = pad->sThumbLX;
            int16 stickY = pad->sThumbLY;

            xOffset += stickX >> 12;
            yOffset += stickY >> 12;
        }

        RenderWeirdGradient(g_backbuffer, xOffset, yOffset);

        Win32_WindowDimension dimension = Win32GetWindowDimension(window);
        Win32DisplayBufferInWindow(deviceContext, dimension.width, dimension.height, g_backbuffer);

    }


    return EXIT_SUCCESS;
}





























