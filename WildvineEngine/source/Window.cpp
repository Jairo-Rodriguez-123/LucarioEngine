/** @file Window.cpp */
#include "Window.h"
#include "BaseApp.h"

HRESULT Window::init(HINSTANCE hInstance, int nCmdShow, WNDPROC wndproc, BaseApp* app) {
  if (!hInstance || !wndproc) return E_INVALIDARG;
  destroy();
  m_hInst = hInstance;

  constexpr const char* kWindowClass = "WildvineEngineWindowClass";
  WNDCLASSEXA wcex{};
  wcex.cbSize = sizeof(wcex);
  wcex.style = CS_HREDRAW | CS_VREDRAW;
  wcex.lpfnWndProc = wndproc;
  wcex.hInstance = m_hInst;
  wcex.hIcon = LoadIconA(m_hInst, MAKEINTRESOURCEA(IDI_TUTORIAL1));
  wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
  wcex.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
  wcex.lpszClassName = kWindowClass;
  wcex.hIconSm = LoadIconA(m_hInst, MAKEINTRESOURCEA(IDI_SMALL));

  if (!RegisterClassExA(&wcex) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
    ERROR("Window", "init", "RegisterClassExA failed");
    return HRESULT_FROM_WIN32(GetLastError());
  }

  RECT rc{0, 0, 1200, 950};
  if (!AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE)) {
    return HRESULT_FROM_WIN32(GetLastError());
  }

  m_hWnd = CreateWindowExA(0, kWindowClass, m_windowName.c_str(), WS_OVERLAPPEDWINDOW,
                           CW_USEDEFAULT, CW_USEDEFAULT,
                           rc.right - rc.left, rc.bottom - rc.top,
                           nullptr, nullptr, hInstance, app);
  if (!m_hWnd) {
    const DWORD error = GetLastError();
    MessageBoxA(nullptr, "CreateWindowExA failed!", "Wildvine Engine", MB_OK | MB_ICONERROR);
    return HRESULT_FROM_WIN32(error);
  }

  ShowWindow(m_hWnd, nCmdShow);
  UpdateWindow(m_hWnd);
  if (!GetClientRect(m_hWnd, &m_rect)) {
    const DWORD error = GetLastError();
    destroy();
    return HRESULT_FROM_WIN32(error);
  }
  m_width = static_cast<unsigned int>(m_rect.right - m_rect.left);
  m_height = static_cast<unsigned int>(m_rect.bottom - m_rect.top);
  if (m_width == 0 || m_height == 0) {
    destroy();
    return E_FAIL;
  }
  return S_OK;
}

void Window::update() {
  if (!m_hWnd) return;
  RECT rc{};
  if (GetClientRect(m_hWnd, &rc)) {
    m_rect = rc;
    m_width = static_cast<unsigned int>(std::max<LONG>(0, rc.right - rc.left));
    m_height = static_cast<unsigned int>(std::max<LONG>(0, rc.bottom - rc.top));
  }
}

void Window::render() {}

void Window::destroy() {
  if (m_hWnd && IsWindow(m_hWnd)) DestroyWindow(m_hWnd);
  m_hWnd = nullptr;
  m_width = 0;
  m_height = 0;
  m_rect = RECT{};
  m_hInst = nullptr;
}
