/**
 * @file Texture.cpp
 * @brief Implementa la logica de Texture dentro del subsistema Core.
 * @ingroup core
 */
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "Texture.h"
#include "Device.h"
#include "DeviceContext.h"
#include <cstdint>
#include <fstream>
#include <algorithm>
#include <cmath>
#include <utility>
#include <limits>

namespace {
constexpr uint32_t kTextureCacheMagic = 0x58545657; // WVTX
constexpr uint32_t kTextureCacheVersion = 1;

struct CachedTextureData {
  int width = 0;
  int height = 0;
  std::vector<unsigned char> rgba;
};

bool GetFileWriteTime(const std::string& path, ULONGLONG& outWriteTime) {
  WIN32_FILE_ATTRIBUTE_DATA attributes{};
  if (!GetFileAttributesExA(path.c_str(), GetFileExInfoStandard, &attributes)) {
    return false;
  }

  ULARGE_INTEGER fileTime{};
  fileTime.LowPart = attributes.ftLastWriteTime.dwLowDateTime;
  fileTime.HighPart = attributes.ftLastWriteTime.dwHighDateTime;
  outWriteTime = fileTime.QuadPart;
  return true;
}

std::string GetTextureCachePath(const std::string& sourcePath) {
  return sourcePath + ".wvtx";
}

bool IsTextureCacheUpToDate(const std::string& sourcePath, const std::string& cachePath) {
  ULONGLONG sourceWriteTime = 0;
  ULONGLONG cacheWriteTime = 0;
  if (!GetFileWriteTime(sourcePath, sourceWriteTime)) {
    return false;
  }
  if (!GetFileWriteTime(cachePath, cacheWriteTime)) {
    return false;
  }
  return cacheWriteTime >= sourceWriteTime;
}

bool ComputeTextureDataSize(int width, int height, uint32_t& outSize) {
  if (width <= 0 || height <= 0 ||
      width > static_cast<int>(D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION) ||
      height > static_cast<int>(D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION)) {
    return false;
  }

  const uint64_t dataSize = static_cast<uint64_t>(width) *
                            static_cast<uint64_t>(height) * 4ull;
  if (dataSize == 0 || dataSize > std::numeric_limits<uint32_t>::max()) {
    return false;
  }

  outSize = static_cast<uint32_t>(dataSize);
  return true;
}

bool ComputeRgbaRowPitch(int width, UINT& outPitch) {
  if (width <= 0) {
    return false;
  }
  const uint64_t pitch = static_cast<uint64_t>(width) * 4ull;
  if (pitch > std::numeric_limits<UINT>::max()) {
    return false;
  }
  outPitch = static_cast<UINT>(pitch);
  return true;
}

bool SaveTextureCache(const std::string& cachePath, int width, int height, const unsigned char* data) {
  uint32_t dataSize = 0;
  if (!data || !ComputeTextureDataSize(width, height, dataSize)) {
    return false;
  }

  std::ofstream stream(cachePath, std::ios::binary | std::ios::trunc);
  if (!stream.is_open()) {
    return false;
  }

  stream.write(reinterpret_cast<const char*>(&kTextureCacheMagic), sizeof(kTextureCacheMagic));
  stream.write(reinterpret_cast<const char*>(&kTextureCacheVersion), sizeof(kTextureCacheVersion));
  stream.write(reinterpret_cast<const char*>(&width), sizeof(width));
  stream.write(reinterpret_cast<const char*>(&height), sizeof(height));
  stream.write(reinterpret_cast<const char*>(&dataSize), sizeof(dataSize));
  stream.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(dataSize));
  return stream.good();
}

bool LoadTextureCache(const std::string& cachePath, CachedTextureData& outTexture) {
  std::ifstream stream(cachePath, std::ios::binary);
  if (!stream.is_open()) {
    return false;
  }

  uint32_t magic = 0;
  uint32_t version = 0;
  uint32_t dataSize = 0;
  stream.read(reinterpret_cast<char*>(&magic), sizeof(magic));
  stream.read(reinterpret_cast<char*>(&version), sizeof(version));
  stream.read(reinterpret_cast<char*>(&outTexture.width), sizeof(outTexture.width));
  stream.read(reinterpret_cast<char*>(&outTexture.height), sizeof(outTexture.height));
  stream.read(reinterpret_cast<char*>(&dataSize), sizeof(dataSize));

  uint32_t expectedDataSize = 0;
  if (!stream.good() ||
      magic != kTextureCacheMagic ||
      version != kTextureCacheVersion ||
      !ComputeTextureDataSize(outTexture.width, outTexture.height, expectedDataSize) ||
      dataSize != expectedDataSize) {
    return false;
  }

  const std::streampos payloadStart = stream.tellg();
  stream.seekg(0, std::ios::end);
  const std::streampos fileEnd = stream.tellg();
  if (payloadStart < 0 || fileEnd < payloadStart ||
      static_cast<uint64_t>(fileEnd - payloadStart) < static_cast<uint64_t>(dataSize)) {
    return false;
  }
  stream.seekg(payloadStart);

  outTexture.rgba.resize(dataSize);
  stream.read(reinterpret_cast<char*>(outTexture.rgba.data()), static_cast<std::streamsize>(dataSize));
  return stream.good();
}

HRESULT CreateTextureFromRGBA(Device& device,
                              int width,
                              int height,
                              const unsigned char* data,
                              ID3D11Texture2D** outTexture,
                              ID3D11ShaderResourceView** outSRV) {
  if (!device.m_device || width <= 0 || height <= 0 || !data || !outTexture || !outSRV) {
    return E_INVALIDARG;
  }

  if (width > static_cast<int>(D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION) ||
      height > static_cast<int>(D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION)) {
    return E_INVALIDARG;
  }
  UINT rowPitch = 0;
  if (!ComputeRgbaRowPitch(width, rowPitch)) {
    return E_INVALIDARG;
  }

  D3D11_TEXTURE2D_DESC textureDesc = {};
  textureDesc.Width = static_cast<UINT>(width);
  textureDesc.Height = static_cast<UINT>(height);
  textureDesc.MipLevels = 1;
  textureDesc.ArraySize = 1;
  textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  textureDesc.SampleDesc.Count = 1;
  textureDesc.Usage = D3D11_USAGE_DEFAULT;
  textureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

  D3D11_SUBRESOURCE_DATA initData = {};
  initData.pSysMem = data;
  initData.SysMemPitch = rowPitch;

  HRESULT hr = device.CreateTexture2D(&textureDesc, &initData, outTexture);
  if (FAILED(hr)) {
    return hr;
  }

  SAFE_RELEASE(*outSRV);
  D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
  srvDesc.Format = textureDesc.Format;
  srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
  srvDesc.Texture2D.MipLevels = 1;

  hr = device.m_device->CreateShaderResourceView(*outTexture, &srvDesc, outSRV);
  if (FAILED(hr)) {
    if (*outTexture != nullptr) {
      (*outTexture)->Release();
      *outTexture = nullptr;
    }
  }
  return hr;
}

HRESULT InitTextureFromImage(Device& device, const std::string& fullPath, Texture& texture) {
  CachedTextureData cachedTexture;
  const std::string cachePath = GetTextureCachePath(fullPath);

  int width = 0;
  int height = 0;
  int channels = 0;
  unsigned char* decodedData = nullptr;
  const unsigned char* uploadData = nullptr;

  if (IsTextureCacheUpToDate(fullPath, cachePath) && LoadTextureCache(cachePath, cachedTexture)) {
    width = cachedTexture.width;
    height = cachedTexture.height;
    uploadData = cachedTexture.rgba.data();
  }
  else {
    int infoWidth = 0;
    int infoHeight = 0;
    int infoChannels = 0;
    uint32_t expectedDecodedSize = 0;
    if (!stbi_info(fullPath.c_str(), &infoWidth, &infoHeight, &infoChannels) ||
        !ComputeTextureDataSize(infoWidth, infoHeight, expectedDecodedSize)) {
      ERROR("Texture", "init", "Texture dimensions are invalid or exceed D3D11 limits.");
      return E_INVALIDARG;
    }

    decodedData = stbi_load(fullPath.c_str(), &width, &height, &channels, 4);
    if (!decodedData) {
      ERROR("Texture", "init",
        ("Failed to load texture: " + std::string(stbi_failure_reason())).c_str());
      return E_FAIL;
    }
    uploadData = decodedData;
    SaveTextureCache(cachePath, width, height, decodedData);
  }

  HRESULT hr = CreateTextureFromRGBA(device, width, height, uploadData, &texture.m_texture, &texture.m_textureFromImg);
  if (decodedData) {
    stbi_image_free(decodedData);
  }

  if (FAILED(hr)) {
    SAFE_RELEASE(texture.m_texture);
    SAFE_RELEASE(texture.m_textureFromImg);
    ERROR("Texture", "init", "Failed to create shader resource view for cached image texture");
    return hr;
  }

  return S_OK;
}
}


Texture::Texture(const Texture& other)
  : m_texture(other.m_texture), m_textureFromImg(other.m_textureFromImg), m_textureName(other.m_textureName) {
  if (m_texture) m_texture->AddRef();
  if (m_textureFromImg) m_textureFromImg->AddRef();
}

Texture& Texture::operator=(const Texture& other) {
  if (this == &other) return *this;
  ID3D11Texture2D* newTexture = other.m_texture;
  ID3D11ShaderResourceView* newSRV = other.m_textureFromImg;
  if (newTexture) newTexture->AddRef();
  if (newSRV) newSRV->AddRef();
  destroy();
  m_texture = newTexture;
  m_textureFromImg = newSRV;
  m_textureName = other.m_textureName;
  return *this;
}

Texture::Texture(Texture&& other) noexcept
  : m_texture(other.m_texture), m_textureFromImg(other.m_textureFromImg), m_textureName(std::move(other.m_textureName)) {
  other.m_texture = nullptr;
  other.m_textureFromImg = nullptr;
}

Texture& Texture::operator=(Texture&& other) noexcept {
  if (this == &other) return *this;
  destroy();
  m_texture = other.m_texture;
  m_textureFromImg = other.m_textureFromImg;
  m_textureName = std::move(other.m_textureName);
  other.m_texture = nullptr;
  other.m_textureFromImg = nullptr;
  return *this;
}

Texture::~Texture() { destroy(); }

HRESULT 
Texture::init(Device& device, 
              const std::string& textureName, 
              ExtensionType extensionType) {
	if (!device.m_device) {
		ERROR("Texture", "init", "Device is null.");
		return E_POINTER;
	}
	if (textureName.empty()) {
		ERROR("Texture", "init", "Texture name cannot be empty.");
		return E_INVALIDARG;
	}

	destroy();
	HRESULT hr = S_OK;

	auto resolvePath = [&](const char* defaultExtension) {
		const size_t slash = textureName.find_last_of("/\\");
		const size_t dot = textureName.find_last_of('.');
		const bool hasExtension = dot != std::string::npos &&
			(slash == std::string::npos || dot > slash);
		return hasExtension ? textureName : textureName + defaultExtension;
	};

	switch (extensionType) {
	case DDS: {
		m_textureName = resolvePath(".dds");
#if WV_HAS_D3DX11
		hr = D3DX11CreateShaderResourceViewFromFileA(
			device.m_device,
			m_textureName.c_str(),
			nullptr,
			nullptr,
			&m_textureFromImg,
			nullptr
		);

		if (FAILED(hr)) {
			ERROR("Texture", "init",
				("Failed to load DDS texture. Verify filepath: " + m_textureName).c_str());
			return hr;
		}
#else
		ERROR("Texture", "init",
			"DDS loading requires legacy D3DX11 or a dedicated DDS loader. PNG/JPG/TGA-style images do not require D3DX11.");
		return E_NOTIMPL;
#endif
		break;
	}

	case PNG: {
    m_textureName = resolvePath(".png");
    hr = InitTextureFromImage(device, m_textureName, *this);
		break;
	}
	case JPG: {
    m_textureName = resolvePath(".jpg");
    hr = InitTextureFromImage(device, m_textureName, *this);
		break;
	}
	default:
		ERROR("Texture", "init", "Unsupported extension type");
		return E_INVALIDARG;
	}

	return hr;
}

HRESULT 
Texture::init(Device& device, 
              unsigned int width, 
              unsigned int height, 
              DXGI_FORMAT Format, 
              unsigned int BindFlags, 
              unsigned int sampleCount, 
              unsigned int qualityLevels) {
  if (!device.m_device) {
    ERROR("Texture", "init", "Device is null.");
    return E_POINTER;
  }
  if (width == 0 || height == 0 ||
      width > D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION ||
      height > D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION) {
    ERROR("Texture", "init", "Texture dimensions are invalid or exceed D3D11 limits");
    return E_INVALIDARG;
  }

  if (Format == DXGI_FORMAT_UNKNOWN || BindFlags == 0 || sampleCount == 0) return E_INVALIDARG;
  destroy();

  D3D11_TEXTURE2D_DESC desc{};
  desc.Width = width;
  desc.Height = height;
  desc.MipLevels = 1;
  desc.ArraySize = 1;
  desc.Format = Format;
  desc.SampleDesc.Count = sampleCount;
  desc.SampleDesc.Quality = sampleCount > 1 ? qualityLevels : 0;
  desc.Usage = D3D11_USAGE_DEFAULT;
  desc.BindFlags = BindFlags;
  desc.CPUAccessFlags = 0;
  desc.MiscFlags = 0;

  HRESULT hr = device.CreateTexture2D(&desc, nullptr, &m_texture);

  if (FAILED(hr)) {
    ERROR("Texture", "init",
      ("Failed to create texture with specified params. HRESULT: " + std::to_string(hr)).c_str());
    return hr;
  }

  return S_OK;
}

HRESULT
Texture::initSolidColor(Device& device,
                        unsigned char r,
                        unsigned char g,
                        unsigned char b,
                        unsigned char a) {
  if (!device.m_device) {
    return E_POINTER;
  }

  destroy();
  const unsigned char pixel[4] = { r, g, b, a };
  const HRESULT hr = CreateTextureFromRGBA(
    device, 1, 1, pixel, &m_texture, &m_textureFromImg);
  if (FAILED(hr)) {
    destroy();
    ERROR("Texture", "initSolidColor", "Failed to create 1x1 fallback texture");
    return hr;
  }
  m_textureName = "<generated-solid-color>";
  return S_OK;
}

HRESULT 
Texture::init(Device& device, Texture& textureRef, DXGI_FORMAT format) {
  if (!device.m_device) {
    ERROR("Texture", "init", "Device is null.");
    return E_POINTER;
  }
  if (!textureRef.m_texture) {
    ERROR("Texture", "init", "Texture is null.");
    return E_POINTER;
  }
  if (format == DXGI_FORMAT_UNKNOWN) {
    return E_INVALIDARG;
  }

  destroy();
  D3D11_TEXTURE2D_DESC textureDesc{};
  textureRef.m_texture->GetDesc(&textureDesc);

  D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
  srvDesc.Format = format;
  if (textureDesc.SampleDesc.Count > 1) {
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DMS;
  }
  else {
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = textureDesc.MipLevels;
    srvDesc.Texture2D.MostDetailedMip = 0;
  }

  HRESULT hr = device.m_device->CreateShaderResourceView(textureRef.m_texture,
                                                         &srvDesc,
                                                         &m_textureFromImg);

  if (FAILED(hr)) {
    ERROR("Texture", "init",
      ("Failed to create texture shader resource view. HRESULT: " + std::to_string(hr)).c_str());
    destroy();
    return hr;
  }

  return S_OK;
}

void 
Texture::update() {

}

void 
Texture::render(DeviceContext& deviceContext, 
                unsigned int StartSlot, 
                unsigned int NumViews) {
  if (!deviceContext.m_deviceContext) {
    ERROR("Texture", "render", "Device Context is null.");
    return;
  }

  if (m_textureFromImg && NumViews > 0) {
    // La clase encapsula una sola SRV; evita que D3D lea punteros adyacentes.
    deviceContext.PSSetShaderResources(StartSlot, 1, &m_textureFromImg);
  }
}

void 
Texture::destroy() {
  SAFE_RELEASE(m_texture);
  SAFE_RELEASE(m_textureFromImg);
  m_textureName.clear();
}

HRESULT 
Texture::CreateCubemap(Device& device, 
                       DeviceContext& deviceContext, 
                       const std::array<std::string, 6>& facePaths, 
                       bool generateMips) {
  if (!device.m_device || !deviceContext.m_deviceContext) return E_POINTER;
  destroy();

  stbi_set_flip_vertically_on_load(false);

  int width = 0, height = 0, channels = 0;
  std::array<unsigned char*, 6> facePixels{};
  std::array<std::string, 6> resolvedPaths{};
  facePixels.fill(nullptr);

  // Visual Studio puede ejecutar con distintos directorios de trabajo.
  // Prueba primero la ruta original y luego ubicaciones comunes del proyecto.
  auto resolveFacePath = [](const std::string& requested,
                            std::string& resolved,
                            int& outW, int& outH, int& outC) -> bool {
    const std::array<std::string, 8> candidates = {
      requested,
      std::string("Resource Files/") + requested,
      std::string("../") + requested,
      std::string("../Resource Files/") + requested,
      std::string("../../") + requested,
      std::string("../../Resource Files/") + requested,
      std::string("../../../") + requested,
      std::string("../../../Resource Files/") + requested
    };

    for (const std::string& candidate : candidates) {
      int w = 0, h = 0, c = 0;
      if (stbi_info(candidate.c_str(), &w, &h, &c)) {
        resolved = candidate;
        outW = w;
        outH = h;
        outC = c;
        return true;
      }
    }
    return false;
  };

  // Validate every face before decoding so malformed/oversized files cannot
  // trigger large allocations and we fail before creating partial GPU state.
  for (int i = 0; i < 6; ++i) {
    int w = 0, h = 0, c = 0;
    uint32_t faceDataSize = 0;

    if (!resolveFacePath(facePaths[i], resolvedPaths[i], w, h, c)) {
      const std::string msg = std::string("Could not find/decode cubemap face: ") + facePaths[i] +
        ". Tried the project directory and Resource Files variants.";
      ERROR("Texture", "CreateCubemap", msg.c_str());
      return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
    }

    if (!ComputeTextureDataSize(w, h, faceDataSize)) {
      const std::string msg = std::string("Cubemap face has invalid/unsupported dimensions: ") +
        resolvedPaths[i] + " (" + std::to_string(w) + "x" + std::to_string(h) + ").";
      ERROR("Texture", "CreateCubemap", msg.c_str());
      return E_INVALIDARG;
    }

    if (w != h) {
      const std::string msg = std::string("Cubemap face must be square: ") + resolvedPaths[i] +
        " is " + std::to_string(w) + "x" + std::to_string(h) + ".";
      ERROR("Texture", "CreateCubemap", msg.c_str());
      return E_INVALIDARG;
    }

    if (i == 0) {
      width = w;
      height = h;
    }
    else if (w != width || h != height) {
      const std::string msg = std::string("All cubemap faces must have the same dimensions. ") +
        resolvedPaths[i] + " is " + std::to_string(w) + "x" + std::to_string(h) +
        ", expected " + std::to_string(width) + "x" + std::to_string(height) + ".";
      ERROR("Texture", "CreateCubemap", msg.c_str());
      return E_INVALIDARG;
    }
  }

  for (int i = 0; i < 6; ++i) {
    int w = 0, h = 0, c = 0;
    facePixels[i] = stbi_load(resolvedPaths[i].c_str(), &w, &h, &c, 4);
    if (!facePixels[i] || w != width || h != height) {
      for (int k = 0; k <= i; ++k) {
        if (facePixels[k]) {
          stbi_image_free(facePixels[k]);
        }
      }
      const std::string msg = std::string("Failed to decode cubemap face consistently: ") + resolvedPaths[i];
      ERROR("Texture", "CreateCubemap", msg.c_str());
      return E_FAIL;
    }
  }

  if (width <= 0 || height <= 0 || width != height ||
      width > static_cast<int>(D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION)) {
    ERROR("Texture", "CreateCubemap", "Cubemap faces must be square and within D3D11 texture size limits.");
    for (auto* p : facePixels) {
      if (p) stbi_image_free(p);
    }
    return E_INVALIDARG;
  }

  UINT rowPitch = 0;
  if (!ComputeRgbaRowPitch(width, rowPitch)) {
    for (auto* p : facePixels) {
      if (p) stbi_image_free(p);
    }
    return E_INVALIDARG;
  }

  D3D11_TEXTURE2D_DESC texDesc{};
  texDesc.Width = static_cast<unsigned int>(width);
  texDesc.Height = static_cast<unsigned int>(height);
  texDesc.MipLevels = generateMips ? 0 : 1;
  texDesc.ArraySize = 6;
  texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  texDesc.SampleDesc.Count = 1;
  texDesc.SampleDesc.Quality = 0;
  texDesc.Usage = D3D11_USAGE_DEFAULT;
  texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | (generateMips ? D3D11_BIND_RENDER_TARGET : 0);
  texDesc.CPUAccessFlags = 0;
  texDesc.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE | (generateMips ? D3D11_RESOURCE_MISC_GENERATE_MIPS : 0);

  HRESULT hr = S_OK;

  if (!generateMips) {
    std::array<D3D11_SUBRESOURCE_DATA, 6> initData{};
    for (int face = 0; face < 6; ++face)
    {
      initData[face].pSysMem = facePixels[face];
      initData[face].SysMemPitch = rowPitch;
      initData[face].SysMemSlicePitch = 0;
    }

		hr = device.CreateTexture2D(&texDesc, initData.data(), &m_texture);
    if (FAILED(hr)) {
      for (auto* p : facePixels) {
        if (p) {
          stbi_image_free(p);
        }
      }
      return hr;
    }
  }
  else {
    hr = device.CreateTexture2D(&texDesc, nullptr, &m_texture);
    if (FAILED(hr)) {
      for (auto* p : facePixels) {
        if (p) {
          stbi_image_free(p);
        }
      }
      return hr;
    }

    UINT mipCount = 1u + static_cast<UINT>(std::floor(std::log2(static_cast<double>(std::max(width, height)))));

    for (UINT face = 0; face < 6; ++face)
    {
      UINT sub = D3D11CalcSubresource(0, face, mipCount);

      deviceContext.UpdateSubresource(
        m_texture,
        sub,
        nullptr,
        facePixels[face],
        rowPitch,
        0
      );
    }
  }

  D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
  srvDesc.Format = texDesc.Format;
  srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
  srvDesc.TextureCube.MostDetailedMip = 0;
  srvDesc.TextureCube.MipLevels = generateMips ? (unsigned int)-1 : 1;
  
  hr = device.m_device->CreateShaderResourceView(m_texture, &srvDesc, &m_textureFromImg);

  if (FAILED(hr)) {
    for (auto* p : facePixels) {
      if (p) {
        stbi_image_free(p);
			}
    }
    destroy();
		return hr;
  }

  if (generateMips)
  {
    deviceContext.m_deviceContext->GenerateMips(m_textureFromImg);
  }

  for (auto* p : facePixels) {
    if (p) {
      stbi_image_free(p);
    }
  }

  m_textureName = "Cubemap";

  return S_OK;
}


