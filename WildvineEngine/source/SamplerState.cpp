/**
 * @file SamplerState.cpp
 * @brief Implementa la logica de SamplerState dentro del subsistema Core.
 * @ingroup core
 */
#include "SamplerState.h"
#include "Device.h"
#include "DeviceContext.h"

HRESULT
SamplerState::init(Device& device) {
  if (!device.m_device) {
    ERROR("SamplerState", "init", "Device is nullptr");
    return E_POINTER;
  }

  destroy();

  D3D11_SAMPLER_DESC sampDesc = {};
  sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
  sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
  sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
  sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
  sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
  sampDesc.MinLOD = 0;
  sampDesc.MaxLOD = D3D11_FLOAT32_MAX;

  HRESULT hr = device.CreateSamplerState(&sampDesc, &m_sampler);
  if (FAILED(hr)) {
    ERROR("SamplerState", "init", "Failed to create SamplerState");
    return hr;
  }

  return S_OK;
}

void 
SamplerState::update() {
  // No hay l�gica de actualizaci�n para un sampler en este caso.
}

void 
SamplerState::render(DeviceContext& deviceContext, 
                     unsigned int StartSlot, 
                     unsigned int NumSamplers) {
  if (!m_sampler) {
    ERROR("SamplerState", "render", "SamplerState is nullptr");
    return;
  }

  // This wrapper owns exactly one sampler. Passing NumSamplers > 1 would make
  // D3D11 read adjacent memory as if it were an array of sampler pointers.
  if (NumSamplers == 0) {
    return;
  }
  deviceContext.PSSetSamplers(StartSlot, 1, &m_sampler);
}

void 
SamplerState::destroy() {
  SAFE_RELEASE(m_sampler);
}


