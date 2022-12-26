#include <node.h>
#include <dwmapi.h>
#include <iostream>

#define DWMWA_MICA_EFFECT               DWORD(1029)
#define DWMWA_SYSTEMBACKDROP_TYPE       DWORD(38)
#define DWMWA_USE_IMMERSIVE_DARK_MODE   DWORD(20)
#define DWMWA_WINDOW_CORNER_PREFERENCE  DWORD(33)
#define DWMWA_BORDER_COLOR              DWORD(34)
#define DWMWA_CAPTION_COLOR             DWORD(35)
#define DWMWA_TEXT_COLOR                DWORD(36)

#define DWMC_BG_AUTO        0
#define DWMC_BG_NONE        1
#define DWMC_BG_MICA        2
#define DWMC_BG_ACRYLIC     3
#define DWMC_BG_MICA_TB     4 // As per StartIsBack, "Tabbed is just untinted / unblended Mica, i.e. heavily blurred wallpaper."
#define DWMC_CORNER         5 // Adjust the roundness of the window's corners.
#define DWMC_BORDER_COLOR   6 // Adjust the color of the window's border.
#define DWMC_CAPTION_COLOR  7 // Adjust the color of the window's caption.
#define DWMC_TEXT_COLOR     8 // Adjust the color of the window's text.
#define DWMC_FRAME          9 // Enable/disable the window's frame (title bar).

using pSetWindowPos = BOOL(WINAPI*)(HWND, HWND, int, int, int, int, int);
using pSetWindowLongA = LONG(WINAPI*)(HWND, int, long);
using pDwmSetWindowAttribute = BOOL(WINAPI*)(HWND, DWORD, int*, int);
using pSetLayeredWindowAttributes = BOOL(WINAPI*)(HWND, int, byte, int);
using pDwmExtendFrameIntoClientArea = BOOL(WINAPI*)(HWND, MARGINS*);

DWORD getBuild()
{
  DWORD dwVersion = 0;
  DWORD dwMajorVersion = 0;
  DWORD dwMinorVersion = 0;
  DWORD dwBuild = 0;

  dwVersion = GetVersion();
  dwMajorVersion = (DWORD)(LOBYTE(LOWORD(dwVersion)));
  dwMinorVersion = (DWORD)(HIBYTE(LOWORD(dwVersion)));
  if (dwVersion < 0x80000000)
    dwBuild = (DWORD)(HIWORD(dwVersion));
  return dwBuild;
}

bool supports_modern_backdrop() {
  return getBuild() >= 22621;
}

bool supports_basic_backdrop() { 
  return getBuild() >= 22000;
}

bool is_light_theme()
{
  unsigned long value = 0;
  unsigned long valuesize = sizeof(value);
  auto res = RegGetValueA(
      HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize", "AppsUseLightTheme",
      RRF_RT_REG_DWORD, nullptr, &value, &valuesize);

  if (res != ERROR_SUCCESS) return false; // Default to dark theme if the registry fails.
  return value;
}

// JavaScript function that checks whether the system supports vibrancy.
// Returns 'full' if the system supports acrylic, mica and mica tabbed backdrops.
// Returns 'basic' if the system supports exclusively mica.
// Returns 'none' if the system does not support vibrancy.
void check_vibrancy_support(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::Local<v8::Context> context = isolate->GetCurrentContext();

  if (supports_modern_backdrop()) {
    args.GetReturnValue().Set(v8::String::NewFromUtf8(isolate, "full").ToLocalChecked());
  } else if (supports_basic_backdrop()) {
    args.GetReturnValue().Set(v8::String::NewFromUtf8(isolate, "basic").ToLocalChecked());
  } else {
    args.GetReturnValue().Set(v8::String::NewFromUtf8(isolate, "none").ToLocalChecked());
  }
}

// JavaScript function that redraws the window using its handle, x/y coordinates and width/height.
// Returns true if the window was successfully redrawn.
void redraw_window(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::Local<v8::Context> context = isolate->GetCurrentContext();

  // Check if the 5 arguments are present: HWND, x, y, width, height.
  if (args.Length() < 5) {
    isolate->ThrowException(v8::Exception::TypeError(
      v8::String::NewFromUtf8(isolate, "Wrong number of arguments").ToLocalChecked()));
    return;
  }

  // Check if the arguments are of the correct type.
  if (!args[0]->IsNumber() || !args[1]->IsNumber() || !args[2]->IsNumber() || !args[3]->IsNumber() || !args[4]->IsNumber()) {
    isolate->ThrowException(v8::Exception::TypeError(
      v8::String::NewFromUtf8(isolate, "Wrong arguments").ToLocalChecked()));
    return;
  }

  // Load in the library because it is not loaded by default.
  const HINSTANCE user32 = LoadLibrary(TEXT("user32.dll"));
  const pSetWindowPos SetWindowPos = reinterpret_cast<pSetWindowPos>(GetProcAddress(user32, "SetWindowPos"));

  // Get the arguments.
  HWND hwnd = reinterpret_cast<HWND>(static_cast<intptr_t>(args[0]->NumberValue(context).ToChecked()));
  int x = static_cast<int>(args[1]->NumberValue(context).ToChecked());
  int y = static_cast<int>(args[2]->NumberValue(context).ToChecked());
  int width = static_cast<int>(args[3]->NumberValue(context).ToChecked());
  int height = static_cast<int>(args[4]->NumberValue(context).ToChecked());

  // Redraw the window.
  SetWindowPos(hwnd, 0, x, y, width, height, 0x0020);
  FreeLibrary(user32);
}

// Submits a command to the DWM backdrop manager.
void submit_dwm_command(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::Local<v8::Context> context = isolate->GetCurrentContext();

  // Check if the 3 arguments are present: HWND, paramaters and value (all 3 are numbers).
  if (args.Length() < 3) {
    isolate->ThrowException(v8::Exception::TypeError(
      v8::String::NewFromUtf8(isolate, "Wrong number of arguments").ToLocalChecked()));
    return;
  }

  // Check if the arguments are numbers.
  if (!args[0]->IsNumber() || !args[1]->IsNumber() || !args[2]->IsNumber()) {
    isolate->ThrowException(v8::Exception::TypeError(v8::String::NewFromUtf8(isolate, "Wrong arguments").ToLocalChecked()));
    return;
  }

  // Cannot assume the libraries will be loaded, so fetch em first and use GetProcAddress
  // to retrieve individual functions.
  const HINSTANCE user32 = LoadLibrary(TEXT("user32.dll"));
  const HINSTANCE dwm = LoadLibrary(TEXT("dwmapi.dll"));
  const pSetWindowLongA SetWindowLongA = reinterpret_cast<pSetWindowLongA>(GetProcAddress(user32, "SetWindowLongA"));
  const pDwmSetWindowAttribute DwmSetWindowAttribute = reinterpret_cast<pDwmSetWindowAttribute>(GetProcAddress(dwm, "DwmSetWindowAttribute"));
  const pDwmExtendFrameIntoClientArea DwmExtendFrameIntoClientArea = reinterpret_cast<pDwmExtendFrameIntoClientArea>(GetProcAddress(dwm, "DwmExtendFrameIntoClientArea"));

  // Get the HWND, params and value as integers. Hopefully no conversion loss from the double->int!
  auto hwnd = reinterpret_cast<HWND>(static_cast<intptr_t>(args[0]->NumberValue(context).ToChecked()));
  auto parameters = static_cast<int>(args[1]->NumberValue(context).ToChecked());
  auto value = static_cast<int>(args[2]->NumberValue(context).ToChecked());

  // Act on the parameter adjustment.
  switch (parameters) {
    case DWMC_CORNER:
      DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &value, sizeof(int));
      break;
    case DWMC_BORDER_COLOR:
      DwmSetWindowAttribute(hwnd, DWMWA_BORDER_COLOR, &value, sizeof(int));
      break;
    case DWMC_CAPTION_COLOR:
      DwmSetWindowAttribute(hwnd, DWMWA_CAPTION_COLOR, &value, sizeof(int));
      break;
    case DWMC_TEXT_COLOR:
      DwmSetWindowAttribute(hwnd, DWMWA_TEXT_COLOR, &value, sizeof(int));
      break;
    case DWMC_FRAME:
      if (value == 0)
        SetWindowLongA(hwnd, GWL_STYLE, 0x004F0000L);
      break;

    default: {
      // Everything else is just a background mode update.
      MARGINS margins = { -1 };
      DwmExtendFrameIntoClientArea(hwnd, &margins); // Extend the frame into the entire god damn window.

      int enable = 0x00;
      if (value == 1 /* DARK */ || (value == 0 /* AUTO */ && !is_light_theme()))
        enable = 0x01;
      DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &enable, sizeof(int));

      // If this version of Windows 11 is recent enough, then we can use the new SYSTEMBACKDROP
      // dmw attribute to set the background mode. Otherwise, we have to use the old MICA_EFFECT bullshit.
      if (supports_modern_backdrop()) {
        DwmSetWindowAttribute(hwnd, DWMWA_SYSTEMBACKDROP_TYPE, &parameters, sizeof(int));
      } else {
        // Older versions of Windows 11 uses distinct APIs for acrylic/mica modes. On newer
        // versions, the same API is used for both. Since we don't support the older API, we just
        // throw an exception if code requests acrylic/tabbed and tell the user to upgrade lol.
        if (parameters > DWMC_BG_MICA) {
          isolate->ThrowException(v8::Exception::TypeError(v8::String::NewFromUtf8(isolate, "Unsupported backdrop mode").ToLocalChecked()));
          return;
        }

        // Update using the undocumented MICA_EFFECT boolean parameter.
        int micaEnable = parameters == 0 ? parameters : (parameters == 1 ? 2 : 1);
        DwmSetWindowAttribute(hwnd, DWMWA_MICA_EFFECT, &micaEnable, sizeof(int));
      }
    }
  }

  // Free the libraries and exit.
  FreeLibrary(user32);
  FreeLibrary(dwm);
}

void Initialize(v8::Local<v8::Object> exports) {
  NODE_SET_METHOD(exports, "submitDwmCommand", submit_dwm_command);
  NODE_SET_METHOD(exports, "redrawWindow", redraw_window);
}

NODE_MODULE(NODE_GYP_MODULE_NAME, Initialize);