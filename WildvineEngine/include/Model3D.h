/**
 * @file Model3D.h
 * @brief Declara la API de Model3D dentro del subsistema Core.
 * @ingroup core
 */
#pragma once
#include "Prerequisites.h"
#include "IResource.h"
#include "MeshComponent.h"
#include "fbx/fbxsdk.h"

enum 
ModelType {
	OBJ,
	FBX
};

class 
Model3D : public IResource {
public:
	Model3D(const std::string& name, ModelType modelType) 
	: IResource(name), m_modelType(modelType), lSdkManager(nullptr), lScene(nullptr) {
		SetType(ResourceType::Model3D);
	}

	Model3D(const std::string& name,
		const SkyboxVertex vertices[],
		const unsigned int indices[]) : IResource(name) {
		MeshComponent mesh;
		mesh.m_skyVertex.assign(vertices, vertices + 8);
		mesh.m_index.assign(indices, indices + 36);
		mesh.m_numVertex = static_cast<int>(mesh.m_skyVertex.size());
		mesh.m_numIndex = static_cast<int>(mesh.m_index.size());
		SetType(ResourceType::Model3D);
		SetState(ResourceState::Loaded);
		m_meshes.push_back(std::move(mesh));
	}

	~Model3D() override;

	// FBX manager/scene are owning raw SDK handles; copying would double-destroy them.
	Model3D(const Model3D&) = delete;
	Model3D& operator=(const Model3D&) = delete;
	Model3D(Model3D&&) = delete;
	Model3D& operator=(Model3D&&) = delete;

	bool 
	load(const std::string& path) override;
	
	bool 
	init() override;
	
	void 
	unload() override;
	
	size_t 
	getSizeInBytes() const override;

	const std::vector<MeshComponent>& 
	GetMeshes() const { return m_meshes; }

	/* FBX MODEL LOADER*/
	bool
	InitializeFBXManager();

	std::vector<MeshComponent>
	LoadFBXModel(const std::string & filePath);

	std::vector<MeshComponent>
	LoadOBJModel(const std::string& filePath);

	void 
  ProcessFBXNode(FbxNode* node);

  void 
  ProcessFBXMesh(FbxNode* node);

  void 
  ProcessFBXMaterials(FbxSurfaceMaterial* material);

	std::vector<std::string> 
  GetTextureFileNames() const { return textureFileNames; }

private:
	std::string GetBinaryCachePath() const;
	bool IsBinaryCacheUpToDate(const std::string& sourcePath, const std::string& cachePath) const;
	bool LoadBinaryCache(const std::string& cachePath);
	bool SaveBinaryCache(const std::string& cachePath) const;

private:
	FbxManager* lSdkManager = nullptr;
	FbxScene* lScene = nullptr;
	std::vector<std::string> textureFileNames;
public:
	ModelType m_modelType = ModelType::OBJ;
	std::vector<MeshComponent> m_meshes;
};


