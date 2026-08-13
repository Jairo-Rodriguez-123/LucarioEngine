/**
 * @file BaseApp.cpp
 * @brief Implementa la logica de BaseApp dentro del subsistema Core.
 * @ingroup core
 */
#include "BaseApp.h"
#include "ResourceManager.h"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <filesystem>
#include <cmath>
#include <limits>
#include <commdlg.h>
#include <shellapi.h>
#include <functional>
#include <cstdio>
#include <cstring>

#if defined(_MSC_VER)
#pragma comment(lib, "Comdlg32.lib")
#pragma comment(lib, "Shell32.lib")
#endif

namespace {
	constexpr int kCurrentSceneVersion = 6;
	constexpr const char* kDefaultSkyboxPath = "Assets/Skyboxes/Wildvine_Dusk.png";
	constexpr size_t kMaxSerializedActors = 10000;
	constexpr size_t kMaxSerializedMaterialsPerActor = 1024;

	bool isSerializedLightActorName(const std::string& actorName)
	{
		return actorName.rfind("Light Actor", 0) == 0;
	}

	bool isFinite(float value)
	{
		return std::isfinite(value);
	}

	bool isFinite(const EU::Vector3& value)
	{
		return isFinite(value.x) && isFinite(value.y) && isFinite(value.z);
	}

	bool isFinite(const XMFLOAT4& value)
	{
		return isFinite(value.x) && isFinite(value.y) &&
			isFinite(value.z) && isFinite(value.w);
	}

	bool areFinite(const MaterialParams& params)
	{
		return isFinite(params.baseColor) &&
			isFinite(params.metallic) &&
			isFinite(params.roughness) &&
			isFinite(params.ao) &&
			isFinite(params.normalScale) &&
			isFinite(params.emissiveStrength) &&
			isFinite(params.alphaCutoff);
	}

	std::string lowerAscii(std::string value)
	{
		std::transform(value.begin(), value.end(), value.begin(),
			[](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		return value;
	}

	std::filesystem::path getExecutableDirectory()
	{
		char modulePath[32768] = {};
		const DWORD length = GetModuleFileNameA(nullptr, modulePath, static_cast<DWORD>(sizeof(modulePath)));
		if (length == 0 || length >= sizeof(modulePath)) {
			return std::filesystem::path();
		}
		return std::filesystem::path(modulePath).parent_path();
	}

	void appendSearchRoot(std::vector<std::filesystem::path>& roots, const std::filesystem::path& root)
	{
		if (root.empty()) return;
		const std::filesystem::path normalized = root.lexically_normal();
		for (const auto& existing : roots) {
			if (lowerAscii(existing.generic_string()) == lowerAscii(normalized.generic_string())) {
				return;
			}
		}
		roots.push_back(normalized);
	}

	std::vector<std::filesystem::path> getAssetSearchRoots()
	{
		namespace fs = std::filesystem;
		std::vector<fs::path> roots;
		std::error_code ec;
		const fs::path cwd = fs::current_path(ec);
		if (!ec) {
			fs::path current = cwd;
			for (int i = 0; i < 5 && !current.empty(); ++i) {
				appendSearchRoot(roots, current);
				const fs::path parent = current.parent_path();
				if (parent == current) break;
				current = parent;
			}
		}

		fs::path executableDir = getExecutableDirectory();
		for (int i = 0; i < 6 && !executableDir.empty(); ++i) {
			appendSearchRoot(roots, executableDir);
			const fs::path parent = executableDir.parent_path();
			if (parent == executableDir) break;
			executableDir = parent;
		}
		return roots;
	}

	bool isUsableOBJFile(const std::filesystem::path& path)
	{
		std::error_code ec;
		if (!std::filesystem::exists(path, ec) || ec || !std::filesystem::is_regular_file(path, ec) || ec) {
			return false;
		}
		return lowerAscii(path.extension().string()) == ".obj";
	}

	bool resolveOBJAssetPath(const std::string& serializedPath, const std::string& actorName, std::filesystem::path& resolvedPath)
	{
		namespace fs = std::filesystem;
		resolvedPath.clear();
		const fs::path requested(serializedPath);

		if (!requested.empty() && requested.is_absolute() && isUsableOBJFile(requested)) {
			resolvedPath = requested.lexically_normal();
			return true;
		}

		std::vector<fs::path> candidates;
		const auto roots = getAssetSearchRoots();
		if (!requested.empty()) {
			if (isUsableOBJFile(requested)) {
				resolvedPath = requested.lexically_normal();
				return true;
			}
			for (const fs::path& root : roots) {
				candidates.push_back(root / requested);
			}
		}

		fs::path filename = requested.filename();
		if (filename.empty() && !actorName.empty()) {
			filename = fs::path(actorName + ".obj");
		}
		if (!filename.empty()) {
			for (const fs::path& root : roots) {
				candidates.push_back(root / "Assets" / "Models" / filename);
			}
		}

		if (!actorName.empty()) {
			const fs::path actorFilename(actorName + ".obj");
			for (const fs::path& root : roots) {
				candidates.push_back(root / "Assets" / "Models" / actorFilename);
			}
		}

		for (const fs::path& candidate : candidates) {
			if (isUsableOBJFile(candidate)) {
				resolvedPath = candidate.lexically_normal();
				return true;
			}
		}
		return false;
	}

	std::string makePortableAssetPath(const std::filesystem::path& sourcePath)
	{
		namespace fs = std::filesystem;
		std::error_code ec;
		fs::path absolutePath = fs::absolute(sourcePath, ec);
		if (ec) absolutePath = sourcePath;
		absolutePath = absolutePath.lexically_normal();

		for (auto it = absolutePath.begin(); it != absolutePath.end(); ++it) {
			if (lowerAscii(it->string()) == "assets") {
				fs::path portable;
				for (auto jt = it; jt != absolutePath.end(); ++jt) {
					portable /= *jt;
				}
				return portable.generic_string();
			}
		}
		return absolutePath.string();
	}

	void ensureDefaultLightComponent(const EU::TSharedPointer<Actor>& actor)
	{
		if (actor.isNull()) {
			return;
		}

		EU::TSharedPointer<LightComponent> lightComponent = actor->getComponent<LightComponent>();
		if (!lightComponent) {
			lightComponent = EU::MakeShared<LightComponent>();
			actor->addComponent(lightComponent);
		}

		LightData& light = lightComponent->getLightData();
		light.type = LightType::Directional;
		light.color = EU::Vector3(1.0f, 1.0f, 1.0f);
		light.intensity = 1.0f;
		light.direction = EU::Vector3(-0.20f, -1.0f, 1.0f);
		light.range = 12.0f;
		light.spotAngle = 0.0f;
		lightComponent->setCastShadow(true);
	}

	bool resolveOptionalAssetPath(const std::string& relativePath, std::string& resolvedPath)
	{
		namespace fs = std::filesystem;
		const fs::path requested(relativePath);
		const std::array<fs::path, 7> candidates = {
			requested,
			fs::path("Resource Files") / requested,
			fs::path("..") / requested,
			fs::path("..") / "Resource Files" / requested,
			fs::path("..") / ".." / requested,
			fs::path("..") / ".." / "Resource Files" / requested,
			fs::path("..") / ".." / ".." / requested
		};

		std::error_code ec;
		for (const fs::path& candidate : candidates) {
			ec.clear();
			if (fs::exists(candidate, ec) && fs::is_regular_file(candidate, ec)) {
				resolvedPath = candidate.lexically_normal().string();
				return true;
			}
		}
		resolvedPath.clear();
		return false;
	}

	struct ObjMaterialInfo {
		std::string name;
		XMFLOAT4 baseColor = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
		EU::Vector3 emissiveColor = EU::Vector3(0.0f, 0.0f, 0.0f);
		float metallic = 0.0f;
		float roughness = 0.55f;
		float ao = 1.0f;
		float normalScale = 1.0f;
		bool hasMetallicValue = false;
		bool hasRoughnessValue = false;
		std::string albedoMap;
		std::string normalMap;
		std::string metallicMap;
		std::string roughnessMap;
		std::string aoMap;
		std::string emissiveMap;
		std::filesystem::path sourceDirectory;
	};

	std::string trimAscii(const std::string& value)
	{
		const size_t first = value.find_first_not_of(" \t\r\n");
		if (first == std::string::npos) return std::string();
		const size_t last = value.find_last_not_of(" \t\r\n");
		return value.substr(first, last - first + 1);
	}

	std::string stripOptionalQuotes(std::string value)
	{
		value = trimAscii(value);
		if (value.size() >= 2 &&
			((value.front() == '"' && value.back() == '"') ||
			 (value.front() == '\'' && value.back() == '\''))) {
			value = value.substr(1, value.size() - 2);
		}
		return value;
	}

	std::string parseMtlMapFilename(const std::string& remainder)
	{
		std::string value = trimAscii(remainder);
		if (value.empty()) return std::string();

		// Quoted filenames are unambiguous and may contain spaces.
		const size_t firstQuote = value.find('"');
		if (firstQuote != std::string::npos) {
			const size_t secondQuote = value.find('"', firstQuote + 1);
			if (secondQuote != std::string::npos && secondQuote > firstQuote + 1) {
				return value.substr(firstQuote + 1, secondQuote - firstQuote - 1);
			}
		}

		// MTL texture options (-s, -o, -bm, etc.) precede the filename.
		// Taking the final token handles the common exporter output safely.
		std::istringstream tokens(value);
		std::string token;
		std::string lastToken;
		while (tokens >> token) {
			lastToken = token;
		}
		return stripOptionalQuotes(lastToken);
	}

	float parseMtlBumpScale(const std::string& remainder, float fallback)
	{
		std::istringstream tokens(remainder);
		std::string token;
		while (tokens >> token) {
			if (lowerAscii(token) == "-bm") {
				float value = fallback;
				if ((tokens >> value) && std::isfinite(value)) return value;
				break;
			}
		}
		return fallback;
	}

	bool isUsableRegularFile(const std::filesystem::path& path)
	{
		std::error_code ec;
		return std::filesystem::exists(path, ec) && !ec &&
			std::filesystem::is_regular_file(path, ec) && !ec;
	}

	bool isSupportedMaterialTextureFile(const std::filesystem::path& path)
	{
		if (!isUsableRegularFile(path)) return false;
		const std::string extension = lowerAscii(path.extension().string());
		return extension == ".png" || extension == ".jpg" || extension == ".jpeg" ||
			extension == ".tga" || extension == ".bmp" || extension == ".dds";
	}

	const char* materialTextureChannelName(MaterialTextureChannel channel)
	{
		switch (channel) {
		case MaterialTextureChannel::Albedo: return "Albedo";
		case MaterialTextureChannel::Normal: return "Normal";
		case MaterialTextureChannel::Metallic: return "Metallic";
		case MaterialTextureChannel::Roughness: return "Roughness";
		case MaterialTextureChannel::AO: return "AO";
		case MaterialTextureChannel::Emissive: return "Emissive";
		default: return "Texture";
		}
	}

	bool isValidMaterialTextureChannel(int value)
	{
		return value >= static_cast<int>(MaterialTextureChannel::Albedo) &&
			value <= static_cast<int>(MaterialTextureChannel::Emissive);
	}

	Texture* getMaterialTextureForChannel(MaterialInstance* materialInstance, MaterialTextureChannel channel)
	{
		if (!materialInstance) return nullptr;
		switch (channel) {
		case MaterialTextureChannel::Albedo: return materialInstance->getAlbedo();
		case MaterialTextureChannel::Normal: return materialInstance->getNormal();
		case MaterialTextureChannel::Metallic: return materialInstance->getMetallic();
		case MaterialTextureChannel::Roughness: return materialInstance->getRoughness();
		case MaterialTextureChannel::AO: return materialInstance->getAO();
		case MaterialTextureChannel::Emissive: return materialInstance->getEmissive();
		default: return nullptr;
		}
	}

	void setMaterialTextureForChannel(MaterialInstance* materialInstance, MaterialTextureChannel channel, Texture* texture)
	{
		if (!materialInstance) return;
		switch (channel) {
		case MaterialTextureChannel::Albedo: materialInstance->setAlbedo(texture); break;
		case MaterialTextureChannel::Normal: materialInstance->setNormal(texture); break;
		case MaterialTextureChannel::Metallic: materialInstance->setMetallic(texture); break;
		case MaterialTextureChannel::Roughness: materialInstance->setRoughness(texture); break;
		case MaterialTextureChannel::AO: materialInstance->setAO(texture); break;
		case MaterialTextureChannel::Emissive: materialInstance->setEmissive(texture); break;
		default: break;
		}
	}

	std::filesystem::path findProjectRoot()
	{
		namespace fs = std::filesystem;
		std::error_code ec;
		const std::vector<fs::path> roots = getAssetSearchRoots();
		for (const fs::path& root : roots) {
			ec.clear();
			const bool hasSource = fs::exists(root / "source", ec) && !ec;
			ec.clear();
			const bool hasInclude = fs::exists(root / "include", ec) && !ec;
			if (hasSource && hasInclude) return root;
		}
		for (const fs::path& root : roots) {
			ec.clear();
			if (fs::exists(root / "Assets", ec) && !ec) return root;
		}
		ec.clear();
		return fs::current_path(ec);
	}

	bool resolveMaterialOverrideTexturePath(const std::string& serializedPath, std::filesystem::path& resolvedPath)
	{
		namespace fs = std::filesystem;
		resolvedPath.clear();
		if (serializedPath.empty()) return false;
		const fs::path requested(serializedPath);
		if (requested.is_absolute() && isSupportedMaterialTextureFile(requested)) {
			resolvedPath = requested.lexically_normal();
			return true;
		}
		if (isSupportedMaterialTextureFile(requested)) {
			resolvedPath = requested.lexically_normal();
			return true;
		}
		for (const fs::path& root : getAssetSearchRoots()) {
			const fs::path candidate = root / requested;
			if (isSupportedMaterialTextureFile(candidate)) {
				resolvedPath = candidate.lexically_normal();
				return true;
			}
		}
		return false;
	}

	bool copyMaterialTextureIntoProject(const std::filesystem::path& sourcePath,
		int actorIndex,
		size_t materialSlot,
		MaterialTextureChannel channel,
		std::filesystem::path& projectTexturePath)
	{
		namespace fs = std::filesystem;
		projectTexturePath.clear();
		if (!isSupportedMaterialTextureFile(sourcePath) || actorIndex < 0) return false;

		const fs::path projectRoot = findProjectRoot();
		if (projectRoot.empty()) return false;
		const fs::path destinationDirectory = projectRoot / "Assets" / "Textures" / "MaterialOverrides" /
			("Actor_" + std::to_string(actorIndex));
		std::error_code ec;
		fs::create_directories(destinationDirectory, ec);
		if (ec) return false;

		std::string extension = lowerAscii(sourcePath.extension().string());
		if (extension == ".jpeg") extension = ".jpg";
		const fs::path destination = destinationDirectory /
			("Slot_" + std::to_string(materialSlot) + "_" + materialTextureChannelName(channel) + extension);

		ec.clear();
		const fs::path sourceAbsolute = fs::absolute(sourcePath, ec).lexically_normal();
		ec.clear();
		const fs::path destinationAbsolute = fs::absolute(destination, ec).lexically_normal();
		if (lowerAscii(sourceAbsolute.generic_string()) != lowerAscii(destinationAbsolute.generic_string())) {
			ec.clear();
			fs::copy_file(sourcePath, destination, fs::copy_options::overwrite_existing, ec);
			if (ec) return false;
		}

		projectTexturePath = destination.lexically_normal();
		return isSupportedMaterialTextureFile(projectTexturePath);
	}

	std::vector<std::filesystem::path> findOBJMaterialLibraries(const std::filesystem::path& objPath)
	{
		std::vector<std::filesystem::path> result;
		std::ifstream stream(objPath);
		if (!stream.is_open()) return result;

		std::string line;
		while (std::getline(stream, line)) {
			const std::string trimmed = trimAscii(line);
			if (trimmed.empty() || trimmed[0] == '#') continue;

			std::istringstream lineStream(trimmed);
			std::string command;
			lineStream >> command;
			if (lowerAscii(command) != "mtllib") continue;

			std::string remainder;
			std::getline(lineStream, remainder);
			remainder = trimAscii(remainder);
			if (remainder.empty()) continue;

			std::vector<std::string> libraryNames;
			if (remainder.front() == '"') {
				size_t cursor = 0;
				while (cursor < remainder.size()) {
					const size_t begin = remainder.find('"', cursor);
					if (begin == std::string::npos) break;
					const size_t end = remainder.find('"', begin + 1);
					if (end == std::string::npos) break;
					if (end > begin + 1) libraryNames.push_back(remainder.substr(begin + 1, end - begin - 1));
					cursor = end + 1;
				}
			}
			else {
				const std::filesystem::path wholePath = objPath.parent_path() / stripOptionalQuotes(remainder);
				if (isUsableRegularFile(wholePath)) {
					libraryNames.push_back(stripOptionalQuotes(remainder));
				}
				else {
					std::istringstream names(remainder);
					std::string name;
					while (names >> name) libraryNames.push_back(stripOptionalQuotes(name));
				}
			}

			for (const std::string& libraryName : libraryNames) {
				if (libraryName.empty()) continue;
				std::filesystem::path candidate(libraryName);
				if (!candidate.is_absolute()) candidate = objPath.parent_path() / candidate;
				candidate = candidate.lexically_normal();
				if (isUsableRegularFile(candidate)) {
					result.push_back(candidate);
				}
			}
		}
		return result;
	}

	std::unordered_map<std::string, ObjMaterialInfo> loadOBJMaterialLibrary(const std::filesystem::path& objPath)
	{
		std::unordered_map<std::string, ObjMaterialInfo> materials;
		for (const std::filesystem::path& mtlPath : findOBJMaterialLibraries(objPath)) {
			std::ifstream stream(mtlPath);
			if (!stream.is_open()) continue;

			ObjMaterialInfo* current = nullptr;
			std::string line;
			while (std::getline(stream, line)) {
				const size_t comment = line.find('#');
				if (comment != std::string::npos) line.erase(comment);
				line = trimAscii(line);
				if (line.empty()) continue;

				std::istringstream lineStream(line);
				std::string command;
				lineStream >> command;
				const std::string lowerCommand = lowerAscii(command);

				if (lowerCommand == "newmtl") {
					std::string materialName;
					std::getline(lineStream, materialName);
					materialName = trimAscii(materialName);
					if (materialName.empty()) continue;
					ObjMaterialInfo info;
					info.name = materialName;
					info.sourceDirectory = mtlPath.parent_path();
					materials[materialName] = info;
					current = &materials[materialName];
					continue;
				}

				if (!current) continue;
				if (lowerCommand == "kd") {
					lineStream >> current->baseColor.x >> current->baseColor.y >> current->baseColor.z;
				}
				else if (lowerCommand == "d") {
					lineStream >> current->baseColor.w;
					current->baseColor.w = std::clamp(current->baseColor.w, 0.0f, 1.0f);
				}
				else if (lowerCommand == "tr") {
					float transparency = 0.0f;
					if (lineStream >> transparency) current->baseColor.w = 1.0f - std::clamp(transparency, 0.0f, 1.0f);
				}
				else if (lowerCommand == "ns") {
					float shininess = 0.0f;
					if (lineStream >> shininess && !current->hasRoughnessValue) {
						shininess = (std::max)(0.0f, shininess);
						current->roughness = std::clamp(std::sqrt(2.0f / (shininess + 2.0f)), 0.02f, 1.0f);
					}
				}
				else if (lowerCommand == "pr") {
					if (lineStream >> current->roughness) {
						current->roughness = std::clamp(current->roughness, 0.0f, 1.0f);
						current->hasRoughnessValue = true;
					}
				}
				else if (lowerCommand == "pm") {
					if (lineStream >> current->metallic) {
						current->metallic = std::clamp(current->metallic, 0.0f, 1.0f);
						current->hasMetallicValue = true;
					}
				}
				else if (lowerCommand == "ke") {
					lineStream >> current->emissiveColor.x >> current->emissiveColor.y >> current->emissiveColor.z;
				}
				else if (lowerCommand == "map_kd") {
					std::string remainder; std::getline(lineStream, remainder); current->albedoMap = parseMtlMapFilename(remainder);
				}
				else if (lowerCommand == "map_bump" || lowerCommand == "bump" || lowerCommand == "norm" || lowerCommand == "map_kn") {
					std::string remainder;
					std::getline(lineStream, remainder);
					current->normalMap = parseMtlMapFilename(remainder);
					current->normalScale = parseMtlBumpScale(remainder, current->normalScale);
				}
				else if (lowerCommand == "map_pm" || lowerCommand == "map_metallic") {
					std::string remainder; std::getline(lineStream, remainder); current->metallicMap = parseMtlMapFilename(remainder);
				}
				else if (lowerCommand == "map_pr" || lowerCommand == "map_roughness") {
					std::string remainder; std::getline(lineStream, remainder); current->roughnessMap = parseMtlMapFilename(remainder);
				}
				else if (lowerCommand == "map_ka" || lowerCommand == "map_ao" || lowerCommand == "map_occlusion") {
					std::string remainder; std::getline(lineStream, remainder); current->aoMap = parseMtlMapFilename(remainder);
				}
				else if (lowerCommand == "map_ke" || lowerCommand == "map_emissive") {
					std::string remainder; std::getline(lineStream, remainder); current->emissiveMap = parseMtlMapFilename(remainder);
				}
			}
		}
		return materials;
	}

	bool resolveMaterialTexturePath(const std::string& rawPath,
		const ObjMaterialInfo& material,
		const std::filesystem::path& objPath,
		std::filesystem::path& resolvedPath)
	{
		resolvedPath.clear();
		const std::string cleaned = stripOptionalQuotes(rawPath);
		if (cleaned.empty()) return false;

		const std::filesystem::path requested(cleaned);
		std::vector<std::filesystem::path> candidates;
		if (requested.is_absolute()) candidates.push_back(requested);
		candidates.push_back(material.sourceDirectory / requested);
		candidates.push_back(objPath.parent_path() / requested);

		const std::filesystem::path filename = requested.filename();
		const std::filesystem::path modelFolderName = objPath.stem();
		for (const std::filesystem::path& root : getAssetSearchRoots()) {
			candidates.push_back(root / "Assets" / "Textures" / requested);
			if (!filename.empty()) {
				candidates.push_back(root / "Assets" / "Textures" / modelFolderName / filename);
				candidates.push_back(root / "Assets" / "Textures" / filename);
				candidates.push_back(root / "Assets" / "Models" / filename);
			}
		}

		for (const auto& candidate : candidates) {
			if (isUsableRegularFile(candidate)) {
				resolvedPath = candidate.lexically_normal();
				return true;
			}
		}
		return false;
	}

	Texture* loadImportedTexture(Device& device,
		const std::filesystem::path& texturePath,
		std::vector<std::unique_ptr<Texture>>& ownedTextures,
		std::unordered_map<std::string, Texture*>& loadedTextureCache)
	{
		if (texturePath.empty()) return nullptr;
		const std::string key = lowerAscii(texturePath.lexically_normal().generic_string());
		auto found = loadedTextureCache.find(key);
		if (found != loadedTextureCache.end()) return found->second;

		auto texture = std::make_unique<Texture>();
		const std::string extension = lowerAscii(texturePath.extension().string());
		const ExtensionType type = extension == ".dds" ? DDS : PNG;
		const HRESULT hr = texture->init(device, texturePath.string(), type);
		if (FAILED(hr)) {
			const std::wstring pathW(texturePath.wstring());
			MESSAGE("Main", "OBJMaterial", L"Texture could not be loaded; fallback will be used: " << pathW);
			return nullptr;
		}

		Texture* raw = texture.get();
		ownedTextures.push_back(std::move(texture));
		loadedTextureCache[key] = raw;
		return raw;
	}

	SimpleVertex makeVertex(float px, float py, float pz,
		float nx, float ny, float nz,
		float tx, float ty, float tz,
		float bx, float by, float bz,
		float u, float v)
	{
		SimpleVertex vertex{};
		vertex.Position = EU::Vector3(px, py, pz);
		vertex.Normal = EU::Vector3(nx, ny, nz);
		vertex.Tangent = EU::Vector3(tx, ty, tz);
		vertex.Bitangent = EU::Vector3(bx, by, bz);
		vertex.TextureCoordinate = EU::Vector2(u, v);
		return vertex;
	}

	void addQuad(MeshComponent& mesh,
		const std::array<EU::Vector3, 4>& positions,
		const EU::Vector3& normal,
		const EU::Vector3& tangent,
		const EU::Vector3& bitangent)
	{
		const unsigned int base = static_cast<unsigned int>(mesh.m_vertex.size());
		const std::array<EU::Vector2, 4> uv = {
			EU::Vector2(0.0f, 1.0f), EU::Vector2(0.0f, 0.0f),
			EU::Vector2(1.0f, 0.0f), EU::Vector2(1.0f, 1.0f)
		};
		for (size_t i = 0; i < positions.size(); ++i) {
			mesh.m_vertex.push_back(makeVertex(
				positions[i].x, positions[i].y, positions[i].z,
				normal.x, normal.y, normal.z,
				tangent.x, tangent.y, tangent.z,
				bitangent.x, bitangent.y, bitangent.z,
				uv[i].x, uv[i].y));
		}
		mesh.m_index.insert(mesh.m_index.end(), {
			base + 0, base + 1, base + 2,
			base + 0, base + 2, base + 3
		});
	}

	MeshComponent makeDemoCube()
	{
		MeshComponent mesh;
		mesh.m_name = "BuiltinCube";
		constexpr float h = 1.0f;

		addQuad(mesh, { EU::Vector3(-h,-h,-h), EU::Vector3(-h, h,-h), EU::Vector3( h, h,-h), EU::Vector3( h,-h,-h) },
			EU::Vector3(0,0,-1), EU::Vector3(1,0,0), EU::Vector3(0,1,0));
		addQuad(mesh, { EU::Vector3( h,-h, h), EU::Vector3( h, h, h), EU::Vector3(-h, h, h), EU::Vector3(-h,-h, h) },
			EU::Vector3(0,0,1), EU::Vector3(-1,0,0), EU::Vector3(0,1,0));
		addQuad(mesh, { EU::Vector3(-h,-h, h), EU::Vector3(-h, h, h), EU::Vector3(-h, h,-h), EU::Vector3(-h,-h,-h) },
			EU::Vector3(-1,0,0), EU::Vector3(0,0,-1), EU::Vector3(0,1,0));
		addQuad(mesh, { EU::Vector3( h,-h,-h), EU::Vector3( h, h,-h), EU::Vector3( h, h, h), EU::Vector3( h,-h, h) },
			EU::Vector3(1,0,0), EU::Vector3(0,0,1), EU::Vector3(0,1,0));
		addQuad(mesh, { EU::Vector3(-h, h,-h), EU::Vector3(-h, h, h), EU::Vector3( h, h, h), EU::Vector3( h, h,-h) },
			EU::Vector3(0,1,0), EU::Vector3(1,0,0), EU::Vector3(0,0,-1));
		addQuad(mesh, { EU::Vector3(-h,-h, h), EU::Vector3(-h,-h,-h), EU::Vector3( h,-h,-h), EU::Vector3( h,-h, h) },
			EU::Vector3(0,-1,0), EU::Vector3(1,0,0), EU::Vector3(0,0,1));

		mesh.m_numVertex = static_cast<int>(mesh.m_vertex.size());
		mesh.m_numIndex = static_cast<int>(mesh.m_index.size());
		return mesh;
	}

	EU::Vector3 normalizedCross(const EU::Vector3& a, const EU::Vector3& b)
	{
		const EU::Vector3 c(
			a.y * b.z - a.z * b.y,
			a.z * b.x - a.x * b.z,
			a.x * b.y - a.y * b.x);
		const float lenSq = c.x * c.x + c.y * c.y + c.z * c.z;
		if (lenSq <= 1e-12f) return EU::Vector3(0, 1, 0);
		const float invLen = 1.0f / std::sqrt(lenSq);
		return c * invLen;
	}

	void addTriangle(MeshComponent& mesh,
		const EU::Vector3& a, const EU::Vector3& b, const EU::Vector3& c)
	{
		const EU::Vector3 edge1 = b - a;
		const EU::Vector3 edge2 = c - a;
		const EU::Vector3 normal = normalizedCross(edge1, edge2);
		const float tangentLenSq = edge1.x * edge1.x + edge1.y * edge1.y + edge1.z * edge1.z;
		EU::Vector3 tangent = tangentLenSq > 1e-12f
			? edge1 * (1.0f / std::sqrt(tangentLenSq))
			: EU::Vector3(1,0,0);
		EU::Vector3 bitangent = normalizedCross(normal, tangent);

		const unsigned int base = static_cast<unsigned int>(mesh.m_vertex.size());
		mesh.m_vertex.push_back(makeVertex(a.x,a.y,a.z, normal.x,normal.y,normal.z, tangent.x,tangent.y,tangent.z, bitangent.x,bitangent.y,bitangent.z, 0.0f,1.0f));
		mesh.m_vertex.push_back(makeVertex(b.x,b.y,b.z, normal.x,normal.y,normal.z, tangent.x,tangent.y,tangent.z, bitangent.x,bitangent.y,bitangent.z, 0.5f,0.0f));
		mesh.m_vertex.push_back(makeVertex(c.x,c.y,c.z, normal.x,normal.y,normal.z, tangent.x,tangent.y,tangent.z, bitangent.x,bitangent.y,bitangent.z, 1.0f,1.0f));
		mesh.m_index.insert(mesh.m_index.end(), { base, base + 1, base + 2 });
	}

	MeshComponent makeDemoPyramid()
	{
		MeshComponent mesh;
		mesh.m_name = "BuiltinPyramid";
		const EU::Vector3 p0(-1.0f, 0.0f,-1.0f);
		const EU::Vector3 p1(-1.0f, 0.0f, 1.0f);
		const EU::Vector3 p2( 1.0f, 0.0f, 1.0f);
		const EU::Vector3 p3( 1.0f, 0.0f,-1.0f);
		const EU::Vector3 top(0.0f, 2.2f, 0.0f);

		addTriangle(mesh, p0, top, p1);
		addTriangle(mesh, p1, top, p2);
		addTriangle(mesh, p2, top, p3);
		addTriangle(mesh, p3, top, p0);
		addTriangle(mesh, p0, p1, p2);
		addTriangle(mesh, p0, p2, p3);

		mesh.m_numVertex = static_cast<int>(mesh.m_vertex.size());
		mesh.m_numIndex = static_cast<int>(mesh.m_index.size());
		return mesh;
	}

	MeshComponent makeDemoFloor()
	{
		MeshComponent mesh;
		mesh.m_name = "BuiltinFloor";
		const float x = 7.0f;
		const float z = 7.0f;
		addQuad(mesh, { EU::Vector3(-x,0,-z), EU::Vector3(-x,0, z), EU::Vector3( x,0, z), EU::Vector3( x,0,-z) },
			EU::Vector3(0,1,0), EU::Vector3(1,0,0), EU::Vector3(0,0,1));
		// Reversed winding makes the floor visible even if the current rasterizer
		// considers the opposite orientation to be the front face.
		mesh.m_index.insert(mesh.m_index.end(), { 0, 2, 1, 0, 3, 2 });
		mesh.m_numVertex = static_cast<int>(mesh.m_vertex.size());
		mesh.m_numIndex = static_cast<int>(mesh.m_index.size());
		return mesh;
	}

	HRESULT uploadBuiltinMesh(Device& device, const MeshComponent& source, Mesh& destination)
	{
		destination.destroy();
		Submesh submesh{};
		HRESULT hr = submesh.vertexBuffer.init(device, source, D3D11_BIND_VERTEX_BUFFER);
		if (FAILED(hr)) return hr;
		hr = submesh.indexBuffer.init(device, source, D3D11_BIND_INDEX_BUFFER);
		if (FAILED(hr)) return hr;
		submesh.indexCount = static_cast<unsigned int>(source.m_index.size());
		submesh.localTransform = source.m_localTransform;
		submesh.materialSlot = 0;
		destination.getSubmeshes().push_back(std::move(submesh));
		return S_OK;
	}

	HRESULT uploadImportedMeshes(Device& device, const std::vector<MeshComponent>& sources, Mesh& destination)
	{
		destination.destroy();
		if (sources.empty()) {
			return E_INVALIDARG;
		}

		for (const MeshComponent& source : sources) {
			if (source.m_vertex.empty() || source.m_index.empty()) {
				continue;
			}

			Submesh submesh{};
			HRESULT hr = submesh.vertexBuffer.init(device, source, D3D11_BIND_VERTEX_BUFFER);
			if (FAILED(hr)) {
				destination.destroy();
				return hr;
			}

			hr = submesh.indexBuffer.init(device, source, D3D11_BIND_INDEX_BUFFER);
			if (FAILED(hr)) {
				destination.destroy();
				return hr;
			}

			submesh.indexCount = static_cast<unsigned int>(source.m_index.size());
			submesh.localTransform = source.m_localTransform;
			submesh.materialSlot = static_cast<unsigned int>(destination.getSubmeshes().size());
			destination.getSubmeshes().push_back(std::move(submesh));
		}

		return destination.getSubmeshes().empty() ? E_FAIL : S_OK;
	}

	bool computeImportedPlacement(const std::vector<MeshComponent>& meshes, EU::Vector3& outPosition, EU::Vector3& outScale)
	{
		float minX = (std::numeric_limits<float>::max)();
		float minY = (std::numeric_limits<float>::max)();
		float minZ = (std::numeric_limits<float>::max)();
		float maxX = -(std::numeric_limits<float>::max)();
		float maxY = -(std::numeric_limits<float>::max)();
		float maxZ = -(std::numeric_limits<float>::max)();
		bool hasVertices = false;

		for (const MeshComponent& mesh : meshes) {
			for (const SimpleVertex& vertex : mesh.m_vertex) {
				const EU::Vector3& p = vertex.Position;
				if (!std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z)) {
					continue;
				}
				minX = (std::min)(minX, p.x);
				minY = (std::min)(minY, p.y);
				minZ = (std::min)(minZ, p.z);
				maxX = (std::max)(maxX, p.x);
				maxY = (std::max)(maxY, p.y);
				maxZ = (std::max)(maxZ, p.z);
				hasVertices = true;
			}
		}

		if (!hasVertices) {
			return false;
		}

		const float extentX = maxX - minX;
		const float extentY = maxY - minY;
		const float extentZ = maxZ - minZ;
		const float maxExtent = (std::max)(extentX, (std::max)(extentY, extentZ));
		if (!std::isfinite(maxExtent) || maxExtent <= 1e-6f) {
			return false;
		}

		float uniformScale = 2.5f / maxExtent;
		uniformScale = (std::max)(0.0001f, (std::min)(uniformScale, 1000.0f));
		const float centerX = (minX + maxX) * 0.5f;
		const float centerZ = (minZ + maxZ) * 0.5f;

		outScale = EU::Vector3(uniformScale, uniformScale, uniformScale);
		outPosition = EU::Vector3(
			-centerX * uniformScale,
			-minY * uniformScale,
			4.0f - centerZ * uniformScale);
		return true;
	}

	bool attachRenderer(const EU::TSharedPointer<Actor>& actor, Mesh& mesh, MaterialInstance& material, bool castShadow)
	{
		if (actor.isNull()) return false;
		EU::TSharedPointer<MeshRendererComponent> renderer = actor->getComponent<MeshRendererComponent>();
		if (!renderer) {
			renderer = EU::MakeShared<MeshRendererComponent>();
			actor->addComponent(renderer);
		}
		renderer->setMesh(&mesh);
		renderer->setMaterialInstance(&material);
		renderer->setVisible(true);
		renderer->setCastShadow(castShadow);
		return true;
	}

	bool attachRenderer(const EU::TSharedPointer<Actor>& actor, Mesh& mesh, const std::vector<MaterialInstance*>& materials, bool castShadow)
	{
		if (actor.isNull() || materials.empty()) return false;
		EU::TSharedPointer<MeshRendererComponent> renderer = actor->getComponent<MeshRendererComponent>();
		if (!renderer) {
			renderer = EU::MakeShared<MeshRendererComponent>();
			actor->addComponent(renderer);
		}
		renderer->setMesh(&mesh);
		renderer->setMaterialInstances(materials);
		renderer->setVisible(true);
		renderer->setCastShadow(castShadow);
		return true;
	}
}

HRESULT
BaseApp::awake() {
	HRESULT hr = S_OK;

	// Inicializacion de dlls y elementos externos al motor.
	m_sceneGraph.init();

	// Log Success Message
	MESSAGE("Main", "Awake", "Application awake successfully.");
	return hr;
}

int
BaseApp::run(HINSTANCE hInst, int nCmdShow) {
	// 1) Initialize Window
	if (FAILED(m_window.init(hInst, nCmdShow, WndProc, this))) {
		ERROR("Main", "Run", "Failed to initialize window.");
		return 0;
	}
	// 2) Awake Application
	if (FAILED(awake())) {
		ERROR("Main", "Run", "Failed to awake application.");
		return 0;
	}
	// 3) Initialize Device and Device Context
	if (FAILED(init())) {
		ERROR("Main", "Run", "Failed to initialize device and device context.");
		return 0;
	}
	// 4) Initialize GUI
	if (!m_gui.init(m_window, m_device, m_deviceContext)) {
		ERROR("Main", "Run", "Failed to initialize ImGui Win32/DX11 backends.");
		return 0;
	}
	m_guiInitialized = true;

	// Main message loop
	MSG msg = {};
	LARGE_INTEGER freq, prev;
	QueryPerformanceFrequency(&freq);
	QueryPerformanceCounter(&prev);
	while (WM_QUIT != msg.message)
	{
		if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else
		{
			LARGE_INTEGER curr;
			QueryPerformanceCounter(&curr);
			float deltaTime = static_cast<float>(curr.QuadPart - prev.QuadPart) / freq.QuadPart;
			prev = curr;
			update(deltaTime);
			render();
		}
	}
	return (int)msg.wParam;
}

HRESULT
BaseApp::init() {
	HRESULT hr = S_OK;

	// Crear swapchain
	hr = m_swapChain.init(m_device, m_deviceContext, m_backBuffer, m_window);

	if (FAILED(hr)) {
		ERROR("Main", "InitDevice",
			("Failed to initialize SwapChain. HRESULT: " + std::to_string(hr)).c_str());
		return hr;
	}

	// Crear render target view
	hr = m_renderTargetView.init(m_device, m_backBuffer, DXGI_FORMAT_R8G8B8A8_UNORM);

	if (FAILED(hr)) {
		ERROR("Main", "InitDevice",
			("Failed to initialize RenderTargetView. HRESULT: " + std::to_string(hr)).c_str());
		return hr;
	}

	// Crear textura de depth stencil
	D3D11_TEXTURE2D_DESC backBufferDesc{};
	if (!m_backBuffer.m_texture) return E_POINTER;
	m_backBuffer.m_texture->GetDesc(&backBufferDesc);
	hr = m_depthStencil.init(m_device,
		m_window.m_width,
		m_window.m_height,
		DXGI_FORMAT_D24_UNORM_S8_UINT,
		D3D11_BIND_DEPTH_STENCIL,
		backBufferDesc.SampleDesc.Count,
		backBufferDesc.SampleDesc.Quality);

	if (FAILED(hr)) {
		ERROR("Main", "InitDevice",
			("Failed to initialize DepthStencil. HRESULT: " + std::to_string(hr)).c_str());
		return hr;
	}

	// Crear el depth stencil view
	hr = m_depthStencilView.init(m_device,
		m_depthStencil,
		DXGI_FORMAT_D24_UNORM_S8_UINT);

	if (FAILED(hr)) {
		ERROR("Main", "InitDevice",
			("Failed to initialize DepthStencilView. HRESULT: " + std::to_string(hr)).c_str());
		return hr;
	}

	// Crear el m_viewport
	hr = m_viewport.init(m_window);

	if (FAILED(hr)) {
		ERROR("Main", "InitDevice",
			("Failed to initialize Viewport. HRESULT: " + std::to_string(hr)).c_str());
		return hr;
	}

	// Optional panoramic skybox. V17 uses one equirectangular 2:1 image instead
	// of six cubemap faces. A missing image never prevents the editor from starting.
	m_skyboxReady = false;
	m_skyboxTexturePath.clear();
	resetSkyboxToDefault();

	std::string lightIconPath;
	if (resolveOptionalAssetPath("slate/icons/light-bulb.png", lightIconPath)) {
		const HRESULT lightIconHr = m_lightIconTexture.init(m_device, lightIconPath, PNG);
		if (FAILED(lightIconHr)) {
			MESSAGE("Main", "InitDevice", "Light icon could not be loaded. Using the fallback marker.");
		}
	}

	// Input layout used by the PBR material shader. ShaderProgram resolves
	// files from the project Shaders/ folder as well as the current directory.
	LayoutBuilder builder;
	builder.Add("POSITION", DXGI_FORMAT_R32G32B32_FLOAT)
		.Add("NORMAL", DXGI_FORMAT_R32G32B32_FLOAT)
		.Add("TANGENT", DXGI_FORMAT_R32G32B32_FLOAT)
		.Add("BITANGENT", DXGI_FORMAT_R32G32B32_FLOAT)
		.Add("TEXCOORD", DXGI_FORMAT_R32G32_FLOAT);

	hr = m_shaderProgram.init(m_device, "PBRShader.hlsl", builder);
	if (FAILED(hr)) {
		ERROR("Main", "InitDevice",
			("Failed to initialize PBRShader. Verify the Shaders folder. HRESULT: " + std::to_string(hr)).c_str());
		return hr;
	}

	// Constant buffer retained for the legacy path and editor compatibility.
	hr = m_constantBuffer.init(m_device, sizeof(CBMain));
	if (FAILED(hr)) {
		ERROR("Main", "InitDevice",
			("Failed to initialize m_constantBuffer Buffer. HRESULT: " + std::to_string(hr)).c_str());
		return hr;
	}

	// Put the camera where all generated demo objects are visible on first boot.
	m_camera.setLens(XM_PIDIV4, m_window.m_width / (float)m_window.m_height, 0.01f, 100.0f);
	m_camera.lookAt(
		EU::Vector3(0.0f, 3.5f, -8.0f),
		EU::Vector3(0.0f, 1.0f, 4.0f),
		EU::Vector3(0.0f, 1.0f, 0.0f));
	m_camera.updateViewMatrix();

	m_constantBufferStruct.LightColor = EU::Vector3(1.0f, 1.0f, 1.0f);
	m_constantBufferStruct.LightDir = EU::Vector3(-0.35f, -1.0f, 0.35f);


	// Default pipeline states.
	hr = m_defaultRasterizer.init(m_device, D3D11_FILL_SOLID, D3D11_CULL_NONE, false, true);
	if (FAILED(hr)) return hr;
	hr = m_defaultDepthStencil.init(m_device, true, D3D11_DEPTH_WRITE_MASK_ALL, D3D11_COMPARISON_LESS);
	if (FAILED(hr)) return hr;
	hr = m_defaultSampler.init(m_device);
	if (FAILED(hr)) return hr;

	auto configurePbrMaterial = [&](Material& material) {
		material.setShader(&m_shaderProgram);
		material.setRasterizerState(&m_defaultRasterizer);
		material.setDepthStencilState(&m_defaultDepthStencil);
		material.setSamplerState(&m_defaultSampler);
		material.setDomain(MaterialDomain::Opaque);
		material.setBlendMode(BlendMode::Opaque);
	};
	configurePbrMaterial(m_pbrMaterial);
	configurePbrMaterial(m_transparentPbrMaterial);
	m_transparentPbrMaterial.setDomain(MaterialDomain::Transparent);
	m_transparentPbrMaterial.setBlendMode(BlendMode::Alpha);
	configurePbrMaterial(m_cyberGunPbrMaterial);
	configurePbrMaterial(m_drakefirePbrMaterial);
	configurePbrMaterial(m_toadPbrMaterial);

	// Generated 1x1 PBR textures remove all dependency on image files.
	if (FAILED(m_AlbedoSRV.initSolidColor(m_device, 255, 255, 255, 255)) ||
		FAILED(m_NormalSRV.initSolidColor(m_device, 128, 128, 255, 255)) ||
		FAILED(m_MetallicSRV.initSolidColor(m_device, 0, 0, 0, 255)) ||
		FAILED(m_RoughnessSRV.initSolidColor(m_device, 170, 170, 170, 255)) ||
		FAILED(m_AOSRV.initSolidColor(m_device, 255, 255, 255, 255)) ||
		FAILED(m_EmissiveSRV.initSolidColor(m_device, 0, 0, 0, 255))) {
		ERROR("Main", "InitDevice", "Failed to create generated PBR fallback textures.");
		return E_FAIL;
	}

	auto configureGeneratedInstance = [&](MaterialInstance& instance, Material& material, const XMFLOAT4& color, float metallic, float roughness) {
		instance.setMaterial(&material);
		instance.setAlbedo(&m_AlbedoSRV);
		instance.setNormal(&m_NormalSRV);
		instance.setMetallic(&m_MetallicSRV);
		instance.setRoughness(&m_RoughnessSRV);
		instance.setAO(&m_AOSRV);
		instance.setEmissive(&m_EmissiveSRV);
		instance.getParams().baseColor = color;
		instance.getParams().metallic = metallic;
		instance.getParams().roughness = roughness;
		instance.getParams().ao = 1.0f;
		instance.getParams().normalScale = 1.0f;
		instance.getParams().emissiveStrength = 0.0f;
		instance.getParams().alphaCutoff = 0.5f;
	};

	configureGeneratedInstance(m_cyberGunMaterial, m_cyberGunPbrMaterial,
		XMFLOAT4(0.95f, 0.22f, 0.12f, 1.0f), 0.15f, 0.35f);
	configureGeneratedInstance(m_drakefireMaterial, m_drakefirePbrMaterial,
		XMFLOAT4(0.08f, 0.42f, 0.95f, 1.0f), 0.05f, 0.28f);
	configureGeneratedInstance(m_toadMaterial, m_toadPbrMaterial,
		XMFLOAT4(0.42f, 0.44f, 0.48f, 1.0f), 0.0f, 0.82f);

	// Built-in scene: cube + pyramid + floor. These meshes are generated in
	// memory so an empty Assets/Models folder can never prevent startup.
	m_cyberGun = EU::MakeShared<Actor>(m_device);
	m_drakefirePistol = EU::MakeShared<Actor>(m_device);
	m_sciFiToad = EU::MakeShared<Actor>(m_device);
	if (m_cyberGun.isNull() || m_drakefirePistol.isNull() || m_sciFiToad.isNull()) {
		ERROR("Main", "InitDevice", "Failed to create built-in demo actors.");
		return E_OUTOFMEMORY;
	}

	m_cyberGun->setName("Demo Cube");
	m_drakefirePistol->setName("Demo Pyramid");
	m_sciFiToad->setName("Demo Floor");

	m_cyberGun->getComponent<Transform>()->setTransform(
		EU::Vector3(-2.2f, 1.0f, 4.0f), EU::Vector3(0.0f, 0.35f, 0.0f), EU::Vector3(1.0f, 1.0f, 1.0f));
	m_drakefirePistol->getComponent<Transform>()->setTransform(
		EU::Vector3(2.2f, 0.0f, 4.8f), EU::Vector3(0.0f, -0.35f, 0.0f), EU::Vector3(1.0f, 1.0f, 1.0f));
	m_sciFiToad->getComponent<Transform>()->setTransform(
		EU::Vector3(0.0f, 0.0f, 4.0f), EU::Vector3(0.0f, 0.0f, 0.0f), EU::Vector3(1.0f, 1.0f, 1.0f));

	const MeshComponent cubeMesh = makeDemoCube();
	const MeshComponent pyramidMesh = makeDemoPyramid();
	const MeshComponent floorMesh = makeDemoFloor();
	if (FAILED(uploadBuiltinMesh(m_device, cubeMesh, m_cyberGunRenderMesh)) ||
		FAILED(uploadBuiltinMesh(m_device, pyramidMesh, m_drakefireRenderMesh)) ||
		FAILED(uploadBuiltinMesh(m_device, floorMesh, m_toadRenderMesh))) {
		ERROR("Main", "InitDevice", "Failed to upload built-in demo geometry to the GPU.");
		return E_FAIL;
	}

	if (!attachRenderer(m_cyberGun, m_cyberGunRenderMesh, m_cyberGunMaterial, true) ||
		!attachRenderer(m_drakefirePistol, m_drakefireRenderMesh, m_drakefireMaterial, true) ||
		!attachRenderer(m_sciFiToad, m_toadRenderMesh, m_toadMaterial, false)) {
		ERROR("Main", "InitDevice", "Failed to attach renderers to built-in demo actors.");
		return E_FAIL;
	}

	m_actors.push_back(m_cyberGun);
	m_actors.push_back(m_drakefirePistol);
	m_actors.push_back(m_sciFiToad);
	for (auto& actor : m_actors) {
		m_sceneGraph.addEntity(actor.get());
	}

	// Registra el tipo de geometria procedural para poder reconstruirla al abrir
	// una escena desde cero (no depende de archivos OBJ externos).
	registerBuiltinActorMetadata(m_cyberGun, BuiltinMeshKind::Cube);
	registerBuiltinActorMetadata(m_drakefirePistol, BuiltinMeshKind::Pyramid);
	registerBuiltinActorMetadata(m_sciFiToad, BuiltinMeshKind::Floor);

	MESSAGE("Main", "InitDevice", "Built-in demo scene created: cube, pyramid and floor.");

	m_directionalLightActor = EU::MakeShared<Actor>(m_device);
	if (!m_directionalLightActor.isNull()) {
		m_directionalLightActor->setName("Light Actor 1");
		EU::TSharedPointer<LightComponent> lightComponent = m_directionalLightActor->getComponent<LightComponent>();
		if (!lightComponent) {
			lightComponent = EU::MakeShared<LightComponent>();
			m_directionalLightActor->addComponent(lightComponent);
		}

		lightComponent->getLightData().type = LightType::Directional;
		lightComponent->getLightData().direction = m_constantBufferStruct.LightDir;
		lightComponent->getLightData().color = m_constantBufferStruct.LightColor;
		lightComponent->getLightData().intensity = 1.0f;
		lightComponent->getLightData().range = 12.0f;
		lightComponent->setCastShadow(true);

		EU::TSharedPointer<Transform> transform = m_directionalLightActor->getComponent<Transform>();
		if (transform) {
			transform->setTransform(EU::Vector3(0.0f, 3.0f, 0.0f),
				EU::Vector3(0.0f, 0.0f, 0.0f),
				EU::Vector3(1.0f, 1.0f, 1.0f));
		}

		m_actors.push_back(m_directionalLightActor);
		m_sceneGraph.addEntity(m_directionalLightActor.get());
	}

	m_currentScenePath = getDefaultScenePath();
	if (isValidSceneFileHeader(m_currentScenePath)) {
		// V13 scenes are self-contained enough to rebuild actors from a clean
		// runtime list. Clearing first prevents stale components when actors were
		// deleted/reordered between sessions.
		clearCurrentSceneActors();
		if (!loadScene(m_currentScenePath)) {
			ERROR("Main", "InitDevice", "Default scene exists but could not be loaded. Starting a fresh scene.");
			createNewScene();
		}
	}

	hr = m_editorViewportPass.init(m_device, 1280, 720);
	if (FAILED(hr)) {
		ERROR("Main", "InitDevice",
			("Failed to initialize EditorViewportPass. HRESULT: " + std::to_string(hr)).c_str());
		return hr;
	}

	hr = m_renderPipeline.init(m_device, RendererType::Deferred);
	if (FAILED(hr)) {
		ERROR("Main", "InitDevice",
			("Failed to initialize RenderPipeline. HRESULT: " + std::to_string(hr)).c_str());
		return hr;
	}

	// Construye el catalogo inicial despues de que el dispositivo y el renderer
	// estan disponibles. Las miniaturas son recursos D3D11 administrados por BaseApp.
	refreshAssetBrowserCatalog(true);

	m_d3dReady = true;
	resetSceneHistory("Initial Scene");
	m_gui.setHistoryAvailability(canUndoSceneHistory(), canRedoSceneHistory());
	return S_OK;
}

void
BaseApp::update(float deltaTime) {
	// Apply a pending editor viewport resize BEFORE starting the new ImGui frame.
	// ImGui stores the viewport/GBuffer SRVs inside its draw commands until
	// GUI::render(). Releasing those SRVs after drawViewportPanel() but before
	// ImGui_ImplDX11_RenderDrawData() leaves dangling texture pointers and can
	// make the D3D11 backend break inside DrawIndexed.
	handleEditorViewportResize();
	// Scene/actor operations are deferred for the same reason as viewport/texture
	// changes: deleting or rebuilding actors while ImGui still references them
	// during the previous frame can leave dangling pointers.
	handlePendingSceneEditorAction();
	// Asset Browser actions are also deferred until the previous ImGui frame is
	// fully rendered. This is especially important when applying a texture that
	// may replace an SRV shown by the editor.
	handlePendingAssetBrowserAction();
	// Texture replacement requests are executed before the new ImGui frame.
	// This keeps the previous frame's SRV pointers alive until ImGui is done with them.
	handlePendingMaterialTextureEdit();
	// Skybox texture replacement is deferred for the same SRV lifetime reason.
	handlePendingSkyboxEdit();
	updateAssetBrowserCatalog(deltaTime);

	// Update our time
	static float t = 0.0f;
	if (m_swapChain.m_driverType == D3D_DRIVER_TYPE_REFERENCE)
	{
		t += (float)XM_PI * 0.0125f;
	}
	else
	{
		static DWORD dwTimeStart = 0;
		DWORD dwTimeCur = GetTickCount();
		if (dwTimeStart == 0)
			dwTimeStart = dwTimeCur;
		t = (dwTimeCur - dwTimeStart) / 1000.0f;
	}
	// Update User Interface
	m_gui.setHistoryAvailability(canUndoSceneHistory(), canRedoSceneHistory());
	m_gui.update(m_viewport, m_window);
	m_camera.updateViewMatrix();
	if (m_gui.consumeCreateLightActorRequest()) {
		EU::TSharedPointer<Actor> lightActor = createLightActor();
		if (!lightActor.isNull()) {
			m_gui.selectedActorIndex = static_cast<int>(m_actors.size()) - 1;
			commitSceneHistory("Create Light");
		}
	}
	if (m_gui.consumeImportMeshRequest()) {
		importOBJFromDialog();
	}
	EU::TSharedPointer<Actor> selectedActor;
	if (m_gui.selectedActorIndex >= 0 &&
		m_gui.selectedActorIndex < static_cast<int>(m_actors.size())) {
		selectedActor = m_actors[m_gui.selectedActorIndex];
	}
	bool show_demo_window = true;
	//ImGui::ShowDemoWindow(&show_demo_window);
	m_gui.drawViewportPanel(m_editorViewportPass.getSRV(), m_actors, m_camera, m_window, selectedActor, m_lightIconTexture.m_textureFromImg);
	m_gui.drawRenderDebugPanel(m_renderPipeline.getPreShadowSRV(), m_editorViewportPass.getSRV(), m_renderPipeline.getShadowMapSRV());
	m_gui.drawGBufferDebugPanel(m_renderPipeline.getGBufferAlbedoMetallicSRV(),
		m_renderPipeline.getGBufferNormalRoughnessSRV(),
		m_renderPipeline.getGBufferWorldAoSRV(),
		m_renderPipeline.getGBufferEmissiveAlphaSRV(),
		selectedActor);
	m_renderPipeline.setShadowFactorDebugEnabled(m_gui.m_visualizeDeferredShadowFactor);
	m_renderPipeline.setDeferredDebugViewMode(m_gui.m_deferredDebugViewMode);
	m_renderPipeline.setPostProcessEnabled(m_gui.m_postProcessEnabled);
	m_renderPipeline.setBloomEnabled(m_gui.m_bloomEnabled);
	m_renderPipeline.setTonemappingEnabled(m_gui.m_tonemappingEnabled);
	m_renderPipeline.setFXAAEnabled(m_gui.m_fxaaEnabled);
	m_renderPipeline.setBloomThreshold(m_gui.m_bloomThreshold);
	m_renderPipeline.setBloomIntensity(m_gui.m_bloomIntensity);
	m_renderPipeline.setExposure(m_gui.m_postExposure);
	m_renderPipeline.setFXAAStrength(m_gui.m_fxaaStrength);
	m_gui.outliner(m_actors);
	if (m_gui.selectedActorIndex >= 0 &&
		m_gui.selectedActorIndex < static_cast<int>(m_actors.size())) {
		selectedActor = m_actors[m_gui.selectedActorIndex];
	}
	m_gui.inspectorGeneral(selectedActor);
	m_gui.drawMaterialEditor(selectedActor);
	m_gui.drawAssetBrowser(m_assetBrowserItems, selectedActor);
	std::string historyCommitLabel;
	if (m_gui.consumeHistoryCommitRequest(historyCommitLabel)) {
		commitSceneHistory(historyCommitLabel);
	}
	m_gui.setHistoryAvailability(canUndoSceneHistory(), canRedoSceneHistory());
	if (m_gui.consumeSaveSceneRequest()) {
		if (m_currentScenePath.empty()) {
			saveSceneAsFromDialog();
		}
		else {
			saveScene(m_currentScenePath);
		}
	}

	unsigned int desiredW = static_cast<unsigned int>(m_gui.m_viewportSize.x);
	unsigned int desiredH = static_cast<unsigned int>(m_gui.m_viewportSize.y);

	const unsigned int kMinViewportSize = 64;

	if (desiredW < kMinViewportSize) desiredW = kMinViewportSize;
	if (desiredH < kMinViewportSize) desiredH = kMinViewportSize;

	// Si cambio el tamano solicitado, reinicia estabilidad
	if (desiredW != m_lastRequestedViewportWidth || desiredH != m_lastRequestedViewportHeight)
	{
		m_lastRequestedViewportWidth = desiredW;
		m_lastRequestedViewportHeight = desiredH;
		m_viewportResizeStableFrames = 0;
	}
	else
	{
		// El tamano ya no cambio este frame
		m_viewportResizeStableFrames++;
	}

	// Solo marcar resize cuando el tamano se haya mantenido estable
	const int kStableFramesRequired = 2;

	if (m_viewportResizeStableFrames >= kStableFramesRequired)
	{
		if (desiredW != m_editorViewportPass.getWidth() ||
			desiredH != m_editorViewportPass.getHeight())
		{
			m_editorViewportResizePending = true;
			m_pendingViewportWidth = desiredW;
			m_pendingViewportHeight = desiredH;
		}
	}

	XMStoreFloat4x4(&m_constantBufferStruct.View, XMMatrixTranspose(m_camera.getView()));
	XMStoreFloat4x4(&m_constantBufferStruct.Projection, XMMatrixTranspose(m_camera.getProj()));
	m_constantBufferStruct.CameraPos = m_camera.getPosition();

	// Update panoramic skybox parameters and camera-relative transform.
	if (m_skyboxReady) {
		m_skybox.setIntensity(m_gui.m_skyboxIntensity);
		m_skybox.setRotationDegrees(m_gui.m_skyboxRotationDegrees);
		m_skybox.setTint(m_gui.m_skyboxTint[0], m_gui.m_skyboxTint[1], m_gui.m_skyboxTint[2]);
		m_skybox.update(m_deviceContext, m_camera);
	}

	// Update Actors
	m_sceneGraph.update(deltaTime, m_deviceContext);

}

void
BaseApp::render() {
	if (!m_d3dReady || !m_deviceContext.m_deviceContext || !m_swapChain.m_swapChain) {
		return;
	}

	// Do NOT resize editor render targets here. By this point update() has
	// already built ImGui draw commands containing SRV pointers for this frame.
	// Resizing here would invalidate those pointers before GUI::render().
	float ClearColor[4] = { 0.1f, 0.1f, 0.1f, 1.0f };

	m_renderScene.clear();
	m_sceneGraph.gatherRenderScene(m_renderScene, m_camera);
	m_renderScene.skybox = (m_skyboxReady && m_gui.m_skyboxEnabled) ? &m_skybox : nullptr;
	m_renderPipeline.render(
		m_deviceContext,
		m_camera,
		m_renderScene,
		m_editorViewportPass
	);

	// 2) Volver al backbuffer principal
	m_renderTargetView.render(m_deviceContext, m_depthStencilView, 1, ClearColor);
	m_viewport.render(m_deviceContext);
	m_depthStencilView.render(m_deviceContext);

	// 4) GUI
	m_gui.render();

	m_swapChain.present();
}

void
BaseApp::destroy() {
	cleanupSceneHistory();
	m_d3dReady = false;
	if (m_deviceContext.m_deviceContext) {
		m_deviceContext.m_deviceContext->ClearState();
	}

	// Shut ImGui down before releasing the D3D resources its backend depends on.
	if (m_guiInitialized) {
		m_gui.destroy();
		m_guiInitialized = false;
	}

	// Release editor-created texture overrides and Asset Browser previews while
	// the D3D device is still alive.
	m_materialTextureOverrides.clear();
	m_assetBrowserItems.clear();
	m_assetBrowserPreviewTextures.clear();
	m_assetBrowserFingerprint = 0;

	// Release scene-owned actors while the D3D device is still alive.
	m_renderScene.clear();
	m_sceneGraph.destroy();
	m_cyberGun.reset();
	m_drakefirePistol.reset();
	m_sciFiToad.reset();
	m_directionalLightActor.reset();
	m_actors.clear();

	for (auto& importedAsset : m_importedMeshAssets) {
		if (!importedAsset) continue;
		importedAsset->actor.reset();
		if (importedAsset->renderMesh) {
			importedAsset->renderMesh->destroy();
		}
	}
	m_importedMeshAssets.clear();

	m_editorViewportPass.destroy();
	m_renderPipeline.destroy();
	m_skybox.destroy();
	m_skyboxReady = false;
	m_cyberGunRenderMesh.destroy();
	m_drakefireRenderMesh.destroy();
	m_toadRenderMesh.destroy();
	m_constantBuffer.destroy();

	m_AlbedoSRV.destroy();
	m_MetallicSRV.destroy();
	m_NormalSRV.destroy();
	m_RoughnessSRV.destroy();
	m_AOSRV.destroy();
	m_EmissiveSRV.destroy();
	m_drakefireAlbedoSRV.destroy();
	m_drakefireNormalSRV.destroy();
	m_drakefireMetallicSRV.destroy();
	m_drakefireRoughnessSRV.destroy();
	m_drakefireAOSRV.destroy();
	m_toadAlbedoSRV.destroy();
	m_toadNormalSRV.destroy();
	m_toadMetallicSRV.destroy();
	m_toadRoughnessSRV.destroy();
	m_toadAOSRV.destroy();
	m_toadGlassAlbedoSRV.destroy();
	m_toadGlassNormalSRV.destroy();
	m_toadGlassRoughnessSRV.destroy();
	m_toadHeadAlbedoSRV.destroy();
	m_toadHeadNormalSRV.destroy();
	m_toadHeadRoughnessSRV.destroy();
	m_lightIconTexture.destroy();
	m_skyboxTex.destroy();

	m_defaultRasterizer.destroy();
	m_defaultDepthStencil.destroy();
	m_defaultSampler.destroy();
	m_shaderProgram.destroy();
	m_depthStencilView.destroy();
	m_depthStencil.destroy();
	m_renderTargetView.destroy();
	m_backBuffer.destroy();
	m_swapChain.destroy();

	delete m_model;
	m_model = nullptr;
	delete m_drakefireModel;
	m_drakefireModel = nullptr;
	delete m_toadModel;
	m_toadModel = nullptr;

	m_deviceContext.destroy();
	m_device.destroy();
}

LRESULT
BaseApp::WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
	BaseApp* app = reinterpret_cast<BaseApp*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
	if (app && app->m_guiInitialized &&
		ImGui_ImplWin32_WndProcHandler(hWnd, message, wParam, lParam)) {
		return true;
	}

	switch (message) {
	case WM_CREATE: {
		CREATESTRUCT* pCreate = reinterpret_cast<CREATESTRUCT*>(lParam);
		SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)pCreate->lpCreateParams);
	}
								return 0;
	case WM_PAINT: {
		PAINTSTRUCT ps;
		BeginPaint(hWnd, &ps);
		EndPaint(hWnd, &ps);
	}
							 return 0;
	case WM_SIZE:
	{
		// Evita recrear cuando esta minimizada
		if (wParam == SIZE_MINIMIZED) return 0;

		unsigned int newW = LOWORD(lParam);
		unsigned int newH = HIWORD(lParam);
		if (newW == 0 || newH == 0) return 0;

		if (app) app->onResize(newW, newH);
		return 0;
	}
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	}
	return DefWindowProc(hWnd, message, wParam, lParam);
}

void BaseApp::onResize(unsigned int newW, unsigned int newH)
{
	if (newW == 0 || newH == 0) {
		return;
	}

	// Keep the logical Win32 client size even if D3D is not ready yet.
	m_window.m_width = newW;
	m_window.m_height = newH;
	if (!m_device.m_device || !m_deviceContext.m_deviceContext || !m_swapChain.m_swapChain) {
		return;
	}

	// ResizeBuffers requires every reference to the old back buffer to be released
	// and unbound. The depth buffer does not reference it, so keep the previous
	// depth resources alive until the replacement set is completely valid.
	m_deviceContext.m_deviceContext->OMSetRenderTargets(0, nullptr, nullptr);
	m_renderTargetView.destroy();
	m_backBuffer.destroy();

	HRESULT hr = m_swapChain.resizeBuffers(newW, newH);
	if (FAILED(hr)) {
		ERROR("Main", "onResize", "IDXGISwapChain::ResizeBuffers failed; restoring the current back buffer.");

		// ResizeBuffers leaves the previous swap-chain buffers intact on failure.
		// Reacquire that buffer so the engine can keep presenting instead of
		// remaining permanently without an RTV.
		Texture restoredBackBuffer;
		RenderTargetView restoredRTV;
		if (SUCCEEDED(m_swapChain.getBackBuffer(restoredBackBuffer)) &&
			SUCCEEDED(restoredRTV.init(m_device, restoredBackBuffer, DXGI_FORMAT_R8G8B8A8_UNORM))) {
			m_backBuffer = std::move(restoredBackBuffer);
			m_renderTargetView = std::move(restoredRTV);
			m_d3dReady = true;
		}
		else {
			m_d3dReady = false;
		}
		return;
	}

	Texture newBackBuffer;
	RenderTargetView newRTV;
	Texture newDepthStencil;
	DepthStencilView newDSV;

	hr = m_swapChain.getBackBuffer(newBackBuffer);
	if (FAILED(hr) || !newBackBuffer.m_texture) {
		ERROR("Main", "onResize", "Failed to acquire the resized back buffer.");
		m_d3dReady = false;
		return;
	}

	D3D11_TEXTURE2D_DESC resizedBackBufferDesc{};
	newBackBuffer.m_texture->GetDesc(&resizedBackBufferDesc);
	if (resizedBackBufferDesc.Width == 0 || resizedBackBufferDesc.Height == 0) {
		ERROR("Main", "onResize", "Resized back buffer returned invalid dimensions.");
		m_d3dReady = false;
		return;
	}

	hr = newRTV.init(m_device, newBackBuffer, DXGI_FORMAT_R8G8B8A8_UNORM);
	if (FAILED(hr)) {
		ERROR("Main", "onResize", "Failed to recreate the main render-target view.");
		m_d3dReady = false;
		return;
	}

	hr = newDepthStencil.init(m_device,
		resizedBackBufferDesc.Width,
		resizedBackBufferDesc.Height,
		DXGI_FORMAT_D24_UNORM_S8_UINT,
		D3D11_BIND_DEPTH_STENCIL,
		resizedBackBufferDesc.SampleDesc.Count,
		resizedBackBufferDesc.SampleDesc.Quality);
	if (FAILED(hr)) {
		ERROR("Main", "onResize", "Failed to recreate the main depth-stencil texture.");
		m_d3dReady = false;
		return;
	}

	hr = newDSV.init(m_device, newDepthStencil, DXGI_FORMAT_D24_UNORM_S8_UINT);
	if (FAILED(hr)) {
		ERROR("Main", "onResize", "Failed to recreate the main depth-stencil view.");
		m_d3dReady = false;
		return;
	}

	// Commit only after every size-dependent D3D resource is valid.
	m_backBuffer = std::move(newBackBuffer);
	m_renderTargetView = std::move(newRTV);
	m_depthStencil = std::move(newDepthStencil);
	m_depthStencilView = std::move(newDSV);

	hr = m_viewport.init(resizedBackBufferDesc.Width, resizedBackBufferDesc.Height);
	if (FAILED(hr)) {
		ERROR("Main", "onResize", "Failed to update the main viewport.");
		m_d3dReady = false;
		return;
	}

	m_camera.setLens(XM_PIDIV4,
		resizedBackBufferDesc.Width / static_cast<float>(resizedBackBufferDesc.Height),
		0.01f,
		100.0f);
	m_d3dReady = true;
}

void BaseApp::handleEditorViewportResize()
{
	if (!m_editorViewportResizePending)
		return;

	// Desbindear antes de tocar recursos
	m_deviceContext.m_deviceContext->OMSetRenderTargets(0, nullptr, nullptr);

	ID3D11ShaderResourceView* nullSRVs[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT] = {};
	m_deviceContext.m_deviceContext->PSSetShaderResources(
		0,
		D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT,
		nullSRVs
	);

	// Crear pass temporal nuevo
	EditorViewportPass newPass;
	HRESULT hr = newPass.init(m_device, m_pendingViewportWidth, m_pendingViewportHeight);
	if (FAILED(hr))
	{
		// Si falla, conserva el pass actual
		m_editorViewportResizePending = false;
		return;
	}

	// Resize the active renderer first. If it fails, keep the currently valid
	// viewport and renderer resources at their previous matching dimensions.
	hr = m_renderPipeline.resize(m_device, m_pendingViewportWidth, m_pendingViewportHeight);
	if (FAILED(hr)) {
		ERROR("Main", "handleEditorViewportResize", "Failed to resize the active renderer.");
		m_editorViewportResizePending = false;
		return;
	}

	// Intercambio seguro: el pass viejo queda en newPass y se destruye al salir.
	m_editorViewportPass.swap(newPass);
	m_editorViewportResizePending = false;
}

std::string BaseApp::getDefaultScenePath() const
{
	CreateDirectoryA("Saved", nullptr);
	return "Saved/DefaultScene.wvscene";
}


bool BaseApp::refreshAssetBrowserCatalog(bool force)
{
	namespace fs = std::filesystem;
	const fs::path projectRoot = findProjectRoot();
	if (projectRoot.empty()) return false;

	const fs::path assetsRoot = projectRoot / "Assets";
	std::error_code ec;
	fs::create_directories(assetsRoot / "Models", ec);
	ec.clear();
	fs::create_directories(assetsRoot / "Textures", ec);

	struct PendingAsset {
		fs::path path;
		AssetBrowserItemType type = AssetBrowserItemType::Texture;
	};
	std::vector<PendingAsset> discovered;

	ec.clear();
	if (fs::exists(assetsRoot, ec) && !ec) {
		fs::recursive_directory_iterator iterator(
			assetsRoot,
			fs::directory_options::skip_permission_denied,
			ec);
		const fs::recursive_directory_iterator end;
		for (; !ec && iterator != end; iterator.increment(ec)) {
			if (ec) break;
			if (!iterator->is_regular_file(ec) || ec) {
				ec.clear();
				continue;
			}

			const fs::path filePath = iterator->path();
			const std::string extension = lowerAscii(filePath.extension().string());
			if (extension == ".obj") {
				discovered.push_back({ filePath, AssetBrowserItemType::ModelOBJ });
			}
			else if (extension == ".mtl") {
				discovered.push_back({ filePath, AssetBrowserItemType::MaterialMTL });
			}
			else if (isSupportedMaterialTextureFile(filePath)) {
				discovered.push_back({ filePath, AssetBrowserItemType::Texture });
			}
		}
	}

	std::sort(discovered.begin(), discovered.end(),
		[](const PendingAsset& a, const PendingAsset& b) {
			const int typeA = static_cast<int>(a.type);
			const int typeB = static_cast<int>(b.type);
			if (typeA != typeB) return typeA < typeB;
			return lowerAscii(a.path.generic_string()) < lowerAscii(b.path.generic_string());
		});

	// Fingerprint basado en ruta, tamano y fecha de modificacion. El catalogo solo
	// se reconstruye cuando algo dentro de Assets realmente cambia.
	unsigned long long fingerprint = 1469598103934665603ull;
	auto mixValue = [&fingerprint](unsigned long long value) {
		fingerprint ^= value;
		fingerprint *= 1099511628211ull;
	};
	std::hash<std::string> stringHasher;
	for (const PendingAsset& asset : discovered) {
		mixValue(static_cast<unsigned long long>(stringHasher(lowerAscii(asset.path.generic_string()))));
		ec.clear();
		const auto size = fs::file_size(asset.path, ec);
		if (!ec) mixValue(static_cast<unsigned long long>(size));
		ec.clear();
		const auto writeTime = fs::last_write_time(asset.path, ec);
		if (!ec) {
			mixValue(static_cast<unsigned long long>(writeTime.time_since_epoch().count()));
		}
	}

	if (!force && fingerprint == m_assetBrowserFingerprint) {
		return false;
	}

	std::vector<AssetBrowserItem> newItems;
	std::vector<std::unique_ptr<Texture>> newPreviewTextures;
	newItems.reserve(discovered.size());
	newPreviewTextures.reserve(std::min<size_t>(discovered.size(), 128));

	constexpr size_t kMaxPreviewTextures = 128;
	constexpr uintmax_t kMaxPreviewFileBytes = 64ull * 1024ull * 1024ull;
	size_t loadedPreviewCount = 0;

	for (const PendingAsset& discoveredAsset : discovered) {
		AssetBrowserItem item;
		item.type = discoveredAsset.type;
		item.name = discoveredAsset.path.filename().string();
		item.relativePath = makePortableAssetPath(discoveredAsset.path);

		if (item.type == AssetBrowserItemType::Texture &&
			loadedPreviewCount < kMaxPreviewTextures) {
			ec.clear();
			const uintmax_t fileBytes = fs::file_size(discoveredAsset.path, ec);
			const std::string extension = lowerAscii(discoveredAsset.path.extension().string());
			// DDS puede requerir el loader legacy. Si no esta disponible, el asset
			// sigue apareciendo en el browser pero sin miniatura.
			if (!ec && fileBytes <= kMaxPreviewFileBytes && extension != ".dds") {
				auto preview = std::make_unique<Texture>();
				if (preview &&
					SUCCEEDED(preview->init(m_device, discoveredAsset.path.string(), PNG)) &&
					preview->m_textureFromImg) {
					item.previewSRV = preview->m_textureFromImg;
					newPreviewTextures.push_back(std::move(preview));
					++loadedPreviewCount;
				}
			}
		}

		newItems.push_back(std::move(item));
	}

	// Esta funcion se ejecuta antes de ImGui::NewFrame(), de forma que los SRV
	// del catalogo anterior ya no estan referenciados por draw commands activos.
	m_assetBrowserItems = std::move(newItems);
	m_assetBrowserPreviewTextures = std::move(newPreviewTextures);
	m_assetBrowserFingerprint = fingerprint;

	MESSAGE("Main", "AssetBrowser", L"Asset catalog refreshed. Items: " << m_assetBrowserItems.size());
	return true;
}

void BaseApp::updateAssetBrowserCatalog(float deltaTime)
{
	if (!std::isfinite(deltaTime) || deltaTime < 0.0f) deltaTime = 0.0f;
	m_assetBrowserRefreshTimer += deltaTime;
	if (m_assetBrowserRefreshTimer < 1.0f) return;
	m_assetBrowserRefreshTimer = 0.0f;
	refreshAssetBrowserCatalog(false);
}

void BaseApp::handlePendingAssetBrowserAction()
{
	AssetBrowserRequest request{};
	if (!m_gui.consumeAssetBrowserRequest(request)) return;

	switch (request.action) {
	case AssetBrowserAction::ImportOBJ:
		if (!request.path.empty()) {
			importOBJModel(request.path);
		}
		break;

	case AssetBrowserAction::ApplyTexture:
	{
		const int actorIndex = m_gui.selectedActorIndex;
		if (actorIndex < 0 || actorIndex >= static_cast<int>(m_actors.size())) break;
		const EU::TSharedPointer<Actor> actor = m_actors[actorIndex];
		if (actor.isNull() || request.path.empty()) break;
		if (!applyMaterialTextureOverride(actor, request.materialSlot, request.channel, request.path)) {
			ERROR("Main", "AssetBrowser", "Could not apply the selected texture to the selected material.");
		}
		else {
			commitSceneHistory("Apply Texture");
		}
		break;
	}

	case AssetBrowserAction::Refresh:
		refreshAssetBrowserCatalog(true);
		break;

	case AssetBrowserAction::OpenAssetsFolder:
	{
		const std::filesystem::path assetsRoot = findProjectRoot() / "Assets";
		std::error_code ec;
		std::filesystem::create_directories(assetsRoot, ec);
		const HINSTANCE result = ShellExecuteA(
			m_window.m_hWnd,
			"open",
			assetsRoot.string().c_str(),
			nullptr,
			nullptr,
			SW_SHOWNORMAL);
		if (reinterpret_cast<INT_PTR>(result) <= 32) {
			ERROR("Main", "AssetBrowser", "Windows could not open the Assets folder.");
		}
		break;
	}

	default:
		break;
	}
}

bool BaseApp::importOBJFromDialog()
{
	char fileName[32768] = {};
	OPENFILENAMEA dialog{};
	dialog.lStructSize = sizeof(dialog);
	dialog.hwndOwner = m_window.m_hWnd;
	dialog.lpstrFilter = "Wavefront OBJ (*.obj)\0*.obj\0All files (*.*)\0*.*\0\0";
	dialog.lpstrFile = fileName;
	dialog.nMaxFile = static_cast<DWORD>(sizeof(fileName));
	dialog.lpstrTitle = "Import OBJ Mesh";
	dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | OFN_EXPLORER;
	dialog.lpstrDefExt = "obj";

	if (!GetOpenFileNameA(&dialog)) {
		const DWORD errorCode = CommDlgExtendedError();
		if (errorCode != 0) {
			ERROR("Main", "importOBJFromDialog", ("Windows file dialog failed. Code: " + std::to_string(errorCode)).c_str());
		}
		return false;
	}

	return importOBJModel(fileName);
}


BaseApp::MaterialTextureOverride* BaseApp::findMaterialTextureOverride(const Actor* actor,
	size_t materialSlot,
	MaterialTextureChannel channel)
{
	if (!actor) return nullptr;
	for (auto& textureOverride : m_materialTextureOverrides) {
		if (textureOverride.actor == actor &&
			textureOverride.materialSlot == materialSlot &&
			textureOverride.channel == channel) {
			return &textureOverride;
		}
	}
	return nullptr;
}

const BaseApp::MaterialTextureOverride* BaseApp::findMaterialTextureOverride(const Actor* actor,
	size_t materialSlot,
	MaterialTextureChannel channel) const
{
	if (!actor) return nullptr;
	for (const auto& textureOverride : m_materialTextureOverrides) {
		if (textureOverride.actor == actor &&
			textureOverride.materialSlot == materialSlot &&
			textureOverride.channel == channel) {
			return &textureOverride;
		}
	}
	return nullptr;
}

bool BaseApp::applyMaterialTextureOverride(const EU::TSharedPointer<Actor>& actor,
	size_t materialSlot,
	MaterialTextureChannel channel,
	const std::string& path)
{
	if (actor.isNull() || path.empty()) return false;
	auto meshRenderer = actor->getComponent<MeshRendererComponent>();
	if (meshRenderer.isNull()) return false;
	const std::vector<MaterialInstance*>& materials = meshRenderer->getMaterialInstances();
	if (materialSlot >= materials.size() || !materials[materialSlot]) return false;

	std::filesystem::path resolvedPath;
	if (!resolveMaterialOverrideTexturePath(path, resolvedPath)) {
		ERROR("Main", "MaterialEditor", ("Texture file could not be resolved: " + path).c_str());
		return false;
	}

	auto texture = std::make_unique<Texture>();
	const std::string extension = lowerAscii(resolvedPath.extension().string());
	const ExtensionType extensionType = extension == ".dds" ? DDS : PNG;
	const HRESULT hr = texture->init(m_device, resolvedPath.string(), extensionType);
	if (FAILED(hr)) {
		ERROR("Main", "MaterialEditor", ("Failed to load material texture: " + resolvedPath.string()).c_str());
		return false;
	}

	MaterialInstance* materialInstance = materials[materialSlot];
	MaterialTextureOverride* existing = findMaterialTextureOverride(actor.get(), materialSlot, channel);
	if (existing) {
		existing->texture = std::move(texture);
		existing->sourcePath = makePortableAssetPath(resolvedPath);
		setMaterialTextureForChannel(materialInstance, channel, existing->texture.get());
	}
	else {
		MaterialTextureOverride textureOverride;
		textureOverride.actor = actor.get();
		textureOverride.materialSlot = materialSlot;
		textureOverride.channel = channel;
		textureOverride.originalTexture = getMaterialTextureForChannel(materialInstance, channel);
		textureOverride.texture = std::move(texture);
		textureOverride.sourcePath = makePortableAssetPath(resolvedPath);
		setMaterialTextureForChannel(materialInstance, channel, textureOverride.texture.get());
		m_materialTextureOverrides.push_back(std::move(textureOverride));
	}

	const std::wstring pathW(resolvedPath.wstring());
	MESSAGE("Main", "MaterialEditor", L"Material texture applied: " << pathW);
	return true;
}

bool BaseApp::clearMaterialTextureOverride(const EU::TSharedPointer<Actor>& actor,
	size_t materialSlot,
	MaterialTextureChannel channel)
{
	if (actor.isNull()) return false;
	auto meshRenderer = actor->getComponent<MeshRendererComponent>();
	if (meshRenderer.isNull()) return false;
	const std::vector<MaterialInstance*>& materials = meshRenderer->getMaterialInstances();
	if (materialSlot >= materials.size() || !materials[materialSlot]) return false;

	for (auto it = m_materialTextureOverrides.begin(); it != m_materialTextureOverrides.end(); ++it) {
		if (it->actor == actor.get() && it->materialSlot == materialSlot && it->channel == channel) {
			setMaterialTextureForChannel(materials[materialSlot], channel, it->originalTexture);
			m_materialTextureOverrides.erase(it);
			MESSAGE("Main", "MaterialEditor", L"Material texture override reset to its imported/default texture.");
			return true;
		}
	}
	return false;
}

void BaseApp::handlePendingMaterialTextureEdit()
{
	MaterialTextureEditRequest request{};
	if (!m_gui.consumeMaterialTextureEditRequest(request)) return;

	const int actorIndex = m_gui.selectedActorIndex;
	if (actorIndex < 0 || actorIndex >= static_cast<int>(m_actors.size())) return;
	EU::TSharedPointer<Actor> actor = m_actors[actorIndex];
	if (actor.isNull()) return;

	if (request.clear) {
		if (clearMaterialTextureOverride(actor, request.materialSlot, request.channel)) {
			commitSceneHistory("Reset Material Texture");
		}
		return;
	}

	char fileName[32768] = {};
	OPENFILENAMEA dialog{};
	dialog.lStructSize = sizeof(dialog);
	dialog.hwndOwner = m_window.m_hWnd;
	dialog.lpstrFilter =
		"Image files (*.png;*.jpg;*.jpeg;*.tga;*.bmp;*.dds)\0*.png;*.jpg;*.jpeg;*.tga;*.bmp;*.dds\0"
		"PNG (*.png)\0*.png\0JPEG (*.jpg;*.jpeg)\0*.jpg;*.jpeg\0TGA (*.tga)\0*.tga\0BMP (*.bmp)\0*.bmp\0DDS (*.dds)\0*.dds\0\0";
	dialog.lpstrFile = fileName;
	dialog.nMaxFile = static_cast<DWORD>(sizeof(fileName));
	dialog.lpstrTitle = "Select Material Texture";
	dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | OFN_EXPLORER;

	if (!GetOpenFileNameA(&dialog)) {
		const DWORD errorCode = CommDlgExtendedError();
		if (errorCode != 0) {
			ERROR("Main", "MaterialEditor", ("Windows texture file dialog failed. Code: " + std::to_string(errorCode)).c_str());
		}
		return;
	}

	const std::filesystem::path selectedPath(fileName);
	std::filesystem::path projectTexturePath;
	if (!copyMaterialTextureIntoProject(selectedPath, actorIndex, request.materialSlot, request.channel, projectTexturePath)) {
		ERROR("Main", "MaterialEditor", "Could not copy the selected texture into Assets/Textures/MaterialOverrides.");
		return;
	}

	if (applyMaterialTextureOverride(actor, request.materialSlot, request.channel, projectTexturePath.string())) {
		commitSceneHistory("Replace Material Texture");
	}
}


bool BaseApp::loadPanoramicSkybox(const std::string& path)
{
	if (path.empty()) return false;

	std::filesystem::path resolvedPath;
	if (!resolveMaterialOverrideTexturePath(path, resolvedPath)) {
		const std::wstring pathW(path.begin(), path.end());
		MESSAGE("Main", "Skybox", L"Panoramic skybox texture was not found: " << pathW);
		return false;
	}

	const std::string extension = lowerAscii(resolvedPath.extension().string());
	if (extension == ".dds") {
		ERROR("Main", "Skybox", "DDS panoramas are not supported by the current image loader. Use PNG/JPG/TGA/BMP.");
		return false;
	}

	Texture loadedTexture;
	const ExtensionType extensionType = (extension == ".jpg" || extension == ".jpeg") ? JPG : PNG;
	HRESULT hr = loadedTexture.init(m_device, resolvedPath.string(), extensionType);
	if (FAILED(hr) || !loadedTexture.m_texture || !loadedTexture.m_textureFromImg) {
		ERROR("Main", "Skybox", "Failed to load panoramic skybox texture.");
		return false;
	}

	D3D11_TEXTURE2D_DESC desc{};
	loadedTexture.m_texture->GetDesc(&desc);
	if (desc.Height == 0) return false;
	const float aspect = static_cast<float>(desc.Width) / static_cast<float>(desc.Height);
	if (!std::isfinite(aspect) || aspect < 1.75f || aspect > 2.25f) {
		ERROR("Main", "Skybox", "Panoramic skybox should use an equirectangular 2:1 image (width approximately twice height).");
		return false;
	}

	if (!m_skyboxReady) {
		hr = m_skybox.init(m_device, &m_deviceContext, loadedTexture);
		if (FAILED(hr)) {
			ERROR("Main", "Skybox", "Failed to initialize panoramic skybox render resources.");
			return false;
		}
		m_skyboxReady = true;
	}
	else {
		m_skybox.setTexture(loadedTexture);
	}

	m_skyboxTex = std::move(loadedTexture);
	m_skyboxTexturePath = makePortableAssetPath(resolvedPath);
	m_gui.setSkyboxTextureDisplayName(resolvedPath.filename().string());
	m_skybox.setIntensity(m_gui.m_skyboxIntensity);
	m_skybox.setRotationDegrees(m_gui.m_skyboxRotationDegrees);
	m_skybox.setTint(m_gui.m_skyboxTint[0], m_gui.m_skyboxTint[1], m_gui.m_skyboxTint[2]);

	const std::wstring pathW(resolvedPath.wstring());
	MESSAGE("Main", "Skybox", L"Loaded panoramic skybox: " << pathW);
	return true;
}

bool BaseApp::copySkyboxTextureIntoProject(const std::string& sourcePath, std::string& outPortablePath)
{
	namespace fs = std::filesystem;
	outPortablePath.clear();
	const fs::path source(sourcePath);
	if (!isSupportedMaterialTextureFile(source)) return false;

	const std::string extension = lowerAscii(source.extension().string());
	if (extension == ".dds") return false;

	std::error_code ec;
	const fs::path skyboxDirectory = findProjectRoot() / "Assets" / "Skyboxes";
	fs::create_directories(skyboxDirectory, ec);
	if (ec) return false;

	const fs::path destination = (skyboxDirectory / source.filename()).lexically_normal();
	fs::path sourceAbsolute = fs::absolute(source, ec).lexically_normal();
	ec.clear();
	fs::path destinationAbsolute = fs::absolute(destination, ec).lexically_normal();
	if (sourceAbsolute != destinationAbsolute) {
		ec.clear();
		fs::copy_file(source, destination, fs::copy_options::overwrite_existing, ec);
		if (ec) return false;
	}

	outPortablePath = makePortableAssetPath(destination);
	return !outPortablePath.empty();
}

void BaseApp::resetSkyboxToDefault()
{
	m_gui.m_skyboxEnabled = true;
	m_gui.m_skyboxIntensity = 1.0f;
	m_gui.m_skyboxRotationDegrees = 0.0f;
	m_gui.m_skyboxTint[0] = 1.0f;
	m_gui.m_skyboxTint[1] = 1.0f;
	m_gui.m_skyboxTint[2] = 1.0f;

	if (!loadPanoramicSkybox(kDefaultSkyboxPath)) {
		m_skybox.destroy();
		m_skyboxTex.destroy();
		m_skyboxReady = false;
		m_skyboxTexturePath.clear();
		m_gui.setSkyboxTextureDisplayName("None");
		MESSAGE("Main", "Skybox", "Default panoramic skybox is unavailable. Continuing with editor clear color.");
	}
}

void BaseApp::handlePendingSkyboxEdit()
{
	SkyboxEditorRequest request{};
	if (!m_gui.consumeSkyboxEditorRequest(request)) return;

	if (request.action == SkyboxEditorAction::ResetDefault) {
		resetSkyboxToDefault();
		commitSceneHistory("Reset Skybox");
		return;
	}
	if (request.action != SkyboxEditorAction::BrowseTexture) return;

	char fileName[32768] = {};
	OPENFILENAMEA dialog{};
	dialog.lStructSize = sizeof(dialog);
	dialog.hwndOwner = m_window.m_hWnd;
	dialog.lpstrFilter =
		"Panoramic images (*.png;*.jpg;*.jpeg;*.tga;*.bmp)\0*.png;*.jpg;*.jpeg;*.tga;*.bmp\0"
		"PNG (*.png)\0*.png\0JPEG (*.jpg;*.jpeg)\0*.jpg;*.jpeg\0TGA (*.tga)\0*.tga\0BMP (*.bmp)\0*.bmp\0\0";
	dialog.lpstrFile = fileName;
	dialog.nMaxFile = static_cast<DWORD>(sizeof(fileName));
	dialog.lpstrTitle = "Select 2:1 Panoramic Skybox";
	dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | OFN_EXPLORER;

	if (!GetOpenFileNameA(&dialog)) {
		const DWORD errorCode = CommDlgExtendedError();
		if (errorCode != 0) {
			ERROR("Main", "Skybox", ("Windows skybox file dialog failed. Code: " + std::to_string(errorCode)).c_str());
		}
		return;
	}

	std::string portablePath;
	if (!copySkyboxTextureIntoProject(fileName, portablePath)) {
		ERROR("Main", "Skybox", "Could not copy the selected panorama into Assets/Skyboxes.");
		return;
	}
	if (loadPanoramicSkybox(portablePath)) {
		m_gui.m_skyboxEnabled = true;
		commitSceneHistory("Change Skybox");
	}
}

const BaseApp::ImportedMeshAsset* BaseApp::findImportedMeshAsset(const Actor* actor) const
{
	if (!actor) return nullptr;
	for (const auto& asset : m_importedMeshAssets) {
		if (asset && !asset->actor.isNull() && asset->actor.get() == actor) {
			return asset.get();
		}
	}
	return nullptr;
}

bool BaseApp::attachOBJAssetToActor(const std::string& path, const EU::TSharedPointer<Actor>& actor, bool autoPlace)
{
	if (path.empty() || actor.isNull()) {
		return false;
	}

	namespace fs = std::filesystem;
	fs::path resolvedPath;
	if (!resolveOBJAssetPath(path, actor->getName(), resolvedPath)) {
		ERROR("Main", "attachOBJAssetToActor", ("Could not resolve OBJ asset: " + path).c_str());
		return false;
	}

	const std::string portablePath = makePortableAssetPath(resolvedPath);
	if (const ImportedMeshAsset* existingAsset = findImportedMeshAsset(actor.get())) {
		if (existingAsset->builtinKind == BuiltinMeshKind::None &&
			!existingAsset->sourcePath.empty() &&
			lowerAscii(existingAsset->sourcePath) == lowerAscii(portablePath) &&
			actor->getComponent<MeshRendererComponent>()) {
			// Same live asset already attached.
			return true;
		}
		// The actor is being repurposed (for example a built-in slot being loaded
		// as an OBJ from a saved scene). Remove the previous ownership first.
		removeActorOwnedResources(actor.get());
	}

	std::string actorName = actor->getName();
	if (actorName.empty() || actorName == "Actor") {
		actorName = resolvedPath.stem().string();
		if (actorName.empty()) actorName = "Imported OBJ";
		actor->setName(actorName);
	}

	auto importedAsset = std::make_unique<ImportedMeshAsset>();
	importedAsset->model = std::make_unique<Model3D>(actorName, ModelType::OBJ);
	if (!importedAsset->model || !importedAsset->model->load(resolvedPath.string())) {
		ERROR("Main", "attachOBJAssetToActor", ("Failed to parse OBJ model: " + resolvedPath.string()).c_str());
		return false;
	}

	importedAsset->renderMesh = std::make_unique<Mesh>();
	if (!importedAsset->renderMesh) {
		return false;
	}

	const std::vector<MeshComponent>& meshes = importedAsset->model->GetMeshes();
	HRESULT hr = uploadImportedMeshes(m_device, meshes, *importedAsset->renderMesh);
	if (FAILED(hr)) {
		ERROR("Main", "attachOBJAssetToActor", "OBJ was parsed, but its GPU vertex/index buffers could not be created.");
		return false;
	}

	const std::unordered_map<std::string, ObjMaterialInfo> mtlMaterials = loadOBJMaterialLibrary(resolvedPath);
	std::unordered_map<std::string, Texture*> loadedTextureCache;
	std::vector<MaterialInstance*> materialPointers;
	materialPointers.reserve(meshes.size());
	importedAsset->materials.reserve(meshes.size());

	auto resolveAndLoadMap = [&](const std::string& mapPath, const ObjMaterialInfo& info) -> Texture* {
		std::filesystem::path texturePath;
		if (!resolveMaterialTexturePath(mapPath, info, resolvedPath, texturePath)) {
			if (!mapPath.empty()) {
				const std::wstring mapW(mapPath.begin(), mapPath.end());
				MESSAGE("Main", "OBJMaterial", L"Texture referenced by MTL was not found; fallback will be used: " << mapW);
			}
			return nullptr;
		}
		return loadImportedTexture(m_device, texturePath, importedAsset->textures, loadedTextureCache);
	};

	for (const MeshComponent& sourceMesh : meshes) {
		auto materialInstance = std::make_unique<MaterialInstance>();
		if (!materialInstance) {
			importedAsset->renderMesh->destroy();
			return false;
		}

		const ObjMaterialInfo* info = nullptr;
		auto materialIt = mtlMaterials.find(sourceMesh.m_materialName);
		if (materialIt != mtlMaterials.end()) info = &materialIt->second;

		const float alpha = info ? std::clamp(info->baseColor.w, 0.0f, 1.0f) : 1.0f;
		auto materialResource = std::make_unique<Material>();
		if (!materialResource) {
			importedAsset->renderMesh->destroy();
			return false;
		}
		materialResource->setShader(m_pbrMaterial.getShader());
		materialResource->setRasterizerState(m_pbrMaterial.getRasterizerState());
		materialResource->setDepthStencilState(m_pbrMaterial.getDepthStencilState());
		materialResource->setSamplerState(m_pbrMaterial.getSamplerState());
		materialResource->setDomain(alpha < 0.999f ? MaterialDomain::Transparent : MaterialDomain::Opaque);
		materialResource->setBlendMode(alpha < 0.999f ? BlendMode::Alpha : BlendMode::Opaque);
		materialInstance->setMaterial(materialResource.get());
		materialInstance->setAlbedo(&m_AlbedoSRV);
		materialInstance->setNormal(&m_NormalSRV);
		// White scalar texture keeps the numeric factor in MaterialParams intact.
		materialInstance->setMetallic(&m_AOSRV);
		materialInstance->setRoughness(&m_AOSRV);
		materialInstance->setAO(&m_AOSRV);
		materialInstance->setEmissive(&m_EmissiveSRV);

		MaterialParams& params = materialInstance->getParams();
		params.baseColor = info ? info->baseColor : XMFLOAT4(0.72f, 0.78f, 0.88f, 1.0f);
		params.metallic = info ? info->metallic : 0.0f;
		params.roughness = info ? info->roughness : 0.55f;
		params.ao = info ? info->ao : 1.0f;
		params.normalScale = info ? info->normalScale : 1.0f;
		params.emissiveStrength = 0.0f;
		params.alphaCutoff = alpha < 0.999f ? 0.0f : 0.5f;

		if (info) {
			if (Texture* texture = resolveAndLoadMap(info->albedoMap, *info)) materialInstance->setAlbedo(texture);
			if (Texture* texture = resolveAndLoadMap(info->normalMap, *info)) materialInstance->setNormal(texture);
			if (Texture* texture = resolveAndLoadMap(info->metallicMap, *info)) {
				materialInstance->setMetallic(texture);
				if (!info->hasMetallicValue) params.metallic = 1.0f;
			}
			if (Texture* texture = resolveAndLoadMap(info->roughnessMap, *info)) {
				materialInstance->setRoughness(texture);
				if (!info->hasRoughnessValue) params.roughness = 1.0f;
			}
			if (Texture* texture = resolveAndLoadMap(info->aoMap, *info)) materialInstance->setAO(texture);
			if (Texture* texture = resolveAndLoadMap(info->emissiveMap, *info)) {
				materialInstance->setEmissive(texture);
				params.emissiveStrength = (std::max)(1.0f,
					(std::max)(info->emissiveColor.x, (std::max)(info->emissiveColor.y, info->emissiveColor.z)));
			}
			else {
				const float emissiveMax = (std::max)(info->emissiveColor.x,
					(std::max)(info->emissiveColor.y, info->emissiveColor.z));
				if (emissiveMax > 0.0001f) {
					auto emissiveTexture = std::make_unique<Texture>();
					const auto toByte = [](float value) -> unsigned char {
						return static_cast<unsigned char>(std::clamp(value, 0.0f, 1.0f) * 255.0f + 0.5f);
					};
					if (SUCCEEDED(emissiveTexture->initSolidColor(m_device,
						toByte(info->emissiveColor.x),
						toByte(info->emissiveColor.y),
						toByte(info->emissiveColor.z), 255))) {
						materialInstance->setEmissive(emissiveTexture.get());
						params.emissiveStrength = 1.0f;
						importedAsset->textures.push_back(std::move(emissiveTexture));
					}
				}
			}
		}

		materialPointers.push_back(materialInstance.get());
		importedAsset->materialResources.push_back(std::move(materialResource));
		importedAsset->materials.push_back(std::move(materialInstance));
	}

	if (!attachRenderer(actor, *importedAsset->renderMesh, materialPointers, true)) {
		importedAsset->renderMesh->destroy();
		return false;
	}

	if (autoPlace) {
		EU::Vector3 spawnPosition(0.0f, 0.0f, 4.0f);
		EU::Vector3 spawnScale(1.0f, 1.0f, 1.0f);
		computeImportedPlacement(meshes, spawnPosition, spawnScale);
		EU::TSharedPointer<Transform> transform = actor->getComponent<Transform>();
		if (transform) {
			transform->setTransform(spawnPosition, EU::Vector3(0.0f, 0.0f, 0.0f), spawnScale);
		}
	}

	importedAsset->actor = actor;
	importedAsset->sourcePath = portablePath;
	m_importedMeshAssets.push_back(std::move(importedAsset));
	return true;
}

bool BaseApp::tryRestoreLegacyOBJActor(const EU::TSharedPointer<Actor>& actor)
{
	if (actor.isNull()) return false;
	if (actor->getComponent<MeshRendererComponent>()) return false;
	if (actor->getComponent<LightComponent>()) return false;

	const std::string actorName = actor->getName();
	if (actorName.empty()) return false;

	std::filesystem::path resolvedPath;
	const std::string legacyGuess = std::string("Assets/Models/") + actorName + ".obj";
	if (!resolveOBJAssetPath(legacyGuess, actorName, resolvedPath)) {
		return false;
	}

	if (!attachOBJAssetToActor(resolvedPath.string(), actor, false)) {
		return false;
	}

	const std::wstring pathW(resolvedPath.wstring());
	MESSAGE("Main", "loadScene", L"Recovered legacy OBJ actor '" << pathW << L"' from Assets/Models.");
	return true;
}

bool BaseApp::importOBJModel(const std::string& path)
{
	if (path.empty()) {
		return false;
	}

	namespace fs = std::filesystem;
	fs::path resolvedPath;
	if (!resolveOBJAssetPath(path, std::string(), resolvedPath)) {
		ERROR("Main", "importOBJModel", ("OBJ file does not exist or is not a valid .obj: " + path).c_str());
		return false;
	}

	std::string actorName = resolvedPath.stem().string();
	if (actorName.empty()) actorName = "Imported OBJ";

	EU::TSharedPointer<Actor> actor = EU::MakeShared<Actor>(m_device);
	if (actor.isNull()) {
		return false;
	}
	actor->setName(actorName);

	if (!attachOBJAssetToActor(resolvedPath.string(), actor, true)) {
		return false;
	}

	m_actors.push_back(actor);
	m_sceneGraph.addEntity(actor.get());
	m_gui.selectedActorIndex = static_cast<int>(m_actors.size()) - 1;

	const std::wstring pathW(resolvedPath.wstring());
	MESSAGE("Main", "importOBJModel", L"Imported OBJ successfully: " << pathW);
	if (!m_historyRestoreInProgress && !m_historyCaptureInProgress) {
		commitSceneHistory("Import OBJ");
	}
	return true;
}

EU::TSharedPointer<Actor> BaseApp::createLightActor(const std::string& name)
{
	EU::TSharedPointer<Actor> lightActor = EU::MakeShared<Actor>(m_device);
	if (lightActor.isNull()) {
		ERROR("Main", "createLightActor", "Failed to create Light Actor.");
		return lightActor;
	}

	size_t lightActorCount = 0;
	for (const auto& actor : m_actors) {
		if (!actor.isNull() && !actor->getComponent<LightComponent>().isNull()) {
			++lightActorCount;
		}
	}

	lightActor->setName(name.empty() ? "Light Actor " + std::to_string(lightActorCount + 1) : name);

	EU::TSharedPointer<LightComponent> lightComponent = lightActor->getComponent<LightComponent>();
	if (!lightComponent) {
		lightComponent = EU::MakeShared<LightComponent>();
		lightActor->addComponent(lightComponent);
	}

	lightComponent->getLightData().type = LightType::Point;
	lightComponent->getLightData().direction = EU::Vector3(-0.20f, -1.0f, 1.0f);
	lightComponent->getLightData().color = EU::Vector3(1.0f, 1.0f, 1.0f);
	lightComponent->getLightData().intensity = 1.0f;
	lightComponent->getLightData().range = 12.0f;
	lightComponent->setCastShadow(false);

	EU::TSharedPointer<Transform> transform = lightActor->getComponent<Transform>();
	if (transform) {
		const float lightOffset = static_cast<float>(lightActorCount) * 2.0f;
		transform->setTransform(EU::Vector3(lightOffset, 3.0f, 0.0f),
			EU::Vector3(0.0f, 0.0f, 0.0f),
			EU::Vector3(1.0f, 1.0f, 1.0f));
	}

	m_actors.push_back(lightActor);
	m_sceneGraph.addEntity(lightActor.get());
	return lightActor;
}


void BaseApp::registerBuiltinActorMetadata(const EU::TSharedPointer<Actor>& actor, BuiltinMeshKind kind)
{
	if (actor.isNull() || kind == BuiltinMeshKind::None) return;
	for (auto& asset : m_importedMeshAssets) {
		if (asset && !asset->actor.isNull() && asset->actor.get() == actor.get()) {
			asset->builtinKind = kind;
			asset->sourcePath.clear();
			return;
		}
	}
	auto metadata = std::make_unique<ImportedMeshAsset>();
	metadata->actor = actor;
	metadata->builtinKind = kind;
	m_importedMeshAssets.push_back(std::move(metadata));
}

void BaseApp::removeActorOwnedResources(const Actor* actor)
{
	if (!actor) return;

	m_materialTextureOverrides.erase(
		std::remove_if(m_materialTextureOverrides.begin(), m_materialTextureOverrides.end(),
			[actor](const MaterialTextureOverride& textureOverride) {
				return textureOverride.actor == actor;
			}),
		m_materialTextureOverrides.end());

	for (auto it = m_importedMeshAssets.begin(); it != m_importedMeshAssets.end();) {
		if (*it && !(*it)->actor.isNull() && (*it)->actor.get() == actor) {
			if ((*it)->renderMesh) {
				(*it)->renderMesh->destroy();
			}
			it = m_importedMeshAssets.erase(it);
		}
		else {
			++it;
		}
	}
}

bool BaseApp::attachBuiltinMeshToActor(BuiltinMeshKind kind, const EU::TSharedPointer<Actor>& actor)
{
	if (actor.isNull() || kind == BuiltinMeshKind::None) return false;

	Mesh* mesh = nullptr;
	MaterialInstance* sourceInstance = nullptr;
	bool castShadow = true;
	switch (kind) {
	case BuiltinMeshKind::Cube:
		mesh = &m_cyberGunRenderMesh;
		sourceInstance = &m_cyberGunMaterial;
		break;
	case BuiltinMeshKind::Pyramid:
		mesh = &m_drakefireRenderMesh;
		sourceInstance = &m_drakefireMaterial;
		break;
	case BuiltinMeshKind::Floor:
		mesh = &m_toadRenderMesh;
		sourceInstance = &m_toadMaterial;
		castShadow = false;
		break;
	default:
		return false;
	}
	if (!mesh || !sourceInstance || !sourceInstance->getMaterial()) return false;

	removeActorOwnedResources(actor.get());

	auto metadata = std::make_unique<ImportedMeshAsset>();
	auto materialResource = std::make_unique<Material>();
	auto materialInstance = std::make_unique<MaterialInstance>();
	if (!metadata || !materialResource || !materialInstance) return false;

	Material* sourceMaterial = sourceInstance->getMaterial();
	materialResource->setShader(sourceMaterial->getShader());
	materialResource->setRasterizerState(sourceMaterial->getRasterizerState());
	materialResource->setDepthStencilState(sourceMaterial->getDepthStencilState());
	materialResource->setSamplerState(sourceMaterial->getSamplerState());
	materialResource->setDomain(sourceMaterial->getDomain());
	materialResource->setBlendMode(sourceMaterial->getBlendMode());

	materialInstance->setMaterial(materialResource.get());
	materialInstance->setAlbedo(sourceInstance->getAlbedo());
	materialInstance->setNormal(sourceInstance->getNormal());
	materialInstance->setMetallic(sourceInstance->getMetallic());
	materialInstance->setRoughness(sourceInstance->getRoughness());
	materialInstance->setAO(sourceInstance->getAO());
	materialInstance->setEmissive(sourceInstance->getEmissive());
	materialInstance->getParams() = sourceInstance->getParams();

	MaterialInstance* materialPtr = materialInstance.get();
	if (!attachRenderer(actor, *mesh, *materialPtr, castShadow)) {
		return false;
	}

	metadata->actor = actor;
	metadata->builtinKind = kind;
	metadata->materialResources.push_back(std::move(materialResource));
	metadata->materials.push_back(std::move(materialInstance));
	m_importedMeshAssets.push_back(std::move(metadata));
	return true;
}

bool BaseApp::tryRestoreLegacyBuiltinActor(const EU::TSharedPointer<Actor>& actor)
{
	if (actor.isNull() || actor->getComponent<MeshRendererComponent>() || actor->getComponent<LightComponent>()) {
		return false;
	}

	BuiltinMeshKind kind = BuiltinMeshKind::None;
	const std::string name = actor->getName();
	if (name == "Demo Cube") kind = BuiltinMeshKind::Cube;
	else if (name == "Demo Pyramid") kind = BuiltinMeshKind::Pyramid;
	else if (name == "Demo Floor") kind = BuiltinMeshKind::Floor;
	if (kind == BuiltinMeshKind::None) return false;

	return attachBuiltinMeshToActor(kind, actor);
}

std::string BaseApp::makeUniqueActorName(const std::string& desiredName, const Actor* ignoreActor) const
{
	std::string base = desiredName.empty() ? "Actor" : desiredName;
	auto exists = [&](const std::string& candidate) {
		for (const auto& actor : m_actors) {
			if (!actor.isNull() && actor.get() != ignoreActor && actor->getName() == candidate) {
				return true;
			}
		}
		return false;
	};

	if (!exists(base)) return base;
	for (int suffix = 2; suffix < 100000; ++suffix) {
		const std::string candidate = base + " " + std::to_string(suffix);
		if (!exists(candidate)) return candidate;
	}
	return base + " Copy";
}

void BaseApp::copyActorEditableState(const EU::TSharedPointer<Actor>& source,
	const EU::TSharedPointer<Actor>& destination)
{
	if (source.isNull() || destination.isNull()) return;

	auto sourceTransform = source->getComponent<Transform>();
	auto destinationTransform = destination->getComponent<Transform>();
	if (sourceTransform && destinationTransform) {
		EU::Vector3 position = sourceTransform->getPosition();
		// A small offset makes the duplicate immediately visible/selectable.
		position.x += 0.35f;
		position.z += 0.35f;
		destinationTransform->setTransform(position,
			sourceTransform->getRotation(),
			sourceTransform->getScale());
	}

	auto sourceRenderer = source->getComponent<MeshRendererComponent>();
	auto destinationRenderer = destination->getComponent<MeshRendererComponent>();
	if (sourceRenderer && destinationRenderer) {
		destinationRenderer->setVisible(sourceRenderer->isVisible());
		destinationRenderer->setCastShadow(sourceRenderer->canCastShadow());

		const auto& sourceMaterials = sourceRenderer->getMaterialInstances();
		const auto& destinationMaterials = destinationRenderer->getMaterialInstances();
		const size_t materialCount = (std::min)(sourceMaterials.size(), destinationMaterials.size());
		for (size_t i = 0; i < materialCount; ++i) {
			if (!sourceMaterials[i] || !destinationMaterials[i]) continue;
			destinationMaterials[i]->getParams() = sourceMaterials[i]->getParams();
			Material* sourceMaterial = sourceMaterials[i]->getMaterial();
			Material* destinationMaterial = destinationMaterials[i]->getMaterial();
			if (sourceMaterial && destinationMaterial) {
				destinationMaterial->setDomain(sourceMaterial->getDomain());
				destinationMaterial->setBlendMode(sourceMaterial->getBlendMode());
			}
		}

		struct OverrideCopy {
			size_t slot = 0;
			MaterialTextureChannel channel = MaterialTextureChannel::Albedo;
			std::string path;
		};
		std::vector<OverrideCopy> copies;
		for (const auto& textureOverride : m_materialTextureOverrides) {
			if (textureOverride.actor == source.get() && !textureOverride.sourcePath.empty()) {
				copies.push_back({ textureOverride.materialSlot, textureOverride.channel, textureOverride.sourcePath });
			}
		}
		for (const OverrideCopy& copy : copies) {
			applyMaterialTextureOverride(destination, copy.slot, copy.channel, copy.path);
		}
	}

	auto sourceLight = source->getComponent<LightComponent>();
	if (sourceLight) {
		auto destinationLight = destination->getComponent<LightComponent>();
		if (!destinationLight) {
			destinationLight = EU::MakeShared<LightComponent>();
			destination->addComponent(destinationLight);
		}
		destinationLight->getLightData() = sourceLight->getLightData();
		destinationLight->setCastShadow(sourceLight->canCastShadow());
	}
}

bool BaseApp::duplicateActorAtIndex(int actorIndex)
{
	if (actorIndex < 0 || actorIndex >= static_cast<int>(m_actors.size())) return false;
	const EU::TSharedPointer<Actor> source = m_actors[actorIndex];
	if (source.isNull()) return false;

	EU::TSharedPointer<Actor> duplicate = EU::MakeShared<Actor>(m_device);
	if (duplicate.isNull()) return false;
	duplicate->setName(makeUniqueActorName(source->getName() + " Copy"));

	const ImportedMeshAsset* sourceAsset = findImportedMeshAsset(source.get());
	auto sourceRenderer = source->getComponent<MeshRendererComponent>();
	bool rendererReady = !sourceRenderer;

	if (sourceAsset && !sourceAsset->sourcePath.empty()) {
		rendererReady = attachOBJAssetToActor(sourceAsset->sourcePath, duplicate, false);
	}
	else if (sourceAsset && sourceAsset->builtinKind != BuiltinMeshKind::None) {
		rendererReady = attachBuiltinMeshToActor(sourceAsset->builtinKind, duplicate);
	}
	else if (sourceRenderer && sourceRenderer->getMesh()) {
		// Generic fallback for a mesh that predates asset metadata. It works in
		// the current session; all built-in and OBJ assets created by V13 carry
		// metadata and therefore use one of the persistent branches above.
		auto metadata = std::make_unique<ImportedMeshAsset>();
		std::vector<MaterialInstance*> materialPointers;
		for (MaterialInstance* sourceInstance : sourceRenderer->getMaterialInstances()) {
			if (!sourceInstance || !sourceInstance->getMaterial()) continue;
			auto materialResource = std::make_unique<Material>();
			auto materialInstance = std::make_unique<MaterialInstance>();
			if (!materialResource || !materialInstance) return false;

			Material* sourceMaterial = sourceInstance->getMaterial();
			materialResource->setShader(sourceMaterial->getShader());
			materialResource->setRasterizerState(sourceMaterial->getRasterizerState());
			materialResource->setDepthStencilState(sourceMaterial->getDepthStencilState());
			materialResource->setSamplerState(sourceMaterial->getSamplerState());
			materialResource->setDomain(sourceMaterial->getDomain());
			materialResource->setBlendMode(sourceMaterial->getBlendMode());

			materialInstance->setMaterial(materialResource.get());
			materialInstance->setAlbedo(sourceInstance->getAlbedo());
			materialInstance->setNormal(sourceInstance->getNormal());
			materialInstance->setMetallic(sourceInstance->getMetallic());
			materialInstance->setRoughness(sourceInstance->getRoughness());
			materialInstance->setAO(sourceInstance->getAO());
			materialInstance->setEmissive(sourceInstance->getEmissive());
			materialInstance->getParams() = sourceInstance->getParams();
			materialPointers.push_back(materialInstance.get());
			metadata->materialResources.push_back(std::move(materialResource));
			metadata->materials.push_back(std::move(materialInstance));
		}
		if (!materialPointers.empty()) {
			rendererReady = attachRenderer(duplicate, *sourceRenderer->getMesh(),
				materialPointers, sourceRenderer->canCastShadow());
			metadata->actor = duplicate;
			m_importedMeshAssets.push_back(std::move(metadata));
		}
	}

	if (!rendererReady) {
		removeActorOwnedResources(duplicate.get());
		return false;
	}

	copyActorEditableState(source, duplicate);
	m_actors.push_back(duplicate);
	m_sceneGraph.addEntity(duplicate.get());
	m_gui.selectedActorIndex = static_cast<int>(m_actors.size()) - 1;

	const std::string duplicateName = duplicate->getName();
	const std::wstring nameW(duplicateName.begin(), duplicateName.end());
	MESSAGE("Main", "duplicateActor", L"Duplicated actor: " << nameW);
	return true;
}

bool BaseApp::deleteActorAtIndex(int actorIndex)
{
	if (actorIndex < 0 || actorIndex >= static_cast<int>(m_actors.size())) return false;
	EU::TSharedPointer<Actor> victim = m_actors[actorIndex];
	if (victim.isNull()) return false;

	Actor* rawActor = victim.get();
	const std::string actorName = victim->getName();
	m_sceneGraph.removeEntity(rawActor);
	removeActorOwnedResources(rawActor);

	if (!m_cyberGun.isNull() && m_cyberGun.get() == rawActor) m_cyberGun.reset();
	if (!m_drakefirePistol.isNull() && m_drakefirePistol.get() == rawActor) m_drakefirePistol.reset();
	if (!m_sciFiToad.isNull() && m_sciFiToad.get() == rawActor) m_sciFiToad.reset();
	if (!m_directionalLightActor.isNull() && m_directionalLightActor.get() == rawActor) m_directionalLightActor.reset();

	m_actors.erase(m_actors.begin() + actorIndex);
	if (m_actors.empty()) {
		m_gui.selectedActorIndex = -1;
	}
	else {
		m_gui.selectedActorIndex = (std::min)(actorIndex, static_cast<int>(m_actors.size()) - 1);
	}

	if (m_directionalLightActor.isNull()) {
		for (const auto& actor : m_actors) {
			if (actor.isNull()) continue;
			auto light = actor->getComponent<LightComponent>();
			if (light && light->getLightData().type == LightType::Directional) {
				m_directionalLightActor = actor;
				break;
			}
		}
	}

	const std::wstring nameW(actorName.begin(), actorName.end());
	MESSAGE("Main", "deleteActor", L"Deleted actor: " << nameW);
	return true;
}

bool BaseApp::renameActorAtIndex(int actorIndex, const std::string& newName)
{
	if (actorIndex < 0 || actorIndex >= static_cast<int>(m_actors.size())) return false;
	EU::TSharedPointer<Actor> actor = m_actors[actorIndex];
	if (actor.isNull()) return false;

	const size_t first = newName.find_first_not_of(" \t\r\n");
	if (first == std::string::npos) return false;
	const size_t last = newName.find_last_not_of(" \t\r\n");
	const std::string trimmed = newName.substr(first, last - first + 1);
	if (trimmed.empty()) return false;

	actor->setName(makeUniqueActorName(trimmed, actor.get()));
	return true;
}

void BaseApp::clearCurrentSceneActors()
{
	m_renderScene.clear();
	m_sceneGraph.destroy();

	m_materialTextureOverrides.clear();

	m_cyberGun.reset();
	m_drakefirePistol.reset();
	m_sciFiToad.reset();
	m_directionalLightActor.reset();
	m_actors.clear();

	for (auto& asset : m_importedMeshAssets) {
		if (asset && asset->renderMesh) {
			asset->renderMesh->destroy();
		}
	}
	m_importedMeshAssets.clear();
	m_gui.selectedActorIndex = -1;
}

void BaseApp::createNewScene()
{
	clearCurrentSceneActors();
	m_currentScenePath.clear();
	resetSkyboxToDefault();

	EU::TSharedPointer<Actor> lightActor = createLightActor("Light Actor 1");
	if (!lightActor.isNull()) {
		auto light = lightActor->getComponent<LightComponent>();
		if (light) {
			light->getLightData().type = LightType::Directional;
			light->getLightData().direction = EU::Vector3(-0.20f, -1.0f, 1.0f);
			light->getLightData().color = EU::Vector3(1.0f, 1.0f, 1.0f);
			light->getLightData().intensity = 1.0f;
			light->setCastShadow(true);
		}
		m_directionalLightActor = lightActor;
		m_gui.selectedActorIndex = 0;
	}

	MESSAGE("Main", "Scene", "Created a new empty scene.");
}

bool BaseApp::isValidSceneFileHeader(const std::string& path) const
{
	if (path.empty()) return false;
	std::ifstream stream(path);
	std::string magic;
	int version = 0;
	return stream.is_open() &&
		(stream >> magic >> version) &&
		magic == "WVSCENE" &&
		version >= 1 &&
		version <= kCurrentSceneVersion;
}


bool BaseApp::canUndoSceneHistory() const
{
	return !m_sceneHistory.empty() && m_sceneHistoryCursor > 0 && m_sceneHistoryCursor < m_sceneHistory.size();
}

bool BaseApp::canRedoSceneHistory() const
{
	return !m_sceneHistory.empty() && (m_sceneHistoryCursor + 1) < m_sceneHistory.size();
}

void BaseApp::cleanupSceneHistory()
{
	std::error_code ec;
	if (!m_sceneHistoryDirectory.empty()) {
		std::filesystem::remove_all(std::filesystem::path(m_sceneHistoryDirectory), ec);
	}
	m_sceneHistory.clear();
	m_sceneHistoryCursor = 0;
	m_sceneHistorySequence = 0;
	m_sceneHistoryDirectory.clear();
}

bool BaseApp::captureSceneHistorySnapshot(const std::string& label, SceneHistorySnapshot& outSnapshot)
{
	if (m_historyCaptureInProgress || m_historyRestoreInProgress) return false;

	namespace fs = std::filesystem;
	std::error_code ec;
	if (m_sceneHistoryDirectory.empty()) {
		fs::path root = fs::temp_directory_path(ec);
		if (ec) {
			ec.clear();
			root = fs::path("intermediate");
		}
		root /= "WildvineEngineHistory";
		root /= std::to_string(static_cast<unsigned long long>(GetCurrentProcessId()));
		fs::create_directories(root, ec);
		if (ec) return false;
		m_sceneHistoryDirectory = root.string();
	}

	fs::path snapshotPath(m_sceneHistoryDirectory);
	char fileName[64] = {};
	sprintf_s(fileName, "snapshot_%06llu.wvscene", ++m_sceneHistorySequence);
	snapshotPath /= fileName;

	const std::string activeScenePath = m_currentScenePath;
	m_historyCaptureInProgress = true;
	const bool saved = saveScene(snapshotPath.string());
	m_historyCaptureInProgress = false;
	m_currentScenePath = activeScenePath;
	if (!saved) {
		fs::remove(snapshotPath, ec);
		return false;
	}

	outSnapshot.snapshotPath = snapshotPath.string();
	outSnapshot.scenePath = activeScenePath;
	outSnapshot.label = label.empty() ? "Edit" : label;
	outSnapshot.selectedActorIndex = m_gui.selectedActorIndex;
	return true;
}

void BaseApp::trimSceneHistory()
{
	constexpr size_t kMaxHistoryStates = 40;
	std::error_code ec;
	while (m_sceneHistory.size() > kMaxHistoryStates) {
		if (!m_sceneHistory.front().snapshotPath.empty()) {
			std::filesystem::remove(m_sceneHistory.front().snapshotPath, ec);
			ec.clear();
		}
		m_sceneHistory.erase(m_sceneHistory.begin());
		if (m_sceneHistoryCursor > 0) --m_sceneHistoryCursor;
	}
}

void BaseApp::resetSceneHistory(const std::string& label)
{
	cleanupSceneHistory();
	SceneHistorySnapshot initial{};
	if (captureSceneHistorySnapshot(label, initial)) {
		m_sceneHistory.push_back(std::move(initial));
		m_sceneHistoryCursor = 0;
	}
	m_gui.setHistoryAvailability(canUndoSceneHistory(), canRedoSceneHistory());
}

void BaseApp::commitSceneHistory(const std::string& label)
{
	if (m_historyCaptureInProgress || m_historyRestoreInProgress) return;

	SceneHistorySnapshot snapshot{};
	if (!captureSceneHistorySnapshot(label, snapshot)) return;

	// Evitar snapshots duplicados (por ejemplo un slider que se activa pero vuelve
	// exactamente a su valor anterior antes de soltar el mouse).
	auto filesEqual = [](const std::string& a, const std::string& b) -> bool {
		if (a.empty() || b.empty()) return false;
		std::error_code ec;
		const auto sizeA = std::filesystem::file_size(a, ec);
		if (ec) return false;
		ec.clear();
		const auto sizeB = std::filesystem::file_size(b, ec);
		if (ec || sizeA != sizeB) return false;
		std::ifstream fa(a, std::ios::binary);
		std::ifstream fb(b, std::ios::binary);
		if (!fa || !fb) return false;
		constexpr size_t kBufferSize = 4096;
		char ba[kBufferSize] = {};
		char bb[kBufferSize] = {};
		while (fa && fb) {
			fa.read(ba, static_cast<std::streamsize>(kBufferSize));
			fb.read(bb, static_cast<std::streamsize>(kBufferSize));
			const std::streamsize ca = fa.gcount();
			const std::streamsize cb = fb.gcount();
			if (ca != cb || std::memcmp(ba, bb, static_cast<size_t>(ca)) != 0) return false;
		}
		return true;
	};

	if (!m_sceneHistory.empty() && m_sceneHistoryCursor < m_sceneHistory.size() &&
		filesEqual(m_sceneHistory[m_sceneHistoryCursor].snapshotPath, snapshot.snapshotPath)) {
		std::error_code ec;
		std::filesystem::remove(snapshot.snapshotPath, ec);
		return;
	}

	// Una edicion nueva despues de Undo invalida la rama de Redo.
	if (!m_sceneHistory.empty() && (m_sceneHistoryCursor + 1) < m_sceneHistory.size()) {
		std::error_code ec;
		for (size_t i = m_sceneHistoryCursor + 1; i < m_sceneHistory.size(); ++i) {
			std::filesystem::remove(m_sceneHistory[i].snapshotPath, ec);
			ec.clear();
		}
		m_sceneHistory.erase(m_sceneHistory.begin() + static_cast<std::ptrdiff_t>(m_sceneHistoryCursor + 1), m_sceneHistory.end());
	}

	m_sceneHistory.push_back(std::move(snapshot));
	m_sceneHistoryCursor = m_sceneHistory.size() - 1;
	trimSceneHistory();
	m_gui.setHistoryAvailability(canUndoSceneHistory(), canRedoSceneHistory());
}

bool BaseApp::restoreSceneHistorySnapshot(size_t historyIndex)
{
	if (historyIndex >= m_sceneHistory.size()) return false;
	const SceneHistorySnapshot snapshot = m_sceneHistory[historyIndex];
	if (snapshot.snapshotPath.empty() || !isValidSceneFileHeader(snapshot.snapshotPath)) return false;

	m_historyRestoreInProgress = true;
	clearCurrentSceneActors();
	const bool loaded = loadScene(snapshot.snapshotPath);
	m_historyRestoreInProgress = false;
	if (!loaded) {
		ERROR("Main", "History", "Failed to restore an Undo/Redo snapshot.");
		return false;
	}

	m_currentScenePath = snapshot.scenePath;
	if (m_actors.empty() || snapshot.selectedActorIndex < 0) {
		m_gui.selectedActorIndex = -1;
	}
	else {
		m_gui.selectedActorIndex = (std::max)(0, (std::min)(snapshot.selectedActorIndex,
			static_cast<int>(m_actors.size()) - 1));
	}
	return true;
}

bool BaseApp::undoSceneHistory()
{
	if (!canUndoSceneHistory()) return false;
	const size_t target = m_sceneHistoryCursor - 1;
	if (!restoreSceneHistorySnapshot(target)) return false;
	m_sceneHistoryCursor = target;
	const std::string label = m_sceneHistory[m_sceneHistoryCursor + 1].label;
	const std::wstring labelW(label.begin(), label.end());
	MESSAGE("Main", "Undo", L"Undo: " << labelW);
	m_gui.setHistoryAvailability(canUndoSceneHistory(), canRedoSceneHistory());
	return true;
}

bool BaseApp::redoSceneHistory()
{
	if (!canRedoSceneHistory()) return false;
	const size_t target = m_sceneHistoryCursor + 1;
	if (!restoreSceneHistorySnapshot(target)) return false;
	m_sceneHistoryCursor = target;
	const std::string label = m_sceneHistory[m_sceneHistoryCursor].label;
	const std::wstring labelW(label.begin(), label.end());
	MESSAGE("Main", "Redo", L"Redo: " << labelW);
	m_gui.setHistoryAvailability(canUndoSceneHistory(), canRedoSceneHistory());
	return true;
}

bool BaseApp::openSceneFromDialog()
{
	namespace fs = std::filesystem;
	std::error_code ec;
	fs::create_directories("Saved", ec);
	std::string initialDirectory;
	const fs::path savedPath = fs::absolute("Saved", ec);
	if (!ec) initialDirectory = savedPath.string();

	char fileName[32768] = {};
	OPENFILENAMEA dialog{};
	dialog.lStructSize = sizeof(dialog);
	dialog.hwndOwner = m_window.m_hWnd;
	dialog.lpstrFilter = "Wildvine Scene (*.wvscene)\0*.wvscene\0All files (*.*)\0*.*\0\0";
	dialog.lpstrFile = fileName;
	dialog.nMaxFile = static_cast<DWORD>(sizeof(fileName));
	dialog.lpstrTitle = "Open Wildvine Scene";
	dialog.lpstrDefExt = "wvscene";
	dialog.lpstrInitialDir = initialDirectory.empty() ? nullptr : initialDirectory.c_str();
	dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | OFN_EXPLORER;

	if (!GetOpenFileNameA(&dialog)) {
		const DWORD errorCode = CommDlgExtendedError();
		if (errorCode != 0) {
			ERROR("Main", "openScene", ("Windows open-scene dialog failed. Code: " + std::to_string(errorCode)).c_str());
		}
		return false;
	}

	if (!isValidSceneFileHeader(fileName)) {
		ERROR("Main", "openScene", "The selected file is not a supported Wildvine scene.");
		return false;
	}

	clearCurrentSceneActors();
	if (!loadScene(fileName)) {
		ERROR("Main", "openScene", "Scene parsing failed. A fresh scene will be created.");
		createNewScene();
		return false;
	}
	m_gui.selectedActorIndex = m_actors.empty() ? -1 : 0;
	return true;
}

bool BaseApp::saveSceneAsFromDialog()
{
	namespace fs = std::filesystem;
	std::error_code ec;
	fs::create_directories("Saved", ec);

	char fileName[32768] = {};
	std::string suggested = "Scene.wvscene";
	if (!m_currentScenePath.empty()) {
		const fs::path currentPath(m_currentScenePath);
		if (!currentPath.filename().empty()) suggested = currentPath.filename().string();
	}
	std::snprintf(fileName, sizeof(fileName), "%s", suggested.c_str());

	OPENFILENAMEA dialog{};
	dialog.lStructSize = sizeof(dialog);
	dialog.hwndOwner = m_window.m_hWnd;
	dialog.lpstrFilter = "Wildvine Scene (*.wvscene)\0*.wvscene\0All files (*.*)\0*.*\0\0";
	dialog.lpstrFile = fileName;
	dialog.nMaxFile = static_cast<DWORD>(sizeof(fileName));
	std::string initialDirectory;
	const fs::path savedPath = fs::absolute("Saved", ec);
	if (!ec) initialDirectory = savedPath.string();

	dialog.lpstrTitle = "Save Wildvine Scene As";
	dialog.lpstrDefExt = "wvscene";
	dialog.lpstrInitialDir = initialDirectory.empty() ? nullptr : initialDirectory.c_str();
	dialog.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | OFN_EXPLORER;

	if (!GetSaveFileNameA(&dialog)) {
		const DWORD errorCode = CommDlgExtendedError();
		if (errorCode != 0) {
			ERROR("Main", "saveSceneAs", ("Windows save-scene dialog failed. Code: " + std::to_string(errorCode)).c_str());
		}
		return false;
	}
	return saveScene(fileName);
}

void BaseApp::handlePendingSceneEditorAction()
{
	SceneEditorRequest request{};
	if (!m_gui.consumeSceneEditorRequest(request)) return;

	switch (request.action) {
	case SceneEditorAction::NewScene:
		createNewScene();
		commitSceneHistory("New Scene");
		break;
	case SceneEditorAction::OpenScene:
		if (openSceneFromDialog()) resetSceneHistory("Open Scene");
		break;
	case SceneEditorAction::SaveSceneAs:
		saveSceneAsFromDialog();
		break;
	case SceneEditorAction::DuplicateActor:
		if (duplicateActorAtIndex(request.actorIndex)) commitSceneHistory("Duplicate Actor");
		break;
	case SceneEditorAction::DeleteActor:
		if (deleteActorAtIndex(request.actorIndex)) commitSceneHistory("Delete Actor");
		break;
	case SceneEditorAction::RenameActor:
		if (renameActorAtIndex(request.actorIndex, request.text)) commitSceneHistory("Rename Actor");
		break;
	case SceneEditorAction::Undo:
		undoSceneHistory();
		break;
	case SceneEditorAction::Redo:
		redoSceneHistory();
		break;
	default:
		break;
	}
	m_gui.setHistoryAvailability(canUndoSceneHistory(), canRedoSceneHistory());
}

bool BaseApp::saveScene(const std::string& path)
{
	if (path.empty()) {
		ERROR("Main", "saveScene", "Scene path cannot be empty.");
		return false;
	}

	std::error_code directoryError;
	const std::filesystem::path scenePath(path);
	const std::filesystem::path parentPath = scenePath.parent_path();
	if (!parentPath.empty()) {
		std::filesystem::create_directories(parentPath, directoryError);
		if (directoryError) {
			ERROR("Main", "saveScene", ("Failed to create scene directory: " + parentPath.string()).c_str());
			return false;
		}
	}

	std::ofstream stream(scenePath, std::ios::trunc);
	if (!stream.is_open()) {
		ERROR("Main", "saveScene", ("Failed to open scene file for writing: " + path).c_str());
		return false;
	}

	stream << "WVSCENE " << kCurrentSceneVersion << "\n";
	stream << "SKYBOX "
		<< (m_gui.m_skyboxEnabled ? 1 : 0) << " "
		<< std::quoted(m_skyboxTexturePath) << " "
		<< m_gui.m_skyboxIntensity << " "
		<< m_gui.m_skyboxRotationDegrees << " "
		<< m_gui.m_skyboxTint[0] << " "
		<< m_gui.m_skyboxTint[1] << " "
		<< m_gui.m_skyboxTint[2] << "\n";
	stream << "ACTOR_COUNT " << m_actors.size() << "\n";

	for (size_t actorIndex = 0; actorIndex < m_actors.size(); ++actorIndex) {
		const EU::TSharedPointer<Actor>& actor = m_actors[actorIndex];
		if (actor.isNull()) {
			continue;
		}

		stream << "ACTOR " << actorIndex << " " << std::quoted(actor->getName()) << "\n";

		if (const ImportedMeshAsset* importedAsset = findImportedMeshAsset(actor.get())) {
			if (!importedAsset->sourcePath.empty()) {
				stream << "OBJ_ASSET " << std::quoted(importedAsset->sourcePath) << "\n";
			}
			else if (importedAsset->builtinKind != BuiltinMeshKind::None) {
				stream << "BUILTIN_MESH " << static_cast<int>(importedAsset->builtinKind) << "\n";
			}
		}

		EU::TSharedPointer<Transform> transform = actor->getComponent<Transform>();
		if (transform) {
			const EU::Vector3& position = transform->getPosition();
			const EU::Vector3& rotation = transform->getRotation();
			const EU::Vector3& scale = transform->getScale();
			stream << "POSITION " << position.x << " " << position.y << " " << position.z << "\n";
			stream << "ROTATION " << rotation.x << " " << rotation.y << " " << rotation.z << "\n";
			stream << "SCALE " << scale.x << " " << scale.y << " " << scale.z << "\n";
		}

		EU::TSharedPointer<MeshRendererComponent> meshRenderer = actor->getComponent<MeshRendererComponent>();
		if (meshRenderer) {
			stream << "VISIBLE " << (meshRenderer->isVisible() ? 1 : 0) << "\n";
			stream << "CAST_SHADOW " << (meshRenderer->canCastShadow() ? 1 : 0) << "\n";

			const std::vector<MaterialInstance*>& materials = meshRenderer->getMaterialInstances();
			if (materials.size() > kMaxSerializedMaterialsPerActor) {
				ERROR("Main", "saveScene", "Too many materials on an actor to serialize safely.");
				return false;
			}
			stream << "MATERIAL_COUNT " << materials.size() << "\n";
			for (size_t i = 0; i < materials.size(); ++i) {
				MaterialInstance* materialInstance = materials[i];
				if (!materialInstance) {
					stream << "MATERIAL " << i << " 0 0 1 1 1 1 0 1 1 1 1 0.5\n";
					continue;
				}

				Material* material = materialInstance->getMaterial();
				const MaterialParams& params = materialInstance->getParams();
				const int domain = material ? static_cast<int>(material->getDomain()) : 0;
				const int blendMode = material ? static_cast<int>(material->getBlendMode()) : 0;

				stream << "MATERIAL " << i << " "
					<< domain << " "
					<< blendMode << " "
					<< params.baseColor.x << " "
					<< params.baseColor.y << " "
					<< params.baseColor.z << " "
					<< params.baseColor.w << " "
					<< params.metallic << " "
					<< params.roughness << " "
					<< params.ao << " "
					<< params.normalScale << " "
					<< params.emissiveStrength << " "
					<< params.alphaCutoff << "\n";
			}

			for (const MaterialTextureOverride& textureOverride : m_materialTextureOverrides) {
				if (textureOverride.actor != actor.get() ||
					textureOverride.materialSlot >= materials.size() ||
					textureOverride.sourcePath.empty()) {
					continue;
				}
				stream << "MATERIAL_TEXTURE "
					<< textureOverride.materialSlot << " "
					<< static_cast<int>(textureOverride.channel) << " "
					<< std::quoted(textureOverride.sourcePath) << "\n";
			}
		}

		EU::TSharedPointer<LightComponent> lightComponent = actor->getComponent<LightComponent>();
		if (lightComponent) {
			const LightData& light = lightComponent->getLightData();
			stream << "LIGHT_COMPONENT "
				<< static_cast<int>(light.type) << " "
				<< light.color.x << " "
				<< light.color.y << " "
				<< light.color.z << " "
				<< light.intensity << " "
				<< light.direction.x << " "
				<< light.direction.y << " "
				<< light.direction.z << " "
				<< light.range << " "
				<< light.spotAngle << " "
				<< (lightComponent->canCastShadow() ? 1 : 0) << "\n";
		}

		stream << "END_ACTOR\n";
		if (!stream) {
			ERROR("Main", "saveScene", "Failed while writing scene data.");
			return false;
		}
	}

	stream << "LIGHT "
		<< m_constantBufferStruct.LightDir.x << " "
		<< m_constantBufferStruct.LightDir.y << " "
		<< m_constantBufferStruct.LightDir.z << " "
		<< m_constantBufferStruct.LightColor.x << " "
		<< m_constantBufferStruct.LightColor.y << " "
		<< m_constantBufferStruct.LightColor.z << "\n";

	stream << "END_SCENE\n";
	stream.flush();
	if (!stream) {
		ERROR("Main", "saveScene", "Failed to flush scene file to disk.");
		return false;
	}

	m_currentScenePath = path;
	if (!m_historyCaptureInProgress && !m_historyRestoreInProgress &&
		!m_sceneHistory.empty() && m_sceneHistoryCursor < m_sceneHistory.size()) {
		m_sceneHistory[m_sceneHistoryCursor].scenePath = path;
	}
	if (!m_historyCaptureInProgress) {
		const std::wstring pathW(path.begin(), path.end());
		MESSAGE("Main", "saveScene", L"Saved scene to '" << pathW << L"'");
	}
	return true;
}

bool BaseApp::loadScene(const std::string& path)
{
	if (path.empty()) {
		return false;
	}
	const std::filesystem::path scenePath(path);
	std::ifstream stream(scenePath);
	if (!stream.is_open()) {
		return false;
	}

	std::string token;
	if (!(stream >> token) || token != "WVSCENE") {
		return false;
	}

	int version = 0;
	if (!(stream >> version) || version < 1 || version > kCurrentSceneVersion) {
		return false;
	}
	if (version < 6) {
		resetSkyboxToDefault();
	}

	size_t declaredActorCount = 0;
	bool hasDeclaredActorCount = false;
	bool foundEndScene = false;
	EU::TSharedPointer<Actor> currentActor;

	while (stream >> token) {
		if (token == "SKYBOX") {
			int enabled = 1;
			std::string texturePath;
			float intensity = 1.0f;
			float rotation = 0.0f;
			float tintR = 1.0f, tintG = 1.0f, tintB = 1.0f;
			if (!(stream >> enabled >> std::quoted(texturePath) >> intensity >> rotation >> tintR >> tintG >> tintB) ||
				!isFinite(intensity) || !isFinite(rotation) ||
				!isFinite(tintR) || !isFinite(tintG) || !isFinite(tintB)) {
				return false;
			}
			m_gui.m_skyboxEnabled = enabled != 0;
			m_gui.m_skyboxIntensity = (std::max)(0.0f, intensity);
			m_gui.m_skyboxRotationDegrees = rotation;
			m_gui.m_skyboxTint[0] = (std::max)(0.0f, tintR);
			m_gui.m_skyboxTint[1] = (std::max)(0.0f, tintG);
			m_gui.m_skyboxTint[2] = (std::max)(0.0f, tintB);
			if (!texturePath.empty() && !loadPanoramicSkybox(texturePath)) {
				MESSAGE("Main", "loadScene", "Serialized skybox texture is unavailable. Keeping the current/default panorama.");
			}
		}
		else if (token == "ACTOR_COUNT") {
			if (!(stream >> declaredActorCount) || declaredActorCount > kMaxSerializedActors) {
				return false;
			}
			hasDeclaredActorCount = true;
		}
		else if (token == "ACTOR") {
			size_t actorIndex = 0;
			std::string actorName;
			if (!(stream >> actorIndex >> std::quoted(actorName))) {
				return false;
			}
			if (actorIndex >= kMaxSerializedActors ||
				(hasDeclaredActorCount && actorIndex >= declaredActorCount)) {
				return false;
			}

			currentActor.reset();
			while (actorIndex >= m_actors.size()) {
				EU::TSharedPointer<Actor> newActor = EU::MakeShared<Actor>(m_device);
				if (newActor.isNull()) {
					return false;
				}
				newActor->setName("Actor " + std::to_string(m_actors.size() + 1));
				m_actors.push_back(newActor);
				m_sceneGraph.addEntity(newActor.get());
			}

			currentActor = m_actors[actorIndex];
			if (currentActor.isNull()) {
				return false;
			}
			currentActor->setName(actorName);
			if (isSerializedLightActorName(actorName)) {
				ensureDefaultLightComponent(currentActor);
				if (m_directionalLightActor.isNull()) {
					m_directionalLightActor = currentActor;
				}
			}
			else {
				// v1-v4 did not persist procedural mesh identity. Restore the demo
				// shape by name early so following MATERIAL/VISIBLE tokens apply.
				if (version < 5) {
					tryRestoreLegacyBuiltinActor(currentActor);
				}
				if (version < 3) {
					// v1/v2 scenes only stored the actor itself. Rebuild a matching OBJ
					// before reading visibility/material tokens so those values can be restored too.
					tryRestoreLegacyOBJActor(currentActor);
				}
			}
		}
		else if (token == "OBJ_ASSET" && !currentActor.isNull()) {
			std::string assetPath;
			if (!(stream >> std::quoted(assetPath)) || assetPath.empty()) {
				return false;
			}

			if (!attachOBJAssetToActor(assetPath, currentActor, false)) {
				const std::wstring assetPathW(assetPath.begin(), assetPath.end());
				MESSAGE("Main", "loadScene", L"OBJ asset is unavailable. Actor will remain in the hierarchy without a mesh: " << assetPathW);
			}
		}
		else if (token == "BUILTIN_MESH" && !currentActor.isNull()) {
			int builtinKindValue = 0;
			if (!(stream >> builtinKindValue) ||
				builtinKindValue < static_cast<int>(BuiltinMeshKind::Cube) ||
				builtinKindValue > static_cast<int>(BuiltinMeshKind::Floor)) {
				return false;
			}
			if (!attachBuiltinMeshToActor(static_cast<BuiltinMeshKind>(builtinKindValue), currentActor)) {
				ERROR("Main", "loadScene", "Failed to reconstruct built-in mesh actor.");
				return false;
			}
		}
		else if (token == "POSITION" && !currentActor.isNull()) {
			float x = 0.0f, y = 0.0f, z = 0.0f;
			if (!(stream >> x >> y >> z) || !isFinite(EU::Vector3(x, y, z))) return false;
			EU::TSharedPointer<Transform> transform = currentActor->getComponent<Transform>();
			if (transform) transform->setPosition(EU::Vector3(x, y, z));
		}
		else if (token == "ROTATION" && !currentActor.isNull()) {
			float x = 0.0f, y = 0.0f, z = 0.0f;
			if (!(stream >> x >> y >> z) || !isFinite(EU::Vector3(x, y, z))) return false;
			EU::TSharedPointer<Transform> transform = currentActor->getComponent<Transform>();
			if (transform) transform->setRotation(EU::Vector3(x, y, z));
		}
		else if (token == "SCALE" && !currentActor.isNull()) {
			float x = 1.0f, y = 1.0f, z = 1.0f;
			if (!(stream >> x >> y >> z) || !isFinite(EU::Vector3(x, y, z))) return false;
			EU::TSharedPointer<Transform> transform = currentActor->getComponent<Transform>();
			if (transform) transform->setScale(EU::Vector3(x, y, z));
		}
		else if (token == "VISIBLE" && !currentActor.isNull()) {
			int value = 1;
			if (!(stream >> value)) return false;
			EU::TSharedPointer<MeshRendererComponent> meshRenderer = currentActor->getComponent<MeshRendererComponent>();
			if (meshRenderer) meshRenderer->setVisible(value != 0);
		}
		else if (token == "CAST_SHADOW" && !currentActor.isNull()) {
			int value = 1;
			if (!(stream >> value)) return false;
			EU::TSharedPointer<MeshRendererComponent> meshRenderer = currentActor->getComponent<MeshRendererComponent>();
			if (meshRenderer) meshRenderer->setCastShadow(value != 0);
		}
		else if (token == "MATERIAL_COUNT") {
			size_t materialCount = 0;
			if (!(stream >> materialCount) || materialCount > kMaxSerializedMaterialsPerActor) {
				return false;
			}
		}
		else if (token == "MATERIAL" && !currentActor.isNull()) {
			size_t materialIndex = 0;
			int domain = 0;
			int blendMode = 0;
			MaterialParams params{};
			if (!(stream >> materialIndex
				>> domain
				>> blendMode
				>> params.baseColor.x
				>> params.baseColor.y
				>> params.baseColor.z
				>> params.baseColor.w
				>> params.metallic
				>> params.roughness
				>> params.ao
				>> params.normalScale)) {
				return false;
			}

			if (version >= 2) {
				if (!(stream >> params.emissiveStrength >> params.alphaCutoff)) return false;
			}
			else {
				if (!(stream >> params.alphaCutoff)) return false;
			}

			if (!areFinite(params)) return false;
			if (materialIndex >= kMaxSerializedMaterialsPerActor) return false;
			if (domain < static_cast<int>(MaterialDomain::Opaque) ||
				domain > static_cast<int>(MaterialDomain::Transparent)) {
				domain = static_cast<int>(MaterialDomain::Opaque);
			}
			if (blendMode < static_cast<int>(BlendMode::Opaque) ||
				blendMode > static_cast<int>(BlendMode::PremultipliedAlpha)) {
				blendMode = static_cast<int>(BlendMode::Opaque);
			}

			EU::TSharedPointer<MeshRendererComponent> meshRenderer = currentActor->getComponent<MeshRendererComponent>();
			if (meshRenderer) {
				const std::vector<MaterialInstance*>& materials = meshRenderer->getMaterialInstances();
				if (materialIndex < materials.size() && materials[materialIndex]) {
					materials[materialIndex]->getParams() = params;
					Material* material = materials[materialIndex]->getMaterial();
					if (material) {
						material->setDomain(static_cast<MaterialDomain>(domain));
						material->setBlendMode(static_cast<BlendMode>(blendMode));
					}
				}
			}
		}
		else if (token == "MATERIAL_TEXTURE" && !currentActor.isNull()) {
			size_t materialIndex = 0;
			int channelValue = 0;
			std::string texturePath;
			if (!(stream >> materialIndex >> channelValue >> std::quoted(texturePath))) {
				return false;
			}
			if (materialIndex >= kMaxSerializedMaterialsPerActor || !isValidMaterialTextureChannel(channelValue)) {
				return false;
			}

			const MaterialTextureChannel channel = static_cast<MaterialTextureChannel>(channelValue);
			if (!applyMaterialTextureOverride(currentActor, materialIndex, channel, texturePath)) {
				const std::wstring texturePathW(texturePath.begin(), texturePath.end());
				MESSAGE("Main", "loadScene", L"Material texture override is unavailable; imported/default texture will be used: " << texturePathW);
			}
		}
		else if (token == "LIGHT_COMPONENT" && !currentActor.isNull()) {
			int type = 0;
			int castShadow = 0;
			LightData light{};
			if (!(stream >> type
				>> light.color.x
				>> light.color.y
				>> light.color.z
				>> light.intensity
				>> light.direction.x
				>> light.direction.y
				>> light.direction.z
				>> light.range
				>> light.spotAngle
				>> castShadow)) {
				return false;
			}

			if (!isFinite(light.color) ||
				!isFinite(light.intensity) ||
				!isFinite(light.direction) ||
				!isFinite(light.range) ||
				!isFinite(light.spotAngle)) {
				return false;
			}

			if (type < static_cast<int>(LightType::Directional) || type > static_cast<int>(LightType::Spot)) {
				type = static_cast<int>(LightType::Point);
			}
			light.type = static_cast<LightType>(type);

			EU::TSharedPointer<LightComponent> lightComponent = currentActor->getComponent<LightComponent>();
			if (!lightComponent) {
				lightComponent = EU::MakeShared<LightComponent>();
				currentActor->addComponent(lightComponent);
			}
			lightComponent->getLightData() = light;
			lightComponent->setCastShadow(castShadow != 0);
		}
		else if (token == "LIGHT") {
			if (!(stream >> m_constantBufferStruct.LightDir.x
				>> m_constantBufferStruct.LightDir.y
				>> m_constantBufferStruct.LightDir.z
				>> m_constantBufferStruct.LightColor.x
				>> m_constantBufferStruct.LightColor.y
				>> m_constantBufferStruct.LightColor.z)) {
				return false;
			}
			if (!isFinite(m_constantBufferStruct.LightDir) ||
				!isFinite(m_constantBufferStruct.LightColor)) {
				return false;
			}

			if (!m_directionalLightActor.isNull()) {
				EU::TSharedPointer<LightComponent> lightComponent = m_directionalLightActor->getComponent<LightComponent>();
				if (lightComponent) {
					lightComponent->getLightData().direction = m_constantBufferStruct.LightDir;
					lightComponent->getLightData().color = m_constantBufferStruct.LightColor;
				}
			}
		}
		else if (token == "END_ACTOR") {
			// Legacy scenes did not persist procedural mesh identity. Recover the
			// original demo actors by name before trying the OBJ fallback.
			if (!currentActor.isNull()) {
				tryRestoreLegacyBuiltinActor(currentActor);
				// Scene versions prior to v3 did not persist imported asset paths.
				// Recover an OBJ automatically when an empty actor has a matching
				// Assets/Models/<ActorName>.obj file (for example SampleSphere.obj).
				tryRestoreLegacyOBJActor(currentActor);
			}
			currentActor.reset();
		}
		else if (token == "END_SCENE") {
			foundEndScene = true;
			break;
		}
		else {
			// Versiones soportadas tienen un vocabulario cerrado. Rechazar tokens
			// desconocidos evita aceptar silenciosamente archivos truncados/corruptos.
			return false;
		}
	}

	if (!foundEndScene) {
		return false;
	}

	// Reconcile the runtime actor list with the serialized actor count. This is
	// what makes deletions persistent even though startup initially creates the
	// built-in demo actors before loading DefaultScene.
	if (hasDeclaredActorCount) {
		while (m_actors.size() > declaredActorCount) {
			if (!deleteActorAtIndex(static_cast<int>(m_actors.size()) - 1)) {
				return false;
			}
		}
		if (m_actors.size() != declaredActorCount) {
			return false;
		}
	}

	m_directionalLightActor.reset();
	for (const auto& actor : m_actors) {
		if (actor.isNull()) continue;
		auto light = actor->getComponent<LightComponent>();
		if (light && light->getLightData().type == LightType::Directional) {
			m_directionalLightActor = actor;
			break;
		}
	}

	m_currentScenePath = path;
	if (!m_historyRestoreInProgress) {
		const std::wstring pathW(path.begin(), path.end());
		MESSAGE("Main", "loadScene", L"Loaded scene from '" << pathW << L"'");
	}
	return true;
}



