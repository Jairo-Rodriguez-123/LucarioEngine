/**
 * @file Model3D.cpp
 * @brief Implementa la logica de Model3D dentro del subsistema Core.
 * @ingroup core
 */
#include "Model3D.h"
#include <chrono>
#include <cstdint>
#include <cmath>
#include <fstream>
#include <unordered_map>
#include <sstream>
#include <limits>

namespace {
constexpr uint32_t kModelCacheMagic = 0x48564D57; // WMVH
constexpr uint32_t kModelCacheVersion = 4;
constexpr uint32_t kMaxCachedMeshes = 100000;
constexpr uint32_t kMaxCachedTextures = 100000;
constexpr uint32_t kMaxCachedStringBytes = 1024 * 1024;

struct ModelCacheEntry {
	std::vector<MeshComponent> meshes;
	std::vector<std::string> textureFileNames;
	ULONGLONG sourceWriteTime = 0;
};

std::unordered_map<std::string, ModelCacheEntry> g_modelCache;

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

bool HasRemainingBytes(std::ifstream& stream, uint64_t byteCount) {
	if (!stream.good()) return false;
	const std::streampos current = stream.tellg();
	if (current == std::streampos(-1)) return false;

	stream.seekg(0, std::ios::end);
	if (!stream.good()) return false;
	const std::streampos end = stream.tellg();
	if (end == std::streampos(-1) || end < current) return false;

	stream.seekg(current);
	if (!stream.good()) return false;
	return static_cast<uint64_t>(end - current) >= byteCount;
}

bool WriteString(std::ofstream& stream, const std::string& value) {
	if (value.size() > kMaxCachedStringBytes ||
		value.size() > std::numeric_limits<uint32_t>::max()) {
		return false;
	}

	const uint32_t length = static_cast<uint32_t>(value.size());
	stream.write(reinterpret_cast<const char*>(&length), sizeof(length));
	if (length > 0) {
		stream.write(value.data(), static_cast<std::streamsize>(length));
	}
	return stream.good();
}

bool ReadString(std::ifstream& stream, std::string& value) {
	uint32_t length = 0;
	stream.read(reinterpret_cast<char*>(&length), sizeof(length));
	if (!stream.good() || length > kMaxCachedStringBytes || !HasRemainingBytes(stream, length)) {
		return false;
	}

	value.resize(length);
	if (length > 0) {
		stream.read(&value[0], static_cast<std::streamsize>(length));
	}
	return stream.good();
}
}

Model3D::~Model3D() {
	unload();
}

bool 
Model3D::load(const std::string& path) {
	SetPath(path);
	SetState(ResourceState::Loading);

	ULONGLONG sourceWriteTime = 0;
	const bool sourceTimestampAvailable = GetFileWriteTime(path, sourceWriteTime);
	auto cacheIt = g_modelCache.find(path);
	if (cacheIt != g_modelCache.end()) {
		if (sourceTimestampAvailable && cacheIt->second.sourceWriteTime == sourceWriteTime) {
			m_meshes = cacheIt->second.meshes;
			textureFileNames = cacheIt->second.textureFileNames;
			SetState(ResourceState::Loaded);
			return true;
		}
		// The source changed (or disappeared), so never return stale geometry.
		g_modelCache.erase(cacheIt);
	}

	const bool success = init();
	SetState(success ? ResourceState::Loaded : ResourceState::Failed);
	return success;
}

bool Model3D::init()
{
	m_meshes.clear();
	textureFileNames.clear();

	const std::string cachePath = GetBinaryCachePath();
	if (IsBinaryCacheUpToDate(m_filePath, cachePath) && LoadBinaryCache(cachePath)) {
		ULONGLONG sourceWriteTime = 0;
		GetFileWriteTime(m_filePath, sourceWriteTime);
		g_modelCache[m_filePath] = ModelCacheEntry{ m_meshes, textureFileNames, sourceWriteTime };
		return true;
	}

	const auto begin = std::chrono::high_resolution_clock::now();
	std::vector<MeshComponent> loadedMeshes;
	if (m_modelType == ModelType::FBX) {
		loadedMeshes = LoadFBXModel(m_filePath);
	}
	else if (m_modelType == ModelType::OBJ) {
		loadedMeshes = LoadOBJModel(m_filePath);
	}
	const auto end = std::chrono::high_resolution_clock::now();
	const auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count();

	if (loadedMeshes.empty()) {
		return false;
	}

	m_meshes = std::move(loadedMeshes);
	ULONGLONG sourceWriteTime = 0;
	GetFileWriteTime(m_filePath, sourceWriteTime);
	g_modelCache[m_filePath] = ModelCacheEntry{ m_meshes, textureFileNames, sourceWriteTime };
	SaveBinaryCache(cachePath);

	const std::wstring modelPathW(m_filePath.begin(), m_filePath.end());
	MESSAGE("ModelLoader", "ModelLoader",
		L"Loaded model '" << modelPathW << L"' in " << elapsedMs << L" ms. Meshes: " << m_meshes.size());
	return true;
}

void Model3D::unload()
{
	if (lScene) {
		lScene->Destroy();
		lScene = nullptr;
	}
	if (lSdkManager) {
		lSdkManager->Destroy();
		lSdkManager = nullptr;
	}
	m_meshes.clear();
	textureFileNames.clear();
	SetState(ResourceState::Unloaded);
}

size_t Model3D::getSizeInBytes() const
{
	size_t totalSize = 0;
	for (const auto& mesh : m_meshes) {
		totalSize += mesh.m_vertex.size() * sizeof(SimpleVertex);
		totalSize += mesh.m_skyVertex.size() * sizeof(SkyboxVertex);
		totalSize += mesh.m_index.size() * sizeof(unsigned int);
	}
	return totalSize;
}

bool
Model3D::InitializeFBXManager() {
	// Permite recargar el mismo recurso sin conservar una escena/manager anterior.
	if (lScene) {
		lScene->Destroy();
		lScene = nullptr;
	}
	if (lSdkManager) {
		lSdkManager->Destroy();
		lSdkManager = nullptr;
	}

	lSdkManager = FbxManager::Create();
	if (!lSdkManager) {
		ERROR("ModelLoader", "FbxManager::Create()", "Unable to create FBX Manager!");
		return false;
	}

	FbxIOSettings* ios = FbxIOSettings::Create(lSdkManager, IOSROOT);
	if (!ios) {
		ERROR("ModelLoader", "FbxIOSettings::Create()", "Unable to create FBX IO settings!");
		lSdkManager->Destroy();
		lSdkManager = nullptr;
		return false;
	}
	lSdkManager->SetIOSettings(ios);

	lScene = FbxScene::Create(lSdkManager, "MyScene");
	if (!lScene) {
		ERROR("ModelLoader", "FbxScene::Create()", "Unable to create FBX Scene!");
		lSdkManager->Destroy();
		lSdkManager = nullptr;
		return false;
	}
	return true;
}

std::vector<MeshComponent> 
Model3D::LoadFBXModel(const std::string& filePath) {
	std::vector<MeshComponent> loadedMeshes;

	auto releaseFbxState = [this]() {
		if (lScene) {
			lScene->Destroy();
			lScene = nullptr;
		}
		if (lSdkManager) {
			lSdkManager->Destroy();
			lSdkManager = nullptr;
		}
	};

	if (InitializeFBXManager()) {
		FbxImporter* lImporter = FbxImporter::Create(lSdkManager, "");
		if (!lImporter) {
			ERROR("ModelLoader", "FbxImporter::Create()", "Unable to create FBX Importer!");
			releaseFbxState();
			return loadedMeshes;
		}

		if (!lImporter->Initialize(filePath.c_str(), -1, lSdkManager->GetIOSettings())) {
			ERROR("ModelLoader", "FbxImporter::Initialize()",
				"Unable to initialize FBX Importer! Error: " << lImporter->GetStatus().GetErrorString());
			lImporter->Destroy();
			releaseFbxState();
			return loadedMeshes;
		}

		if (!lImporter->Import(lScene)) {
			ERROR("ModelLoader", "FbxImporter::Import()",
				"Unable to import FBX Scene! Error: " << lImporter->GetStatus().GetErrorString());
			lImporter->Destroy();
			releaseFbxState();
			return loadedMeshes;
		}
		else {
			m_name = lImporter->GetFileName();
		}

		FbxAxisSystem::DirectX.ConvertScene(lScene);
		FbxSystemUnit::m.ConvertScene(lScene);
		FbxGeometryConverter gc(lSdkManager);
		gc.Triangulate(lScene, true);

		lImporter->Destroy();

		FbxNode* lRootNode = lScene->GetRootNode();
		if (lRootNode) {
			m_meshes.clear();
			for (int i = 0; i < lRootNode->GetChildCount(); i++) {
				ProcessFBXNode(lRootNode->GetChild(i));
			}
			loadedMeshes = m_meshes;
			releaseFbxState();
			return loadedMeshes;
		}
		else {
			ERROR("ModelLoader", "FbxScene::GetRootNode()",
				"Unable to get root node from FBX Scene!");
			releaseFbxState();
			return loadedMeshes;
		}
	}
	return loadedMeshes;
}

std::vector<MeshComponent>
Model3D::LoadOBJModel(const std::string& filePath) {
	struct ObjIndex {
		int position = -1;
		int texcoord = -1;
		int normal = -1;

		bool operator==(const ObjIndex& other) const {
			return position == other.position &&
				texcoord == other.texcoord &&
				normal == other.normal;
		}
	};

	struct ObjIndexHasher {
		size_t operator()(const ObjIndex& index) const {
			size_t seed = static_cast<size_t>(index.position + 1);
			seed ^= static_cast<size_t>(index.texcoord + 1) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
			seed ^= static_cast<size_t>(index.normal + 1) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
			return seed;
		}
	};

	struct ObjMeshBuilder {
		std::string name = "default";
		std::string materialName = "default";
		std::vector<SimpleVertex> vertices;
		std::vector<unsigned int> indices;
		std::unordered_map<ObjIndex, unsigned int, ObjIndexHasher> vertexLookup;
	};

	auto fixIndex = [](int index, int count) -> int {
		if (index > 0) return index - 1;
		if (index < 0) return count + index;
		return -1;
	};

	auto normalize = [](EU::Vector3& value) {
		const float lengthSq = value.x * value.x + value.y * value.y + value.z * value.z;
		if (lengthSq <= 1e-20f) {
			value = EU::Vector3(0.0f, 1.0f, 0.0f);
			return;
		}
		const float invLength = 1.0f / std::sqrt(lengthSq);
		value.x *= invLength;
		value.y *= invLength;
		value.z *= invLength;
	};

	auto parseFaceVertex = [&](const std::string& token,
		int positionCount,
		int texcoordCount,
		int normalCount) -> ObjIndex {
		ObjIndex result{};
		size_t firstSlash = token.find('/');
		size_t secondSlash = token.find('/', firstSlash == std::string::npos ? token.size() : firstSlash + 1);

		const std::string positionToken = token.substr(0, firstSlash);
		const std::string texcoordToken =
			(firstSlash == std::string::npos) ? std::string() :
			(secondSlash == std::string::npos ? token.substr(firstSlash + 1) : token.substr(firstSlash + 1, secondSlash - firstSlash - 1));
		const std::string normalToken = (secondSlash == std::string::npos) ? std::string() : token.substr(secondSlash + 1);

		try {
			if (!positionToken.empty()) result.position = fixIndex(std::stoi(positionToken), positionCount);
			if (!texcoordToken.empty()) result.texcoord = fixIndex(std::stoi(texcoordToken), texcoordCount);
			if (!normalToken.empty()) result.normal = fixIndex(std::stoi(normalToken), normalCount);
		}
		catch (const std::exception&) {
			return ObjIndex{};
		}

		return result;
	};

	auto computeTangents = [&](MeshComponent& mesh) {
		std::vector<EU::Vector3> generatedNormals(mesh.m_vertex.size(), EU::Vector3(0.0f, 0.0f, 0.0f));

		for (size_t i = 0; i + 2 < mesh.m_index.size(); i += 3) {
			const unsigned int i0 = mesh.m_index[i + 0];
			const unsigned int i1 = mesh.m_index[i + 1];
			const unsigned int i2 = mesh.m_index[i + 2];
			if (i0 >= mesh.m_vertex.size() || i1 >= mesh.m_vertex.size() || i2 >= mesh.m_vertex.size()) {
				continue;
			}

			SimpleVertex& v0 = mesh.m_vertex[i0];
			SimpleVertex& v1 = mesh.m_vertex[i1];
			SimpleVertex& v2 = mesh.m_vertex[i2];

			const EU::Vector3 edge1 = v1.Position - v0.Position;
			const EU::Vector3 edge2 = v2.Position - v0.Position;
			const EU::Vector3 faceNormal(
				edge1.y * edge2.z - edge1.z * edge2.y,
				edge1.z * edge2.x - edge1.x * edge2.z,
				edge1.x * edge2.y - edge1.y * edge2.x);
			generatedNormals[i0] += faceNormal;
			generatedNormals[i1] += faceNormal;
			generatedNormals[i2] += faceNormal;

			const float du1 = v1.TextureCoordinate.x - v0.TextureCoordinate.x;
			const float dv1 = v1.TextureCoordinate.y - v0.TextureCoordinate.y;
			const float du2 = v2.TextureCoordinate.x - v0.TextureCoordinate.x;
			const float dv2 = v2.TextureCoordinate.y - v0.TextureCoordinate.y;
			const float denominator = du1 * dv2 - du2 * dv1;
			if (std::fabs(denominator) < 1e-8f) {
				continue;
			}
			const float invDenominator = 1.0f / denominator;

			const EU::Vector3 tangent(
				(edge1.x * dv2 - edge2.x * dv1) * invDenominator,
				(edge1.y * dv2 - edge2.y * dv1) * invDenominator,
				(edge1.z * dv2 - edge2.z * dv1) * invDenominator);
			const EU::Vector3 bitangent(
				(edge2.x * du1 - edge1.x * du2) * invDenominator,
				(edge2.y * du1 - edge1.y * du2) * invDenominator,
				(edge2.z * du1 - edge1.z * du2) * invDenominator);

			v0.Tangent += tangent;
			v1.Tangent += tangent;
			v2.Tangent += tangent;
			v0.Bitangent += bitangent;
			v1.Bitangent += bitangent;
			v2.Bitangent += bitangent;
		}

		for (size_t vertexIndex = 0; vertexIndex < mesh.m_vertex.size(); ++vertexIndex) {
			SimpleVertex& vertex = mesh.m_vertex[vertexIndex];
			const float normalLengthSq =
				vertex.Normal.x * vertex.Normal.x +
				vertex.Normal.y * vertex.Normal.y +
				vertex.Normal.z * vertex.Normal.z;
			if (normalLengthSq <= 1e-20f) {
				vertex.Normal = generatedNormals[vertexIndex];
			}
			normalize(vertex.Normal);

			const float tangentDotNormal =
				vertex.Tangent.x * vertex.Normal.x +
				vertex.Tangent.y * vertex.Normal.y +
				vertex.Tangent.z * vertex.Normal.z;
			vertex.Tangent = vertex.Tangent - (vertex.Normal * tangentDotNormal);

			const float tangentLengthSq =
				vertex.Tangent.x * vertex.Tangent.x +
				vertex.Tangent.y * vertex.Tangent.y +
				vertex.Tangent.z * vertex.Tangent.z;
			if (tangentLengthSq <= 1e-20f) {
				const EU::Vector3 reference = std::fabs(vertex.Normal.y) < 0.999f
					? EU::Vector3(0.0f, 1.0f, 0.0f)
					: EU::Vector3(1.0f, 0.0f, 0.0f);
				vertex.Tangent = EU::Vector3(
					reference.y * vertex.Normal.z - reference.z * vertex.Normal.y,
					reference.z * vertex.Normal.x - reference.x * vertex.Normal.z,
					reference.x * vertex.Normal.y - reference.y * vertex.Normal.x);
			}
			normalize(vertex.Tangent);

			vertex.Bitangent = EU::Vector3(
				vertex.Normal.y * vertex.Tangent.z - vertex.Normal.z * vertex.Tangent.y,
				vertex.Normal.z * vertex.Tangent.x - vertex.Normal.x * vertex.Tangent.z,
				vertex.Normal.x * vertex.Tangent.y - vertex.Normal.y * vertex.Tangent.x);
			normalize(vertex.Bitangent);
		}
	};

	auto flushMesh = [&](ObjMeshBuilder& builder, std::vector<MeshComponent>& meshes) {
		if (builder.indices.empty() || builder.vertices.empty()) {
			builder = ObjMeshBuilder{};
			return;
		}

		if (builder.vertices.size() > static_cast<size_t>(std::numeric_limits<int>::max()) ||
			builder.indices.size() > static_cast<size_t>(std::numeric_limits<int>::max())) {
			ERROR("ModelLoader", "LoadOBJModel", "OBJ mesh is too large for the engine buffer counters.");
			builder = ObjMeshBuilder{};
			return;
		}

		MeshComponent mesh;
		mesh.m_name = builder.name;
		mesh.m_materialName = builder.materialName;
		mesh.m_vertex = std::move(builder.vertices);
		mesh.m_index = std::move(builder.indices);
		mesh.m_numVertex = static_cast<int>(mesh.m_vertex.size());
		mesh.m_numIndex = static_cast<int>(mesh.m_index.size());
		computeTangents(mesh);
		meshes.push_back(std::move(mesh));
		builder = ObjMeshBuilder{};
	};

	std::ifstream file(filePath);
	if (!file.is_open()) {
		ERROR("ModelLoader", "LoadOBJModel", ("Unable to open OBJ file: " + filePath).c_str());
		return {};
	}

	std::vector<EU::Vector3> positions;
	std::vector<EU::Vector2> texcoords;
	std::vector<EU::Vector3> normals;
	std::vector<MeshComponent> loadedMeshes;
	ObjMeshBuilder currentMesh;
	std::string currentGroupName = "default";
	std::string currentMaterialName = "default";
	currentMesh.name = currentGroupName;
	currentMesh.materialName = currentMaterialName;

	std::string line;
	while (std::getline(file, line)) {
		if (line.empty() || line[0] == '#') {
			continue;
		}

		std::istringstream stream(line);
		std::string command;
		stream >> command;

		if (command == "v") {
			EU::Vector3 position{};
			if ((stream >> position.x >> position.y >> position.z) &&
				std::isfinite(position.x) && std::isfinite(position.y) && std::isfinite(position.z)) {
				if (positions.size() >= static_cast<size_t>(std::numeric_limits<int>::max())) {
					ERROR("ModelLoader", "LoadOBJModel", "OBJ contains too many positions.");
					return {};
				}
				positions.push_back(position);
			}
		}
		else if (command == "vt") {
			EU::Vector2 uv{};
			if ((stream >> uv.x >> uv.y) && std::isfinite(uv.x) && std::isfinite(uv.y)) {
				if (texcoords.size() >= static_cast<size_t>(std::numeric_limits<int>::max())) {
					ERROR("ModelLoader", "LoadOBJModel", "OBJ contains too many texture coordinates.");
					return {};
				}
				uv.y = 1.0f - uv.y;
				texcoords.push_back(uv);
			}
		}
		else if (command == "vn") {
			EU::Vector3 normal{};
			if ((stream >> normal.x >> normal.y >> normal.z) &&
				std::isfinite(normal.x) && std::isfinite(normal.y) && std::isfinite(normal.z)) {
				if (normals.size() >= static_cast<size_t>(std::numeric_limits<int>::max())) {
					ERROR("ModelLoader", "LoadOBJModel", "OBJ contains too many normals.");
					return {};
				}
				normalize(normal);
				normals.push_back(normal);
			}
		}
		else if (command == "g" || command == "o") {
			flushMesh(currentMesh, loadedMeshes);
			stream >> currentGroupName;
			if (currentGroupName.empty()) {
				currentGroupName = "default";
			}
			currentMesh.name = currentGroupName;
			currentMesh.materialName = currentMaterialName;
		}
		else if (command == "usemtl") {
			if (!currentMesh.indices.empty()) {
				flushMesh(currentMesh, loadedMeshes);
			}
			std::getline(stream, currentMaterialName);
			const size_t first = currentMaterialName.find_first_not_of(" \t\r\n");
			const size_t last = currentMaterialName.find_last_not_of(" \t\r\n");
			if (first == std::string::npos) {
				currentMaterialName = "default";
			}
			else {
				currentMaterialName = currentMaterialName.substr(first, last - first + 1);
			}
			currentMesh.name = currentGroupName;
			currentMesh.materialName = currentMaterialName;
		}
		else if (command == "f") {
			std::vector<unsigned int> polygonIndices;
			std::string token;
			while (stream >> token) {
				const ObjIndex objIndex = parseFaceVertex(
					token,
					static_cast<int>(positions.size()),
					static_cast<int>(texcoords.size()),
					static_cast<int>(normals.size()));
				if (objIndex.position < 0 || objIndex.position >= static_cast<int>(positions.size())) {
					polygonIndices.clear();
					break;
				}

				auto it = currentMesh.vertexLookup.find(objIndex);
				if (it == currentMesh.vertexLookup.end()) {
					SimpleVertex vertex{};
					if (objIndex.position >= 0 && objIndex.position < static_cast<int>(positions.size())) {
						vertex.Position = positions[objIndex.position];
					}
					if (objIndex.texcoord >= 0 && objIndex.texcoord < static_cast<int>(texcoords.size())) {
						vertex.TextureCoordinate = texcoords[objIndex.texcoord];
					}
					else {
						vertex.TextureCoordinate = EU::Vector2(0.0f, 0.0f);
					}
					if (objIndex.normal >= 0 && objIndex.normal < static_cast<int>(normals.size())) {
						vertex.Normal = normals[objIndex.normal];
					}
					else {
						// Leave missing normals at zero so computeTangents can derive them from geometry.
						vertex.Normal = EU::Vector3(0.0f, 0.0f, 0.0f);
					}
					vertex.Tangent = EU::Vector3(0.0f, 0.0f, 0.0f);
					vertex.Bitangent = EU::Vector3(0.0f, 0.0f, 0.0f);

					if (currentMesh.vertices.size() >= static_cast<size_t>(std::numeric_limits<unsigned int>::max())) {
						polygonIndices.clear();
						break;
					}
					const unsigned int newIndex = static_cast<unsigned int>(currentMesh.vertices.size());
					currentMesh.vertices.push_back(vertex);
					currentMesh.vertexLookup[objIndex] = newIndex;
					polygonIndices.push_back(newIndex);
				}
				else {
					polygonIndices.push_back(it->second);
				}
			}

			for (size_t i = 1; i + 1 < polygonIndices.size(); ++i) {
				if (currentMesh.indices.size() > static_cast<size_t>(std::numeric_limits<int>::max()) - 3u) {
					ERROR("ModelLoader", "LoadOBJModel", "OBJ contains too many indices.");
					return {};
				}
				currentMesh.indices.push_back(polygonIndices[0]);
				currentMesh.indices.push_back(polygonIndices[i]);
				currentMesh.indices.push_back(polygonIndices[i + 1]);
			}
		}
	}

	flushMesh(currentMesh, loadedMeshes);
	return loadedMeshes;
}

void 
Model3D::ProcessFBXNode(FbxNode* node) {
	if (!node) {
		return;
	}

	for (int materialIndex = 0; materialIndex < node->GetMaterialCount(); ++materialIndex) {
		ProcessFBXMaterials(node->GetMaterial(materialIndex));
	}

	if (node->GetNodeAttribute()) {
		if (node->GetNodeAttribute()->GetAttributeType() == FbxNodeAttribute::eMesh) {
			ProcessFBXMesh(node);
		}
	}

	for (int i = 0; i < node->GetChildCount(); i++) {
		ProcessFBXNode(node->GetChild(i));
	}
}

void 
Model3D::ProcessFBXMesh(FbxNode* node) {
  FbxMesh* mesh = node->GetMesh();
  if (!mesh) return;

  const int polygonCount = mesh->GetPolygonCount();
  const int controlPointCount = mesh->GetControlPointsCount();
  if (polygonCount <= 0 || controlPointCount <= 0 ||
      polygonCount > (std::numeric_limits<int>::max() / 3)) {
    return;
  }

  if (mesh->GetElementNormalCount() == 0)
    mesh->GenerateNormals(true, true);

  const char* uvSetName = nullptr;
  {
    FbxStringList uvSets; mesh->GetUVSetNames(uvSets);
    if (uvSets.GetCount() > 0) uvSetName = uvSets[0];
  }

  if (mesh->GetElementTangentCount() == 0 && uvSetName)
    mesh->GenerateTangentsData(uvSetName);

  const FbxGeometryElementUV* uvElem = (mesh->GetElementUVCount() > 0) ? mesh->GetElementUV(0) : nullptr;
  const FbxGeometryElementTangent* tanElem = (mesh->GetElementTangentCount() > 0) ? mesh->GetElementTangent(0) : nullptr;
  const FbxGeometryElementBinormal* binElem = (mesh->GetElementBinormalCount() > 0) ? mesh->GetElementBinormal(0) : nullptr;

  std::vector<SimpleVertex> vertices;
  std::vector<unsigned int> indices;
  const size_t estimatedCornerCount = static_cast<size_t>(polygonCount) * 3u;
  vertices.reserve(estimatedCornerCount);
  indices.reserve(estimatedCornerCount);

  auto resolveElementIndex = [](auto* elem, int cpIdx, int pvIdx) -> int {
    if (!elem) return -1;
    using E = FbxGeometryElement;
    int sourceIndex = -1;
    switch (elem->GetMappingMode()) {
    case E::eByControlPoint: sourceIndex = cpIdx; break;
    case E::eByPolygonVertex: sourceIndex = pvIdx; break;
    default: return -1;
    }
    if (sourceIndex < 0) return -1;

    int directIndex = sourceIndex;
    if (elem->GetReferenceMode() == E::eIndexToDirect) {
      if (sourceIndex >= elem->GetIndexArray().GetCount()) return -1;
      directIndex = elem->GetIndexArray().GetAt(sourceIndex);
    }
    else if (elem->GetReferenceMode() != E::eDirect) {
      return -1;
    }
    return (directIndex >= 0 && directIndex < elem->GetDirectArray().GetCount()) ? directIndex : -1;
    };

  auto readV2 = [&resolveElementIndex](const FbxGeometryElementUV* elem, int cpIdx, int pvIdx) -> FbxVector2 {
    const int idx = resolveElementIndex(elem, cpIdx, pvIdx);
    return (idx >= 0) ? elem->GetDirectArray().GetAt(idx) : FbxVector2(0, 0);
    };
  auto readV4 = [&resolveElementIndex](auto* elem, int cpIdx, int pvIdx) -> FbxVector4 {
    const int idx = resolveElementIndex(elem, cpIdx, pvIdx);
    return (idx >= 0) ? elem->GetDirectArray().GetAt(idx) : FbxVector4(0, 0, 0, 0);
    };

  for (int p = 0; p < polygonCount; ++p)
  {
    const int polySize = mesh->GetPolygonSize(p);
    if (polySize < 3) continue;
    std::vector<unsigned> cornerIdx; cornerIdx.reserve(polySize);
    bool polygonValid = true;

    for (int v = 0; v < polySize; ++v)
    {
      const int cpIndex = mesh->GetPolygonVertex(p, v);
      const int pvIndex = mesh->GetPolygonVertexIndex(p) + v;
      if (cpIndex < 0 || cpIndex >= controlPointCount) {
        polygonValid = false;
        break;
      }

      SimpleVertex out{};

      FbxVector4 P = mesh->GetControlPointAt(cpIndex);
      out.Position = { (float)P[0], (float)P[1], (float)P[2] };

      FbxVector4 N(0, 1, 0, 0);
      mesh->GetPolygonVertexNormal(p, v, N);
      if (N.SquareLength() > 1e-16) {
        N.Normalize();
      }
      else {
        N = FbxVector4(0, 1, 0, 0);
      }
      out.Normal = { (float)N[0], (float)N[1], (float)N[2] };

      if (uvElem && uvSetName) {
        int uvIdx = mesh->GetTextureUVIndex(p, v);
        FbxVector2 uv = (uvIdx >= 0 && uvIdx < uvElem->GetDirectArray().GetCount())
          ? uvElem->GetDirectArray().GetAt(uvIdx)
          : readV2(uvElem, cpIndex, pvIndex);
        out.TextureCoordinate = { (float)uv[0], 1.0f - (float)uv[1] };
      }
      else {
        out.TextureCoordinate = { 0.0f, 0.0f };
      }

      if (tanElem) {
        FbxVector4 T = readV4(tanElem, cpIndex, pvIndex);
        out.Tangent = { (float)T[0], (float)T[1], (float)T[2] };
      }
      else out.Tangent = { 0,0,0 };
      
      if (binElem) {
        FbxVector4 B = readV4(binElem, cpIndex, pvIndex);
        out.Bitangent = { (float)B[0], (float)B[1], (float)B[2] };
      }
      else out.Bitangent = { 0,0,0 };

      if (vertices.size() >= static_cast<size_t>(std::numeric_limits<unsigned int>::max())) {
        polygonValid = false;
        break;
      }
      cornerIdx.push_back(static_cast<unsigned int>(vertices.size()));
      vertices.push_back(out);
    }

    if (!polygonValid || cornerIdx.size() != static_cast<size_t>(polySize)) {
      vertices.resize(vertices.size() - cornerIdx.size());
      continue;
    }

    for (int k = 1; k + 1 < polySize; ++k) {
      indices.push_back(cornerIdx[0]);
      indices.push_back(cornerIdx[k + 1]);
      indices.push_back(cornerIdx[k]);
    }
  }

  if (mesh->GetElementTangentCount() == 0 || mesh->GetElementBinormalCount() == 0)
  {
    auto add = [](EU::Vector3 a, const EU::Vector3& b) { a.x += b.x; a.y += b.y; a.z += b.z; return a; };
    auto sub = [](const EU::Vector3& a, const EU::Vector3& b) { return EU::Vector3(a.x - b.x, a.y - b.y, a.z - b.z); };
    auto mul = [](const EU::Vector3& a, float s) { return EU::Vector3(a.x * s, a.y * s, a.z * s); };
  
    for (size_t i = 0; i + 2 < indices.size(); i += 3)
    {
      SimpleVertex& v0 = vertices[indices[i + 0]];
      SimpleVertex& v1 = vertices[indices[i + 1]];
      SimpleVertex& v2 = vertices[indices[i + 2]];
  
      EU::Vector3 e1 = sub(v1.Position, v0.Position);
      EU::Vector3 e2 = sub(v2.Position, v0.Position);
  
      float du1 = v1.TextureCoordinate.x - v0.TextureCoordinate.x;
      float dv1 = v1.TextureCoordinate.y - v0.TextureCoordinate.y;
      float du2 = v2.TextureCoordinate.x - v0.TextureCoordinate.x;
      float dv2 = v2.TextureCoordinate.y - v0.TextureCoordinate.y;
  
      float denom = du1 * dv2 - du2 * dv1;
      float r = (std::fabs(denom) < 1e-8f) ? 0.0f : 1.0f / denom;
  
      EU::Vector3 T = mul(EU::Vector3(e1.x * dv2 - e2.x * dv1, e1.y * dv2 - e2.y * dv1, e1.z * dv2 - e2.z * dv1), r);
      EU::Vector3 B = mul(EU::Vector3(e2.x * du1 - e1.x * du2, e2.y * du1 - e1.y * du2, e2.z * du1 - e1.z * du2), r);
  
      v0.Tangent = add(v0.Tangent, T);
      v1.Tangent = add(v1.Tangent, T);
      v2.Tangent = add(v2.Tangent, T);
      v0.Bitangent = add(v0.Bitangent, B);
      v1.Bitangent = add(v1.Bitangent, B);
      v2.Bitangent = add(v2.Bitangent, B);
    }
  }

  const bool autoDetectMirror = true;
  const bool forceFlipWinding = false;

  bool mirrored = true;
  if (autoDetectMirror) {
    FbxAMatrix geo;
    geo.SetT(node->GetGeometricTranslation(FbxNode::eSourcePivot));
    geo.SetR(node->GetGeometricRotation(FbxNode::eSourcePivot));
    geo.SetS(node->GetGeometricScaling(FbxNode::eSourcePivot));
    FbxAMatrix world = node->EvaluateGlobalTransform() * geo;

    FbxVector4 S = world.GetS();
    double detScale = S[0] * S[1] * S[2];
    mirrored = (detScale < 0.0);
  }

  if (mirrored || forceFlipWinding) {
    for (size_t i = 0; i + 2 < indices.size(); i += 3)
      std::swap(indices[i + 1], indices[i + 2]);

    for (auto& v : vertices) {
      v.Normal = { v.Normal.x, v.Normal.y, v.Normal.z };
      v.Tangent = { v.Tangent.x, v.Tangent.y, v.Tangent.z };
      v.Bitangent = { v.Bitangent.x, v.Bitangent.y, v.Bitangent.z };
    }
  }

  auto dot3 = [](const EU::Vector3& a, const EU::Vector3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; };
  auto lengthSq3 = [](const EU::Vector3& v) { return v.x * v.x + v.y * v.y + v.z * v.z; };
  auto norm3 = [&](EU::Vector3& v, const EU::Vector3& fallback) {
    const float lengthSq = lengthSq3(v);
    if (lengthSq <= 1e-20f) {
      v = fallback;
      return;
    }
    const float invLength = 1.0f / std::sqrt(lengthSq);
    v.x *= invLength;
    v.y *= invLength;
    v.z *= invLength;
  };
  auto sub3 = [](const EU::Vector3& a, const EU::Vector3& b) { return EU::Vector3(a.x - b.x, a.y - b.y, a.z - b.z); };
  auto cross3 = [](const EU::Vector3& a, const EU::Vector3& b) {
    return EU::Vector3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x);
    };
  
  for (auto& v : vertices)
  {
    norm3(v.Normal, EU::Vector3(0.0f, 1.0f, 0.0f));
    const float dTN = dot3(v.Tangent, v.Normal);
    v.Tangent = sub3(v.Tangent, EU::Vector3(v.Normal.x * dTN, v.Normal.y * dTN, v.Normal.z * dTN));

    if (lengthSq3(v.Tangent) <= 1e-20f) {
      const EU::Vector3 reference = std::fabs(v.Normal.y) < 0.999f
        ? EU::Vector3(0.0f, 1.0f, 0.0f)
        : EU::Vector3(1.0f, 0.0f, 0.0f);
      v.Tangent = cross3(reference, v.Normal);
    }
    norm3(v.Tangent, EU::Vector3(1.0f, 0.0f, 0.0f));
  
    EU::Vector3 Bcalc = cross3(v.Normal, v.Tangent);
    const float hand = (dot3(Bcalc, v.Bitangent) < 0.0f) ? -1.0f : 1.0f;
    v.Bitangent = { Bcalc.x * hand, Bcalc.y * hand, Bcalc.z * hand };
    norm3(v.Bitangent, EU::Vector3(0.0f, 0.0f, 1.0f));
  }

  if (vertices.empty() || indices.empty() || (indices.size() % 3u) != 0u ||
      vertices.size() > static_cast<size_t>(std::numeric_limits<int>::max()) ||
      indices.size() > static_cast<size_t>(std::numeric_limits<int>::max())) {
    return;
  }

  MeshComponent mc;
  mc.m_name = node->GetName();
  mc.m_vertex = std::move(vertices);
  mc.m_index = std::move(indices);
  mc.m_numVertex = (int)mc.m_vertex.size();
  mc.m_numIndex = (int)mc.m_index.size();

  // FBX matrices use a column-vector convention. Transpose into the
  // row-vector convention used by XNAMath/Direct3D in the engine.
  FbxAMatrix geometricTransform;
  geometricTransform.SetT(node->GetGeometricTranslation(FbxNode::eSourcePivot));
  geometricTransform.SetR(node->GetGeometricRotation(FbxNode::eSourcePivot));
  geometricTransform.SetS(node->GetGeometricScaling(FbxNode::eSourcePivot));
  const FbxAMatrix globalTransform = node->EvaluateGlobalTransform() * geometricTransform;
  mc.m_localTransform._11 = static_cast<float>(globalTransform.Get(0, 0));
  mc.m_localTransform._12 = static_cast<float>(globalTransform.Get(1, 0));
  mc.m_localTransform._13 = static_cast<float>(globalTransform.Get(2, 0));
  mc.m_localTransform._14 = static_cast<float>(globalTransform.Get(3, 0));
  mc.m_localTransform._21 = static_cast<float>(globalTransform.Get(0, 1));
  mc.m_localTransform._22 = static_cast<float>(globalTransform.Get(1, 1));
  mc.m_localTransform._23 = static_cast<float>(globalTransform.Get(2, 1));
  mc.m_localTransform._24 = static_cast<float>(globalTransform.Get(3, 1));
  mc.m_localTransform._31 = static_cast<float>(globalTransform.Get(0, 2));
  mc.m_localTransform._32 = static_cast<float>(globalTransform.Get(1, 2));
  mc.m_localTransform._33 = static_cast<float>(globalTransform.Get(2, 2));
  mc.m_localTransform._34 = static_cast<float>(globalTransform.Get(3, 2));
  mc.m_localTransform._41 = static_cast<float>(globalTransform.Get(0, 3));
  mc.m_localTransform._42 = static_cast<float>(globalTransform.Get(1, 3));
  mc.m_localTransform._43 = static_cast<float>(globalTransform.Get(2, 3));
  mc.m_localTransform._44 = static_cast<float>(globalTransform.Get(3, 3));
  m_meshes.push_back(std::move(mc));
}

void Model3D::ProcessFBXMaterials(FbxSurfaceMaterial* material)
{
	if (!material) {
		return;
	}

	FbxProperty prop = material->FindProperty(FbxSurfaceMaterial::sDiffuse);
	if (!prop.IsValid()) {
		return;
	}

	const int textureCount = prop.GetSrcObjectCount<FbxTexture>();
	for (int i = 0; i < textureCount; ++i) {
		FbxTexture* texture = FbxCast<FbxTexture>(prop.GetSrcObject<FbxTexture>(i));
		if (!texture) {
			continue;
		}

		std::string textureName;
		if (FbxFileTexture* fileTexture = FbxCast<FbxFileTexture>(texture)) {
			const char* fileName = fileTexture->GetFileName();
			if (fileName && *fileName) {
				textureName = fileName;
			}
		}
		if (textureName.empty()) {
			const char* objectName = texture->GetName();
			if (objectName && *objectName) {
				textureName = objectName;
			}
		}

		if (!textureName.empty() &&
			std::find(textureFileNames.begin(), textureFileNames.end(), textureName) == textureFileNames.end()) {
			textureFileNames.push_back(std::move(textureName));
		}
	}
}

std::string
Model3D::GetBinaryCachePath() const {
	return m_filePath + ".wvmesh";
}

bool
Model3D::IsBinaryCacheUpToDate(const std::string& sourcePath, const std::string& cachePath) const {
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

bool
Model3D::LoadBinaryCache(const std::string& cachePath) {
	std::ifstream stream(cachePath, std::ios::binary);
	if (!stream.is_open()) {
		return false;
	}

	uint32_t magic = 0;
	uint32_t version = 0;
	uint32_t meshCount = 0;
	uint32_t textureCount = 0;

	stream.read(reinterpret_cast<char*>(&magic), sizeof(magic));
	stream.read(reinterpret_cast<char*>(&version), sizeof(version));
	stream.read(reinterpret_cast<char*>(&meshCount), sizeof(meshCount));
	stream.read(reinterpret_cast<char*>(&textureCount), sizeof(textureCount));

	if (!stream.good() || magic != kModelCacheMagic || version != kModelCacheVersion ||
		meshCount > kMaxCachedMeshes || textureCount > kMaxCachedTextures) {
		return false;
	}

	// Cada textura necesita al menos su longitud (uint32_t) y cada mesh al menos
	// su nombre. Esta comprobacion evita reserve() absurdos en caches corruptos.
	const uint64_t minimumRecordBytes =
		(static_cast<uint64_t>(meshCount) + static_cast<uint64_t>(textureCount)) * sizeof(uint32_t);
	if (!HasRemainingBytes(stream, minimumRecordBytes)) {
		return false;
	}

	std::vector<MeshComponent> loadedMeshes;
	std::vector<std::string> loadedTextures;
	loadedMeshes.reserve(meshCount);
	loadedTextures.reserve(textureCount);

	for (uint32_t i = 0; i < textureCount; ++i) {
		std::string textureName;
		if (!ReadString(stream, textureName)) {
			return false;
		}
		loadedTextures.push_back(std::move(textureName));
	}

	for (uint32_t i = 0; i < meshCount; ++i) {
		MeshComponent mesh;
		if (!ReadString(stream, mesh.m_name) || !ReadString(stream, mesh.m_materialName)) {
			return false;
		}

		uint32_t vertexCount = 0;
		uint32_t indexCount = 0;
		stream.read(reinterpret_cast<char*>(&vertexCount), sizeof(vertexCount));
		stream.read(reinterpret_cast<char*>(&indexCount), sizeof(indexCount));
		if (!stream.good() ||
			vertexCount == 0 || indexCount == 0 ||
			vertexCount > static_cast<uint32_t>(std::numeric_limits<int>::max()) ||
			indexCount > static_cast<uint32_t>(std::numeric_limits<int>::max())) {
			return false;
		}

		const uint64_t vertexBytes = static_cast<uint64_t>(vertexCount) * sizeof(SimpleVertex);
		const uint64_t indexBytes = static_cast<uint64_t>(indexCount) * sizeof(unsigned int);
		const uint64_t payloadBytes = sizeof(mesh.m_localTransform) + vertexBytes + indexBytes;
		if (vertexBytes > static_cast<uint64_t>(std::numeric_limits<std::streamsize>::max()) ||
			indexBytes > static_cast<uint64_t>(std::numeric_limits<std::streamsize>::max()) ||
			!HasRemainingBytes(stream, payloadBytes)) {
			return false;
		}

		stream.read(reinterpret_cast<char*>(&mesh.m_localTransform), sizeof(mesh.m_localTransform));
		if (!stream.good()) {
			return false;
		}

		mesh.m_vertex.resize(vertexCount);
		mesh.m_index.resize(indexCount);
		if (vertexCount > 0) {
			stream.read(reinterpret_cast<char*>(mesh.m_vertex.data()), static_cast<std::streamsize>(vertexBytes));
		}
		if (indexCount > 0) {
			stream.read(reinterpret_cast<char*>(mesh.m_index.data()), static_cast<std::streamsize>(indexBytes));
		}
		if (!stream.good()) {
			return false;
		}

		for (const unsigned int index : mesh.m_index) {
			if (index >= vertexCount) {
				return false;
			}
		}

		mesh.m_numVertex = static_cast<int>(vertexCount);
		mesh.m_numIndex = static_cast<int>(indexCount);
		loadedMeshes.push_back(std::move(mesh));
	}

	m_meshes = std::move(loadedMeshes);
	textureFileNames = std::move(loadedTextures);

	const std::wstring cachePathW(cachePath.begin(), cachePath.end());
	MESSAGE("ModelLoader", "BinaryCache",
		L"Loaded binary cache '" << cachePathW << L"'");
	return true;
}

bool
Model3D::SaveBinaryCache(const std::string& cachePath) const {
	std::ofstream stream(cachePath, std::ios::binary | std::ios::trunc);
	if (!stream.is_open()) {
		return false;
	}

	if (m_meshes.size() > kMaxCachedMeshes ||
		textureFileNames.size() > kMaxCachedTextures ||
		m_meshes.size() > std::numeric_limits<uint32_t>::max() ||
		textureFileNames.size() > std::numeric_limits<uint32_t>::max()) {
		return false;
	}

	const uint32_t meshCount = static_cast<uint32_t>(m_meshes.size());
	const uint32_t textureCount = static_cast<uint32_t>(textureFileNames.size());

	stream.write(reinterpret_cast<const char*>(&kModelCacheMagic), sizeof(kModelCacheMagic));
	stream.write(reinterpret_cast<const char*>(&kModelCacheVersion), sizeof(kModelCacheVersion));
	stream.write(reinterpret_cast<const char*>(&meshCount), sizeof(meshCount));
	stream.write(reinterpret_cast<const char*>(&textureCount), sizeof(textureCount));

	for (const std::string& textureName : textureFileNames) {
		if (!WriteString(stream, textureName)) {
			return false;
		}
	}

	for (const MeshComponent& mesh : m_meshes) {
		if (!WriteString(stream, mesh.m_name) || !WriteString(stream, mesh.m_materialName)) {
			return false;
		}

		if (mesh.m_vertex.empty() || mesh.m_index.empty() ||
			mesh.m_vertex.size() > static_cast<size_t>(std::numeric_limits<uint32_t>::max()) ||
			mesh.m_index.size() > static_cast<size_t>(std::numeric_limits<uint32_t>::max()) ||
			mesh.m_vertex.size() > static_cast<size_t>(std::numeric_limits<int>::max()) ||
			mesh.m_index.size() > static_cast<size_t>(std::numeric_limits<int>::max())) {
			return false;
		}

		for (const unsigned int index : mesh.m_index) {
			if (index >= mesh.m_vertex.size()) {
				return false;
			}
		}

		const uint32_t vertexCount = static_cast<uint32_t>(mesh.m_vertex.size());
		const uint32_t indexCount = static_cast<uint32_t>(mesh.m_index.size());
		const uint64_t vertexBytes = static_cast<uint64_t>(vertexCount) * sizeof(SimpleVertex);
		const uint64_t indexBytes = static_cast<uint64_t>(indexCount) * sizeof(unsigned int);
		if (vertexBytes > static_cast<uint64_t>(std::numeric_limits<std::streamsize>::max()) ||
			indexBytes > static_cast<uint64_t>(std::numeric_limits<std::streamsize>::max())) {
			return false;
		}

		stream.write(reinterpret_cast<const char*>(&vertexCount), sizeof(vertexCount));
		stream.write(reinterpret_cast<const char*>(&indexCount), sizeof(indexCount));
		stream.write(reinterpret_cast<const char*>(&mesh.m_localTransform), sizeof(mesh.m_localTransform));

		if (vertexCount > 0) {
			stream.write(reinterpret_cast<const char*>(mesh.m_vertex.data()), static_cast<std::streamsize>(vertexBytes));
		}
		if (indexCount > 0) {
			stream.write(reinterpret_cast<const char*>(mesh.m_index.data()), static_cast<std::streamsize>(indexBytes));
		}

		if (!stream.good()) {
			return false;
		}
	}

	return stream.good();
}


