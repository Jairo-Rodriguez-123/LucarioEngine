#pragma once
#include "Prerequisites.h"

 /*
  *  @brief Forward declaration for the Mesh class used by render objects.
 */
class Mesh;
 /*
  *  @brief Forward declaration for the MaterialInstance class used by render objects.
 */
class MaterialInstance;

 /*
  *  @brief Material domain indicating how a material is rendered (opaque/masked/transparent).
 */
enum class
MaterialDomain {
	 /*
	  *  @brief Fully opaque material.
	 */
	Opaque = 0,
	 /*
	  *  @brief Material that uses alpha cutoff masking.
	 */
	Masked,
	 /*
	  *  @brief Material that supports transparency blending.
	 */
	Transparent
};

 /*
  *  @brief Blending mode used by materials for transparency.
 */

 /*
	*  @brief Tipo de renderizador activo en el motor.
	*/
enum class RendererType {
	
	Forward = 0,
	Deferred
};

enum class
BlendMode {


	 /*
	  *  @brief No blending, fully opaque.
	 */
	Opaque = 0,
	 /*
	  *  @brief Standard alpha blending.
	 */
	Alpha,
	 /*
	  *  @brief Additive blending mode.
	 */
	Additive,
	 /*
	  *  @brief Premultiplied alpha blending.
	 */
	PremultipliedAlpha
};

 /*
  *  @brief Types of render passes used by the renderer.
 */
enum class
RenderPassType {
	 /*
	  *  @brief Shadow pass for rendering depth from lights.
	 */
	Shadow = 0,
	 /*
	  *  @brief Main opaque geometry pass.
	 */
	Opaque,
	 /*
	  *  @brief Skybox rendering pass.
	 */
	Skybox,
	 /*
	  *  @brief Transparent geometry pass.
	 */
	Transparent,
	 /*
	  *  @brief Editor-specific rendering pass.
	 */
	Editor
};

 /*
  *  @brief Types of lights supported by the engine.
 */
enum class
LightType {
	 /*
	  *  @brief Directional light, infinite directionally lit source.
	 */
	Directional = 0,
	 /*
	  *  @brief Point light emitting in all directions from a position.
	 */
	Point,
	 /*
	  *  @brief Spot light with cone angle and direction.
	 */
	Spot
};

 /*
  *  @brief Maximum number of scene lights supported by the engine.
 */
constexpr int kMaxSceneLights = 8;

 /*
  *  @brief Data layout for a single light used in shaders and CPU side.
 */
struct
LightData {
	 /*
	  *  @brief The type of the light (Directional, Point, Spot).
	 */
	LightType type = LightType::Directional;
	 /*
	  *  @brief RGB color of the light.
	 */
	EU::Vector3 color = EU::Vector3(1.0f, 1.0f, 1.0f);
	 /*
	  *  @brief Intensity multiplier for the light.
	 */
	float intensity = 1.0f;

	 /*
	  *  @brief Direction vector for directional or spot lights.
	 */
	EU::Vector3 direction = EU::Vector3(0.0f, -1.0f, 0.0f);
	 /*
	  *  @brief Effective range for point/spot lights.
	 */
	float range = 0.0f;

	 /*
	  *  @brief World-space position for point/spot lights.
	 */
	EU::Vector3 position = EU::Vector3(0.0f, 0.0f, 0.0f);
	 /*
	  *  @brief Spot light cone angle (in degrees or radians as used by the engine).
	 */
	float spotAngle = 0.0f;
	 /*
	  *  @brief Whether this light is allowed to generate the scene shadow map.
	  *
	  *  This is CPU-side render metadata; it is copied from LightComponent and is
	  *  intentionally not packed into CBPerFrame.
	 */
	bool castShadow = false;
};

 /*
  *  @brief Material parameter block containing PBR parameters and factors.
 */
struct
MaterialParams {
	 /*
	  *  @brief Base color (RGBA) for the material.
	 */
	XMFLOAT4 baseColor = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	 /*
	  *  @brief Metallic factor for PBR shading.
	 */
	float metallic = 1.0f;
	 /*
	  *  @brief Roughness factor for PBR shading.
	 */
	float roughness = 1.0f;
	 /*
	  *  @brief Ambient occlusion multiplier.
	 */
	float ao = 1.0f;
	 /*
	  *  @brief Normal map influence scale.
	 */
	float normalScale = 1.0f;
	 /*
	  *  @brief Emissive strength multiplier.
	 */
	float emissiveStrength = 1.0f;
	 /*
	  *  @brief Alpha cutoff threshold for masked materials.
	 */
	float alphaCutoff = 0.5f;
};

 /*
  *  @brief Constant buffer layout updated per-frame with camera and lighting info.
 */
struct
CBPerFrame {
	 /*
	  *  @brief View matrix for the current camera.
	 */
	XMFLOAT4X4 View{};
	 /*
	  *  @brief Projection matrix for the current camera.
	 */
	XMFLOAT4X4 Projection{};
	 /*
	  *  @brief Light view-projection matrix for shadow mapping.
	 */
	XMFLOAT4X4 LightViewProjection{};
	 /*
	  *  @brief Camera world position.
	 */
	EU::Vector3 CameraPos{};
	 /*
	  *  @brief Padding to align the structure.
	 */
	float pad0 = 0.0f;
	 /*
	  *  @brief Primary light direction in world space.
	 */
	EU::Vector3 LightDir = EU::Vector3(0.0f, -1.0f, 0.0f);
	 /*
	  *  @brief Padding to align the structure.
	 */
	float pad1 = 0.0f;
	 /*
	  *  @brief Primary light color.
	 */
	EU::Vector3 LightColor = EU::Vector3(1.0f, 1.0f, 1.0f);
	 /*
	  *  @brief Range of the primary light.
	 */
	float LightRange = 10.0f;
	 /*
	  *  @brief Position of the primary light.
	 */
	EU::Vector3 LightPosition = EU::Vector3(0.0f, 3.0f, 0.0f);
	 /*
	  *  @brief Integer representing the primary light type.
	 */
	int LightType = 0;
  /*
   *  @brief Array of light positions and ranges packed in XMFLOAT4 for shaders.
  */
  XMFLOAT4 LightPositionsRanges[kMaxSceneLights]{};
  /*
   *  @brief Array of light colors and types packed in XMFLOAT4 for shaders.
  */
  XMFLOAT4 LightColorsTypes[kMaxSceneLights]{};
  /*
   *  @brief Array of light colors and intensities packed in XMFLOAT4 for shaders.
  */
  XMFLOAT4 LightDirectionsIntensities[kMaxSceneLights]{};
  /*
   *  @brief Current number of active lights in the scene.
  */
  int LightCount = 0;
  /*
   *  @brief Padding vector to keep alignment of the constant buffer.
  */
  XMFLOAT3 pad2 = XMFLOAT3(0.0f, 0.0f, 0.0f);
};

 /*
  *  @brief Constant buffer layout updated per-object with transform data.
 */
struct
CBPerObject {
	 /*
	  *  @brief World transform matrix for the object.
	 */
	XMFLOAT4X4 World{};
};

 /*
  *  @brief Constant buffer layout for material-specific parameters.
 */
struct
CBPerMaterial {
	 /*
	  *  @brief Base color (RGBA) for the material in the shader.
	 */
	XMFLOAT4 BaseColor = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	 /*
	  *  @brief Metallic factor sent to the shader.
	 */
	float Metallic = 1.0f;
	 /*
	  *  @brief Roughness factor sent to the shader.
	 */
	float Roughness = 1.0f;
	 /*
	  *  @brief Ambient occlusion factor for the shader.
	 */
	float AO = 1.0f;
	 /*
	  *  @brief Normal map scale for the shader.
	 */
	float NormalScale = 1.0f;
	 /*
	  *  @brief Emissive strength for the shader.
	 */
	float EmissiveStrength = 1.0f;
	 /*
	  *  @brief Alpha cutoff value for masked materials in the shader.
	 */
	float AlphaCutoff = 0.0f;
	 /*
	  *  @brief Padding to maintain alignment.
	 */
	float pad0 = 0.0f;
	 /*
	  *  @brief Padding to maintain alignment.
	 */
	float pad1 = 0.0f;
	 /*
	  *  @brief Padding to maintain alignment.
	 */
	float pad2 = 0.0f;
	 /*
	  *  @brief Padding to maintain alignment.
	 */
	float pad3 = 0.0f;
	 /*
	  *  @brief Padding to maintain alignment.
	 */
	float pad4 = 0.0f;
	 /*
	  *  @brief Padding to maintain alignment.
	 */
	float pad5 = 0.0f;
};

static_assert((sizeof(CBPerFrame) % 16) == 0, "CBPerFrame must be a multiple of 16 bytes");
static_assert((sizeof(CBPerObject) % 16) == 0, "CBPerObject must be a multiple of 16 bytes");
static_assert((sizeof(CBPerMaterial) % 16) == 0, "CBPerMaterial must be a multiple of 16 bytes");

 /*
  *  @brief Represents a renderable object with mesh, materials and rendering flags.
 */
struct
RenderObject {
	 /*
	  *  @brief Pointer to the mesh used by this render object.
	 */
	Mesh* mesh = nullptr;
	 /*
	  *  @brief Pointer to the primary material instance for the object.
	 */
	MaterialInstance* materialInstance = nullptr;
	 /*
	  *  @brief List of material instances (for multi-material meshes).
	 */
	std::vector<MaterialInstance*> materialInstances;
	 /*
	  *  @brief World transform matrix for this render object.
	 */
	XMMATRIX world = XMMatrixIdentity();
	 /*
	  *  @brief Whether the object casts shadows.
	 */
	bool castShadow = true;
	 /*
	  *  @brief Whether the object should be rendered in the transparent pass.
	 */
	bool transparent = false;
	 /*
	  *  @brief Cached distance to the camera for sorting.
	 */
	float distanceToCamera = 0.0f;
};


