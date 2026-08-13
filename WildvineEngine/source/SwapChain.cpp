/**
 * @file SwapChain.cpp
 * @brief Implementa la logica de SwapChain dentro del subsistema Core.
 * @ingroup core
 */
#include "SwapChain.h"
#include "Device.h"
#include "DeviceContext.h"
#include "Texture.h"
#include "Window.h"

HRESULT SwapChain::init(Device& device, DeviceContext& deviceContext,
                        Texture& backBuffer, const Window& window) {
  if (!window.m_hWnd || window.m_width == 0 || window.m_height == 0) return E_INVALIDARG;

  // Permite reinicializar la clase sin filtrar interfaces previas.
  destroy();
  device.destroy();
  deviceContext.destroy();

  unsigned int createDeviceFlags = 0;
#ifdef _DEBUG
  createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

  const D3D_DRIVER_TYPE driverTypes[] = {
    D3D_DRIVER_TYPE_HARDWARE,
    D3D_DRIVER_TYPE_WARP,
    D3D_DRIVER_TYPE_REFERENCE
  };
  const D3D_FEATURE_LEVEL featureLevels[] = {
    D3D_FEATURE_LEVEL_11_0,
    D3D_FEATURE_LEVEL_10_1,
    D3D_FEATURE_LEVEL_10_0
  };

  auto tryCreateDevice = [&](unsigned int flags) -> HRESULT {
    HRESULT result = E_FAIL;
    for (D3D_DRIVER_TYPE driverType : driverTypes) {
      device.destroy();
      deviceContext.destroy();
      result = D3D11CreateDevice(nullptr, driverType, nullptr, flags,
                                 featureLevels, ARRAYSIZE(featureLevels), D3D11_SDK_VERSION,
                                 &device.m_device, &m_featureLevel, &deviceContext.m_deviceContext);
      if (SUCCEEDED(result)) {
        m_driverType = driverType;
        return result;
      }
    }
    return result;
  };

  HRESULT hr = tryCreateDevice(createDeviceFlags);
#ifdef _DEBUG
  // En equipos sin la capa de depuracion instalada, un build Debug debe seguir
  // pudiendo arrancar el motor en lugar de fallar toda la inicializacion D3D11.
  if (FAILED(hr) && (createDeviceFlags & D3D11_CREATE_DEVICE_DEBUG) != 0) {
    hr = tryCreateDevice(createDeviceFlags & ~D3D11_CREATE_DEVICE_DEBUG);
  }
#endif
  if (FAILED(hr)) {
    device.destroy();
    deviceContext.destroy();
    return hr;
  }

  auto failInitialization = [&](HRESULT failure) -> HRESULT {
    destroy();
    deviceContext.destroy();
    device.destroy();
    backBuffer.destroy();
    return failure;
  };

  // Intentar 4x MSAA y degradar limpiamente a 1x cuando no este disponible.
  m_sampleCount = 4;
  m_qualityLevels = 0;
  hr = device.m_device->CheckMultisampleQualityLevels(DXGI_FORMAT_R8G8B8A8_UNORM,
                                                       m_sampleCount, &m_qualityLevels);
  if (FAILED(hr) || m_qualityLevels == 0) {
    m_sampleCount = 1;
    m_qualityLevels = 0;
    hr = S_OK;
  }

  DXGI_SWAP_CHAIN_DESC sd{};
  sd.BufferCount = 1;
  sd.BufferDesc.Width = window.m_width;
  sd.BufferDesc.Height = window.m_height;
  sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  sd.BufferDesc.RefreshRate.Numerator = 60;
  sd.BufferDesc.RefreshRate.Denominator = 1;
  sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  sd.OutputWindow = window.m_hWnd;
  sd.Windowed = TRUE;
  sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
  sd.SampleDesc.Count = m_sampleCount;
  sd.SampleDesc.Quality = (m_sampleCount > 1 && m_qualityLevels > 0) ? m_qualityLevels - 1 : 0;

  hr = device.m_device->QueryInterface(__uuidof(IDXGIDevice), reinterpret_cast<void**>(&m_dxgiDevice));
  if (FAILED(hr)) return failInitialization(hr);
  hr = m_dxgiDevice->GetAdapter(&m_dxgiAdapter);
  if (FAILED(hr)) return failInitialization(hr);
  hr = m_dxgiAdapter->GetParent(__uuidof(IDXGIFactory), reinterpret_cast<void**>(&m_dxgiFactory));
  if (FAILED(hr)) return failInitialization(hr);

  hr = m_dxgiFactory->CreateSwapChain(device.m_device, &sd, &m_swapChain);
  if (FAILED(hr)) return failInitialization(hr);

  hr = getBackBuffer(backBuffer);
  if (FAILED(hr)) return failInitialization(hr);
  return S_OK;
}

void SwapChain::update() {}
void SwapChain::render() {}

void SwapChain::destroy() {
  SAFE_RELEASE(m_swapChain);
  SAFE_RELEASE(m_dxgiFactory);
  SAFE_RELEASE(m_dxgiAdapter);
  SAFE_RELEASE(m_dxgiDevice);
  m_driverType = D3D_DRIVER_TYPE_NULL;
  m_sampleCount = 1;
  m_qualityLevels = 0;
}

void SwapChain::present() {
  if (!m_swapChain) {
    ERROR("SwapChain", "present", "Swap chain is not initialized");
    return;
  }
  const HRESULT hr = m_swapChain->Present(0, 0);
  if (FAILED(hr)) ERROR("SwapChain", "present", "Present failed");
}

HRESULT SwapChain::resizeBuffers(unsigned int width, unsigned int height) {
  if (!m_swapChain) return E_POINTER;
  if (width == 0 || height == 0) return E_INVALIDARG;
  return m_swapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);
}

HRESULT SwapChain::getBackBuffer(Texture& backBuffer) {
  if (!m_swapChain) return E_POINTER;

  // GetBuffer debe escribir en el miembro COM, no sobre la memoria del objeto Texture.
  // Reset the entire wrapper so a stale SRV can never survive a back-buffer refresh.
  backBuffer.destroy();
  const HRESULT hr = m_swapChain->GetBuffer(
    0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&backBuffer.m_texture));
  if (FAILED(hr)) {
    backBuffer.m_texture = nullptr;
    ERROR("SwapChain", "getBackBuffer", "Failed to get back buffer");
  }
  return hr;
}
