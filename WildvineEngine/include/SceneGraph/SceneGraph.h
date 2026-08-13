/**
 * @file SceneGraph.h
 * @brief Declara la API de SceneGraph dentro del subsistema SceneGraph.
 * @ingroup scenegraph
 */
#pragma once
#include "Prerequisites.h"
#include <unordered_set>

class Entity;
class DeviceContext;
class Camera;
class RenderScene;

/**
 * @class SceneGraph
 * @brief Administra la jerarquia de entidades y su actualizacion espacial.
 *
 * El `SceneGraph` registra entidades, resuelve relaciones padre-hijo y actualiza
 * las matrices de mundo antes de generar la informacion necesaria para render.
 */
class 
SceneGraph {
public:
	SceneGraph()  = default;
	~SceneGraph() = default;

	void 
	init();

	/**
	 * @brief Registra una entidad dentro del grafo.
	 * @param e Entidad a registrar.
	 */
	void 
	addEntity(Entity* e);  // registra en el grafo

	/**
	 * @brief Elimina una entidad del grafo si esta registrada.
	 * @param e Entidad a retirar.
	 */
	void 
	removeEntity(Entity* e);

	bool 
	isAncestor(Entity* possibleAncestor, Entity* node) const;

	bool
	attach(Entity* child, Entity* parent);

	bool
	detach(Entity* child);

	void 
	update(float deltaTime, DeviceContext& deviceContext);
	
	void 
	render(DeviceContext& deviceContext);

	void
	gatherRenderScene(RenderScene& outScene, const Camera& camera);

	void
	destroy();
private:
	void
	updateWorldRecursive(Entity* node, const XMMATRIX& parentWorld, std::unordered_set<Entity*>& visited);

	bool 
	isRoot(Entity* e) const;

	bool
	isRegistered(Entity* e) const;

private:
	//std::vector<EU::TSharedPointer<Entity>> m_entities;
public:
	std::vector<Entity*> m_entities; ///< Entidades registradas en el grafo.
};


