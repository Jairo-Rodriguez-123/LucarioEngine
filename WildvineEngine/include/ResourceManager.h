/**
 * @file ResourceManager.h
 * @brief Declara la API de ResourceManager dentro del subsistema Core.
 * @ingroup core
 */
#pragma once
#include "Prerequisites.h"
#include "IResource.h"

class 
ResourceManager {
public:
	ResourceManager()  = default;
	~ResourceManager() = default;

	// Singleton
	static ResourceManager& getInstance() {
		static ResourceManager instance;
		return instance;
	}

	ResourceManager(const ResourceManager&) = delete;
	ResourceManager& operator=(const ResourceManager&) = delete;

	/// Obtener o cargar un recurso de tipo T (T debe heredar de IResource).
	template<typename T, typename... Args>
	std::shared_ptr<T> GetOrLoad(const std::string& key,
                               const std::string& filename,
                               Args&&... args) {
		static_assert(std::is_base_of<IResource, T>::value,
                      "T debe heredar de IResource");
		// 1. Ya existe el recurso en el cach?
		auto it = m_resources.find(key);
		if (it != m_resources.end()) {
			// Intentar castear al tipo correcto. Si la clave existe con otro tipo
			// o con un recurso fallido, liberarlo antes de reemplazarlo para no
			// dejar recursos GPU/CPU vivos fuera del cache.
			auto existing = std::dynamic_pointer_cast<T>(it->second);
			if (existing && existing->GetState() == ResourceState::Loaded) {
				return existing; // Flyweight: reutilizamos la instancia
			}
			if (it->second) {
				it->second->unload();
			}
			m_resources.erase(it);
		}

		// 2. No existe o no est cargado -> crearlo y cargarlo
		std::shared_ptr<T> resource = std::make_shared<T>(key, std::forward<Args>(args)...);

		if (!resource->load(filename)) {
			// Puedes manejar errores ms fino aqu
			return nullptr;
		}


		// 3. Guardar en el cach y devolver
		m_resources[key] = resource;
		return resource;
	}

	/// Obtener un recurso ya cargado, sin cargarlo si no existe.
	template<typename T>
	std::shared_ptr<T> Get(const std::string& key) const
	{
		auto it = m_resources.find(key);
		if (it == m_resources.end()) return nullptr;

		return std::dynamic_pointer_cast<T>(it->second);
	}

	/// Liberar un recurso especfico
	void Unload(const std::string& key)
	{
		auto it = m_resources.find(key);
		if (it != m_resources.end()) {
			it->second->unload();
			m_resources.erase(it);
		}
	}

	/// Liberar todos los recursos
	void UnloadAll()
	{
		for (auto& [key, res] : m_resources) {
			if (res) {
				res->unload();
			}
		}
		m_resources.clear();
	}

private:
	std::unordered_map<std::string, std::shared_ptr<IResource>> m_resources;
};

