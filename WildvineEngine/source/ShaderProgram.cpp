/** @file ShaderProgram.cpp */
#include "ShaderProgram.h"
#include "Device.h"
#include "DeviceContext.h"
#include "EngineUtilities/Utilities/LayoutBuilder.h"
#include <fstream>
#include <new>
#include <filesystem>


namespace {
std::string ResolveShaderPath(const char* requestedPath) {
  if (!requestedPath || !*requestedPath) {
    return std::string();
  }

  namespace fs = std::filesystem;
  const fs::path requested(requestedPath);
  std::error_code ec;

  // Absolute paths are accepted directly.
  if (requested.is_absolute()) {
    if (fs::exists(requested, ec) && fs::is_regular_file(requested, ec)) {
      return requested.lexically_normal().string();
    }
    return std::string();
  }

  std::vector<fs::path> roots;
  ec.clear();
  roots.push_back(fs::current_path(ec));

  // Also search relative to the executable. This makes shader loading
  // independent from Visual Studio's Debugging/Working Directory setting.
  char modulePath[MAX_PATH] = {};
  const DWORD moduleLength = GetModuleFileNameA(nullptr, modulePath, MAX_PATH);
  if (moduleLength > 0 && moduleLength < MAX_PATH) {
    roots.push_back(fs::path(modulePath).parent_path());
  }

  for (const fs::path& initialRoot : roots) {
    fs::path root = initialRoot;
    for (int level = 0; level < 7 && !root.empty(); ++level) {
      const fs::path direct = root / requested;
      ec.clear();
      if (fs::exists(direct, ec) && fs::is_regular_file(direct, ec)) {
        return direct.lexically_normal().string();
      }

      const fs::path shaders = root / "Shaders" / requested;
      ec.clear();
      if (fs::exists(shaders, ec) && fs::is_regular_file(shaders, ec)) {
        return shaders.lexically_normal().string();
      }

      const fs::path resources = root / "Resource Files" / requested;
      ec.clear();
      if (fs::exists(resources, ec) && fs::is_regular_file(resources, ec)) {
        return resources.lexically_normal().string();
      }

      const fs::path parent = root.parent_path();
      if (parent == root) break;
      root = parent;
    }
  }

  return std::string();
}

class RelativeShaderInclude final : public ID3DInclude {
public:
  explicit RelativeShaderInclude(const std::string& shaderFile)
    : m_rootDirectory(directoryOf(shaderFile)) {}

  HRESULT __stdcall Open(D3D_INCLUDE_TYPE,
                         LPCSTR fileName,
                         LPCVOID parentData,
                         LPCVOID* data,
                         UINT* bytes) override {
    if (!fileName || !data || !bytes) return E_INVALIDARG;
    *data = nullptr;
    *bytes = 0;

    std::string directory = m_rootDirectory;
    const auto parent = m_openFiles.find(parentData);
    if (parentData && parent != m_openFiles.end()) {
      directory = parent->second;
    }

    std::string fullPath;
    if (isAbsolute(fileName)) {
      fullPath = fileName;
    } else {
      fullPath = directory + fileName;
    }

    std::ifstream input(fullPath.c_str(), std::ios::binary | std::ios::ate);
    if (!input) return E_FAIL;

    const std::streamoff size = input.tellg();
    if (size <= 0 || static_cast<unsigned long long>(size) > 0xFFFFFFFFull)
      return E_FAIL;

    input.seekg(0, std::ios::beg);
    char* buffer = new (std::nothrow) char[static_cast<size_t>(size)];
    if (!buffer) return E_OUTOFMEMORY;

    if (!input.read(buffer, size)) {
      delete[] buffer;
      return E_FAIL;
    }

    *data = buffer;
    *bytes = static_cast<UINT>(size);
    m_openFiles[buffer] = directoryOf(fullPath);
    return S_OK;
  }

  HRESULT __stdcall Close(LPCVOID data) override {
    if (!data) return E_INVALIDARG;
    m_openFiles.erase(data);
    delete[] static_cast<const char*>(data);
    return S_OK;
  }

private:
  static bool isAbsolute(const std::string& path) {
    return (!path.empty() && (path[0] == '/' || path[0] == '\\')) ||
           (path.size() > 1 && path[1] == ':');
  }

  static std::string directoryOf(const std::string& path) {
    const std::string::size_type pos = path.find_last_of("/\\");
    return pos == std::string::npos ? std::string() : path.substr(0, pos + 1);
  }

private:
  std::string m_rootDirectory;
  std::unordered_map<LPCVOID, std::string> m_openFiles;
};
}

HRESULT ShaderProgram::init(Device& device, const std::string& fileName,
                            const LayoutBuilder& layoutBuilder) {
  if (!device.m_device) return E_POINTER;
  if (fileName.empty() || layoutBuilder.Count() == 0) return E_INVALIDARG;

  destroy();
  m_shaderFileName = fileName;

  HRESULT hr = CreateShader(device, VERTEX_SHADER);
  if (FAILED(hr)) { destroy(); return hr; }

  hr = CreateInputLayout(device, layoutBuilder);
  if (FAILED(hr)) { destroy(); return hr; }

  hr = CreateShader(device, PIXEL_SHADER);
  if (FAILED(hr)) { destroy(); return hr; }
  return S_OK;
}

void ShaderProgram::update() {}

HRESULT ShaderProgram::CreateInputLayout(Device& device, const LayoutBuilder& layoutBuilder) {
  if (!device.m_device) return E_POINTER;
  if (!m_vertexShaderData) return E_POINTER;
  const auto& layout = layoutBuilder.Get();
  if (layout.empty()) return E_INVALIDARG;

  HRESULT hr = m_inputLayout.init(device, layout.data(), static_cast<UINT>(layout.size()),
                                  m_vertexShaderData);
  // El blob de VS ya no es necesario despues de crear el input layout.
  SAFE_RELEASE(m_vertexShaderData);
  return hr;
}

HRESULT ShaderProgram::CreateShader(Device& device, ShaderType type) {
  if (!device.m_device) return E_POINTER;
  if (m_shaderFileName.empty()) return E_INVALIDARG;
  if (type != VERTEX_SHADER && type != PIXEL_SHADER) return E_INVALIDARG;

  ID3DBlob* shaderData = nullptr;
  const char* entryPoint = type == PIXEL_SHADER ? "PS" : "VS";
  const char* model = type == PIXEL_SHADER ? "ps_5_0" : "vs_5_0";

  HRESULT hr = CompileShaderFromFile(m_shaderFileName.c_str(), entryPoint, model, &shaderData);
  if (FAILED(hr) || !shaderData) {
    SAFE_RELEASE(shaderData);
    const std::string message = "Failed to compile shader: " + m_shaderFileName;
    ERROR("ShaderProgram", "CreateShader", message.c_str());
    return FAILED(hr) ? hr : E_FAIL;
  }

  if (type == PIXEL_SHADER) {
    SAFE_RELEASE(m_PixelShader);
    hr = device.CreatePixelShader(shaderData->GetBufferPointer(),
                                  static_cast<unsigned int>(shaderData->GetBufferSize()),
                                  nullptr, &m_PixelShader);
    SAFE_RELEASE(m_pixelShaderData);
    // Pixel shader bytecode no se necesita luego de CreatePixelShader.
    SAFE_RELEASE(shaderData);
  } else {
    SAFE_RELEASE(m_VertexShader);
    hr = device.CreateVertexShader(shaderData->GetBufferPointer(),
                                   static_cast<unsigned int>(shaderData->GetBufferSize()),
                                   nullptr, &m_VertexShader);
    if (SUCCEEDED(hr)) {
      SAFE_RELEASE(m_vertexShaderData);
      m_vertexShaderData = shaderData;
      shaderData = nullptr;
    }
    SAFE_RELEASE(shaderData);
  }

  if (FAILED(hr)) ERROR("ShaderProgram", "CreateShader", "Failed to create shader object");
  return hr;
}

HRESULT ShaderProgram::CreateShader(Device& device, ShaderType type,
                                    const std::string& fileName) {
  if (fileName.empty()) return E_INVALIDARG;
  m_shaderFileName = fileName;
  return CreateShader(device, type);
}

HRESULT ShaderProgram::CompileShaderFromFile(const char* szFileName,
                                             LPCSTR szEntryPoint,
                                             LPCSTR szShaderModel,
                                             ID3DBlob** ppBlobOut) {
  if (!szFileName || !*szFileName || !szEntryPoint || !szShaderModel || !ppBlobOut)
    return E_INVALIDARG;
  *ppBlobOut = nullptr;

  const std::string resolvedFileName = ResolveShaderPath(szFileName);
  if (resolvedFileName.empty()) {
    const std::string message = std::string("Shader file not found: ") + szFileName +
      ". Copy the physical Shaders folder next to the project/source folders.";
    ERROR("ShaderProgram", "CompileShaderFromFile", message.c_str());
    return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
  }

  DWORD flags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined(DEBUG) || defined(_DEBUG)
  flags |= D3DCOMPILE_DEBUG;
#endif

  ID3DBlob* errorBlob = nullptr;
  HRESULT hr = E_FAIL;

// D3DCompileFromFile and D3D_COMPILE_STANDARD_FILE_INCLUDE were added to
// newer Windows SDKs. The June 2010 DirectX SDK headers do not expose them,
// so keep a compatible D3DCompile fallback for older projects.
#if defined(D3D_COMPILE_STANDARD_FILE_INCLUDE)
  int wideLength = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                                       resolvedFileName.c_str(), -1, nullptr, 0);
  UINT codePage = CP_UTF8;
  DWORD conversionFlags = MB_ERR_INVALID_CHARS;
  if (wideLength <= 0) {
    codePage = CP_ACP;
    conversionFlags = 0;
    wideLength = MultiByteToWideChar(codePage, conversionFlags,
                                     resolvedFileName.c_str(), -1, nullptr, 0);
  }
  if (wideLength <= 0) return E_INVALIDARG;

  std::wstring wideFileName(static_cast<size_t>(wideLength), L'\0');
  if (MultiByteToWideChar(codePage, conversionFlags, resolvedFileName.c_str(), -1,
                          &wideFileName[0], wideLength) <= 0) {
    return E_INVALIDARG;
  }

  hr = D3DCompileFromFile(wideFileName.c_str(), nullptr,
                          D3D_COMPILE_STANDARD_FILE_INCLUDE,
                          szEntryPoint, szShaderModel, flags, 0,
                          ppBlobOut, &errorBlob);
#else
  std::ifstream shaderFile(resolvedFileName.c_str(), std::ios::binary | std::ios::ate);
  if (!shaderFile) {
    ERROR("ShaderProgram", "CompileShaderFromFile", "Shader file could not be opened");
    return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
  }

  const std::streamoff fileSize = shaderFile.tellg();
  if (fileSize <= 0) {
    ERROR("ShaderProgram", "CompileShaderFromFile", "Shader file is empty or invalid");
    return E_FAIL;
  }

  shaderFile.seekg(0, std::ios::beg);
  std::vector<char> source(static_cast<size_t>(fileSize));
  if (!shaderFile.read(source.data(), fileSize)) {
    ERROR("ShaderProgram", "CompileShaderFromFile", "Shader file could not be read");
    return E_FAIL;
  }

  RelativeShaderInclude includeHandler(resolvedFileName);
  hr = D3DCompile(source.data(), source.size(), resolvedFileName.c_str(), nullptr, &includeHandler,
                  szEntryPoint, szShaderModel, flags, 0,
                  ppBlobOut, &errorBlob);
#endif

  if (FAILED(hr)) {
    if (errorBlob && errorBlob->GetBufferPointer()) {
      const std::string message = std::string("Shader compilation failed: ") +
        static_cast<const char*>(errorBlob->GetBufferPointer());
      ERROR("ShaderProgram", "CompileShaderFromFile", message.c_str());
    } else {
      ERROR("ShaderProgram", "CompileShaderFromFile",
            "Shader compilation failed without compiler message");
    }
  }
  SAFE_RELEASE(errorBlob);
  return hr;
}

void ShaderProgram::render(DeviceContext& deviceContext) {
  if (!deviceContext.m_deviceContext) return;
  if (!m_VertexShader || !m_PixelShader || !m_inputLayout.m_inputLayout) {
    ERROR("ShaderProgram", "render", "Shaders or input layout are not initialized");
    return;
  }
  m_inputLayout.render(deviceContext);
  deviceContext.VSSetShader(m_VertexShader, nullptr, 0);
  deviceContext.PSSetShader(m_PixelShader, nullptr, 0);
}

void ShaderProgram::render(DeviceContext& deviceContext, ShaderType type) {
  if (!deviceContext.m_deviceContext) return;
  if (type == VERTEX_SHADER) {
    deviceContext.VSSetShader(m_VertexShader, nullptr, 0);
  } else if (type == PIXEL_SHADER) {
    deviceContext.PSSetShader(m_PixelShader, nullptr, 0);
  }
}

void ShaderProgram::destroy() {
  SAFE_RELEASE(m_VertexShader);
  SAFE_RELEASE(m_PixelShader);
  SAFE_RELEASE(m_vertexShaderData);
  SAFE_RELEASE(m_pixelShaderData);
  m_inputLayout.destroy();
}
