/**
 * @file Prerequisites.h
 * @brief Declara la API de Prerequisites dentro del subsistema Core.
 * @ingroup core
 */
#pragma once
// Librerias STD
#include <string>
#include <sstream>
#include <vector>

// Evita que windows.h defina las macros min/max, que rompen llamadas como
// std::numeric_limits<T>::min()/max() y std::min/std::max.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

// Prefer the DirectXMath version shipped with modern Windows SDKs. XNAMath
// is kept only as a fallback for projects still using the legacy DirectX SDK.
#if defined(__has_include)
#  if __has_include(<DirectXMath.h>)
#    include <DirectXMath.h>
using namespace DirectX;
#  elif __has_include(<xnamath.h>)
#    include <xnamath.h>
#  else
#    error "DirectXMath.h (or legacy xnamath.h) is required."
#  endif
#else
#  include <DirectXMath.h>
using namespace DirectX;
#endif
#include <thread>
#include <memory>
#include <unordered_map>
#include <type_traits>
#include <array>
#include <algorithm>
#include <utility>
#include <cstdint>
#include <atomic>

// Librerias DirectX
#include <d3d11.h>
#include <d3dcompiler.h>

// Make the core D3D11 dependencies self-contained for Visual Studio builds.
// FBX/ImGui remain external dependencies and are intentionally not hard-coded here.
#if defined(_MSC_VER)
#  pragma comment(lib, "d3d11.lib")
#  pragma comment(lib, "dxgi.lib")
#  pragma comment(lib, "d3dcompiler.lib")
#endif

// D3DX11 is part of the deprecated DirectX SDK and is not installed by default
// with modern Windows SDKs. Keep it optional only for the legacy DDS fallback.
#if defined(__has_include)
#  if __has_include(<d3dx11.h>)
#    include <d3dx11.h>
#    define WV_HAS_D3DX11 1
#  else
#    define WV_HAS_D3DX11 0
#  endif
#else
#  include <d3dx11.h>
#  define WV_HAS_D3DX11 1
#endif
#include "Resource.h"

// Third Party Libraries
#include "EngineUtilities/Vectors/Vector2.h"
#include "EngineUtilities/Vectors/Vector3.h"
#include "EngineUtilities/Memory/TSharedPointer.h"
#include "EngineUtilities/Memory/TWeakPointer.h"
#include "EngineUtilities/Memory/TStaticPtr.h"
#include "EngineUtilities/Memory/TUniquePtr.h"

// MACROS
#define SAFE_RELEASE(x) do { if ((x) != nullptr) { (x)->Release(); (x) = nullptr; } } while (0)

#define MESSAGE(classObj, method, state) do {                         \
    std::wostringstream os_;                                         \
    os_ << classObj << "::" << method << " : "                     \
        << "[CREATION OF RESOURCE : " << state << "]\n";          \
    OutputDebugStringW(os_.str().c_str());                            \
} while (0)

#define ERROR(classObj, method, errorMSG) do {                        \
    try {                                                            \
        std::wostringstream os_;                                     \
        os_ << L"ERROR : " << classObj << L"::" << method           \
            << L" : " << errorMSG << L"\n";                         \
        OutputDebugStringW(os_.str().c_str());                        \
    } catch (...) {                                                  \
        OutputDebugStringW(L"Failed to log error message.\n");      \
    }                                                                \
} while (0)

//--------------------------------------------------------------------------------------
// Structures
//--------------------------------------------------------------------------------------
struct SimpleVertex
{
  EU::Vector3 Position;
  EU::Vector3 Normal;
  EU::Vector3 Tangent;
  EU::Vector3 Bitangent;
  EU::Vector2 TextureCoordinate;
};

struct 
SkyboxVertex {
	float x,y,z;
};


struct CBNeverChanges
{
  XMMATRIX mView;
};

struct CBSkybox
{
  XMMATRIX mviewProj;
};

struct CBChangeOnResize
{
  XMMATRIX mProjection;
};

// Constant buffer used in the vertex and pixel shaders.  Align to
// 16?bytes as required by Direct3D constant buffers.
struct CBMain
{
  //XMFLOAT4X4 World;
  XMFLOAT4X4 View;
  XMFLOAT4X4 Projection;
  EU::Vector3 CameraPos;
  float pad0;
  EU::Vector3 LightDir;
  float pad1;
  EU::Vector3 LightColor;
  float pad2;
};

struct CBChangesEveryFrame
{
  XMMATRIX mWorld;
  XMFLOAT4 vMeshColor;
};

static_assert((sizeof(CBNeverChanges) % 16) == 0, "CBNeverChanges must be a multiple of 16 bytes");
static_assert((sizeof(CBSkybox) % 16) == 0, "CBSkybox must be 16-byte aligned for D3D11 constant buffers");
static_assert((sizeof(CBChangeOnResize) % 16) == 0, "CBChangeOnResize must be a multiple of 16 bytes");
static_assert((sizeof(CBMain) % 16) == 0, "CBMain must be a multiple of 16 bytes");
static_assert((sizeof(CBChangesEveryFrame) % 16) == 0, "CBChangesEveryFrame must be a multiple of 16 bytes");

enum ExtensionType {
  DDS = 0,
  PNG = 1,
  JPG = 2
};

enum ShaderType {
  VERTEX_SHADER = 0,
  PIXEL_SHADER = 1
};

/**
 * @enum ComponentType
 * @brief Tipos de componentes disponibles en el juego.
 */
enum 
ComponentType {
  NONE = 0,     ///< Tipo de componente no especificado.
  TRANSFORM = 1,///< Componente de transformaci�n.
  MESH = 2,     ///< Componente de malla.
  MATERIAL = 3,  ///< Componente de material.
	HIERARCHY = 4,///< Componente de jerarqu�a.
  LIGHT = 5,     ///< Componente de luz.
  MESH_RENDERER = 6
};



