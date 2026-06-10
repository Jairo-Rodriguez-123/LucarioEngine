#pragma once
#include "Prerequisites.h"
#include "Rendering/RenderTypes.h"

class Skybox;

class
RenderScene {
public:

	/*
		*  @brief Clears all stored render objects, lights and resets the skybox pointer.
	*/
	void clear();

public:
	/*
		*  @brief Container of opaque render objects. These are typically rendered first.
	*/
	std::vector<RenderObject> opaqueObjects;       
	/*
		*  @brief Container of transparent render objects. These are typically rendered after opaque objects.
	*/
	std::vector<RenderObject> transparentObjects;  
	/*
		*  @brief Collection of directional lights affecting the scene.
	*/
	std::vector<LightData> directionalLights;      
	/*
		*  @brief Pointer to the scene skybox. May be nullptr if no skybox is set.
	*/
	Skybox* skybox = nullptr;                      
};


