/**
 * @file Skybox.h
 * @brief Declara la API de Skybox dentro del subsistema Utilities.
 * @ingroup utilities
 */
#pragma once
#include "Prerequisites.h"
#include "ShaderProgram.h"
#include "Texture.h"
#include "Buffer.h"
#include "SamplerState.h"
#include "Model3D.h"
#include "RasterizerState.h"
#include "DepthStencilState.h"
#include "EngineUtilities/Utilities/Camera.h"
#include "ECS/Actor.h"

class Device;
class DeviceContext;

class 
Skybox {
public:
	Skybox()  = default;
	~Skybox() { destroy(); }

	HRESULT
	init(Device& device, DeviceContext* deviceContext, Texture& panorama);

	// Replaces the equirectangular panorama without rebuilding geometry/shaders.
	void setTexture(Texture& panorama);
	void setIntensity(float intensity);
	void setRotationDegrees(float degrees);
	void setTint(float r, float g, float b);
	
	void 
	update(DeviceContext& deviceContext, Camera& camera);

	void
	render(DeviceContext& deviceContext);

	void
	destroy();

private:
	ShaderProgram m_shaderProgram;
	Buffer m_constantBuffer;
	SamplerState m_samplerState;
	RasterizerState m_rasterizerState;
	DepthStencilState m_depthStencilState;
	Texture m_skyboxTexture;
	Model3D* m_cubeModel = nullptr;
	EU::TSharedPointer<Actor> m_skybox;
	float m_intensity = 1.0f;
	float m_rotationDegrees = 0.0f;
	XMFLOAT3 m_tint = XMFLOAT3(1.0f, 1.0f, 1.0f);

};

