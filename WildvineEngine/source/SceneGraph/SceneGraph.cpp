/**
 * @file SceneGraph.cpp
 * @brief Implementa la logica de SceneGraph dentro del subsistema SceneGraph.
 * @ingroup scenegraph
 */
#include "SceneGraph/SceneGraph.h"
#include "SceneGraph/HierarchyComponent.h"
#include "ECS/Entity.h"
#include "ECS/Transform.h"
#include "ECS/LightComponent.h"
#include "ECS/MeshRendererComponent.h"
#include "DeviceContext.h"
#include "EngineUtilities/Utilities/Camera.h"
#include "Rendering/Material.h"
#include "Rendering/MaterialInstance.h"
#include "Rendering/RenderScene.h"
#include <cmath>

void SceneGraph::init() {
	m_entities.clear();
}

void SceneGraph::destroy() {
	for (Entity* e : m_entities)
	{
		if (!e) continue;
		auto h = e->getComponent<HierarchyComponent>();
		if (h)
		{
			h->m_parent = nullptr;
			h->m_children.clear();
		}
	}

	m_entities.clear();
}

void 
SceneGraph::addEntity(Entity* e) {
	if (!e) {
		return;
	}
	if (isRegistered(e)) {
		return;
	}

	//	// Validar que existen los componentes minimos
	if (!e->getComponent<Transform>()) {
		e->addComponent(EU::MakeShared<Transform>());
		e->getComponent<Transform>()->init();
	}
	if (!e->getComponent<HierarchyComponent>()) {
		e->addComponent(EU::MakeShared<HierarchyComponent>());
		e->getComponent<HierarchyComponent>()->init();
	}

	m_entities.push_back(e);
}

void 
SceneGraph::removeEntity(Entity* e) {
	if (!e) return;
	if (!isRegistered(e)) return;

	// 1) Detach de su padre (si tiene)
	detach(e);

	// 2) Reparent de hijos a null (roots) o detach total
	auto h = e->getComponent<HierarchyComponent>();
	if (h)
	{
		// Copia local para no invalidar mientras iteras
		auto childrenCopy = h->m_children;
		for (Entity* c : childrenCopy)
		{
			if (!c) continue;
			// detach del padre (que es e)
			auto hc = c->getComponent<HierarchyComponent>();
			if (hc && hc->m_parent == e)
				hc->m_parent = nullptr;

			// quitar referencia en e
			h->removeChild(c);

			// marcar dirty para recalcular world
			auto wt = c->getComponent<Transform>();
			//if (wt) wt->dirty = true;
			//markWorldDirtyRecursive(wt);
		}

		h->m_children.clear();
	}

	// 3) eliminar del registro
	m_entities.erase(std::remove(m_entities.begin(), m_entities.end(), e), m_entities.end());
}

bool 
SceneGraph::isAncestor(Entity* possibleAncestor, Entity* node) const {
	// Walk upwards from node. The visited set also protects this validation
	// from a hierarchy that was already corrupted through direct component access.
	if (!possibleAncestor || !node) return false;

	std::unordered_set<Entity*> visited;
	while (node && visited.insert(node).second)
	{
		auto h = node->getComponent<HierarchyComponent>();
		if (!h || !h->m_parent) {
			return false;
		}
		if (h->m_parent == possibleAncestor) {
			return true;
		}
		node = h->m_parent;
	}
	return false;
}

bool
SceneGraph::isRoot(Entity* e) const {
	
	if (!e) return false;
	auto h = e->getComponent<HierarchyComponent>();
	return (!h || h->m_parent == nullptr);
}

bool 
SceneGraph::isRegistered(Entity* e) const {
	return std::find(m_entities.begin(), m_entities.end(), e) != m_entities.end();
}

bool 
SceneGraph::attach(Entity* child, Entity* parent)
{
	if (!child || !parent) return false;
	if (child == parent) return false;

	// Registro autom�tico
	addEntity(child);
	addEntity(parent);

	// Evita ciclos: parent no puede estar debajo de child
	if (isAncestor(child, parent)) return false;

	// Si child ya tiene padre, detach
	detach(child);

	auto hc = child->getComponent<HierarchyComponent>();
	auto hp = parent->getComponent<HierarchyComponent>();
	if (!hc || !hp) return false;

	hc->m_parent = parent;
	hp->addChild(child);

	//markWorldDirtyRecursive(wt);
	return true;
}

bool 
SceneGraph::detach(Entity* child) {
	if (!child) return false;

	auto hc = child->getComponent<HierarchyComponent>();
	if (!hc) return false;

	Entity* parent = hc->m_parent;
	if (!parent) return true; // ya estaba root

	auto hp = parent->getComponent<HierarchyComponent>();
	if (hp) hp->removeChild(child);

	hc->m_parent = nullptr;

	//markWorldDirtyRecursive(wt);
	return true;
}

void
SceneGraph::update(float deltaTime, DeviceContext& deviceContext) {
	// Actualiza todas las entidades.
	for (Entity* e : m_entities)
	{
		if (!e) continue;
		e->update(deltaTime, deviceContext);
	}

	// Propaga las matrices world desde los roots. visited protege contra
	// ciclos incluso si alguien modifica HierarchyComponent directamente.
	std::unordered_set<Entity*> visited;
	for (Entity* e : m_entities)
	{
		if (e && isRoot(e))
		{
			updateWorldRecursive(e, XMMatrixIdentity(), visited);
		}
	}

	// Una jerarquia corrupta puede no tener root. Procesar cualquier entidad
	// restante desde identidad evita matrices world obsoletas y recursion infinita.
	for (Entity* e : m_entities) {
		if (e && visited.find(e) == visited.end()) {
			updateWorldRecursive(e, XMMatrixIdentity(), visited);
		}
	}
}

void
SceneGraph::updateWorldRecursive(Entity* node, const XMMATRIX& parentWorld, std::unordered_set<Entity*>& visited) {
	if (!node || !isRegistered(node) || !visited.insert(node).second) {
		return;
	}

	auto t = node->getComponent<Transform>();
	auto h = node->getComponent<HierarchyComponent>();
	if (!t || !h) {
		return;
	}

	// Transform::matrix es LOCAL (S*R*T). World = Local * ParentWorld.
	const XMMATRIX worldMatrix = t->matrix * parentWorld;
	t->worldMatrix = worldMatrix;

	for (Entity* c : h->m_children) {
		updateWorldRecursive(c, worldMatrix, visited);
	}
}

void SceneGraph::render(DeviceContext& deviceContext) {
	// Render all entities
	for (auto& e : m_entities) {
		if (e) {
			e->render(deviceContext);
		}
	}
}

void
SceneGraph::gatherRenderScene(RenderScene& outScene, const Camera& camera) {
	outScene.clear();
	for (Entity* entity : m_entities)
	{
		if (!entity) {
			continue;
		}

		auto transform = entity->getComponent<Transform>();
		auto lightComponent = entity->getComponent<LightComponent>();
		if (lightComponent) {
			LightData light = lightComponent->getLightData();
			light.castShadow = lightComponent->canCastShadow();
			if (light.type == LightType::Directional || light.type == LightType::Spot) {
				const float dirLengthSq = light.direction.x * light.direction.x +
					light.direction.y * light.direction.y +
					light.direction.z * light.direction.z;
				if (!std::isfinite(dirLengthSq) || dirLengthSq <= 1e-12f) {
					light.direction = EU::Vector3(0.0f, -1.0f, 0.0f);
				}
				else {
					const float invLength = 1.0f / std::sqrt(dirLengthSq);
					light.direction = light.direction * invLength;
				}
			}
			if (transform) {
				XMFLOAT4X4 lightWorld{};
				XMStoreFloat4x4(&lightWorld, transform->worldMatrix);
				light.position = EU::Vector3(lightWorld._41, lightWorld._42, lightWorld._43);
			}
			outScene.directionalLights.push_back(light);
		}

		auto meshRenderer = entity->getComponent<MeshRendererComponent>();
		if (!meshRenderer || !transform || !meshRenderer->isVisible()) {
			continue;
		}

		RenderObject renderObject{};
		renderObject.mesh = meshRenderer->getMesh();
		renderObject.materialInstance = meshRenderer->getMaterialInstance();
		renderObject.materialInstances = meshRenderer->getMaterialInstances();
		renderObject.world = transform->worldMatrix;
		renderObject.castShadow = meshRenderer->canCastShadow();

		if (!renderObject.mesh) {
			continue;
		}

		EU::Vector3 cameraPos = camera.getPosition();
		XMFLOAT4X4 worldMatrix{};
		XMStoreFloat4x4(&worldMatrix, transform->worldMatrix);
		EU::Vector3 objectPos(worldMatrix._41, worldMatrix._42, worldMatrix._43);
		float dx = objectPos.x - cameraPos.x;
		float dy = objectPos.y - cameraPos.y;
		float dz = objectPos.z - cameraPos.z;
		renderObject.distanceToCamera = dx * dx + dy * dy + dz * dz;

		bool hasOpaque = false;
		bool hasTransparent = false;
		auto classifyMaterial = [&](MaterialInstance* instance) {
			if (!instance || !instance->getMaterial()) {
				return;
			}
			if (instance->getMaterial()->getDomain() == MaterialDomain::Transparent) {
				hasTransparent = true;
			}
			else {
				hasOpaque = true;
			}
		};

		if (!renderObject.materialInstances.empty()) {
			for (MaterialInstance* instance : renderObject.materialInstances) {
				classifyMaterial(instance);
			}
		}
		else {
			classifyMaterial(renderObject.materialInstance);
		}

		// No usable material means there is nothing this renderer can draw.
		if (!hasOpaque && !hasTransparent) {
			continue;
		}

		if (hasOpaque) {
			RenderObject opaqueObject = renderObject;
			opaqueObject.transparent = false;
			outScene.opaqueObjects.push_back(std::move(opaqueObject));
		}
		if (hasTransparent) {
			RenderObject transparentObject = renderObject;
			transparentObject.transparent = true;
			outScene.transparentObjects.push_back(std::move(transparentObject));
		}
	}
}



