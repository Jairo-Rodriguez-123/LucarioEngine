/**
 * @file GUI.cpp
 * @brief Implementa la logica de GUI dentro del subsistema GUI.
 * @ingroup gui
 */
#include "EngineUtilities/GUI/GUI.h"
#include "Viewport.h"
#include "Window.h"
#include "Device.h"
#include "DeviceContext.h"
#include "Texture.h"
#include "MeshComponent.h"
#include "ECS/Actor.h"
#include "ECS/LightComponent.h"
#include "ECS/MeshRendererComponent.h"
#include "Rendering/Mesh.h"
#include "Rendering/Material.h"
#include "Rendering/MaterialInstance.h"
#include "EngineUtilities/Utilities/Camera.h"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cmath>
static ImGuizmo::OPERATION mCurrentGizmoOperation(ImGuizmo::TRANSLATE);
static ImGuizmo::MODE mCurrentGizmoMode(ImGuizmo::LOCAL);

namespace {
const char* GetLightTypeLabel(LightType type);

struct StudioRect {
    ImVec2 pos;
    ImVec2 size;
};

struct StudioLayout {
    StudioRect leftTop;
    StudioRect leftBottom;
    StudioRect viewport;
    StudioRect rightTop;
    StudioRect rightBottom;
};

constexpr float kStudioMenuHeight = 22.0f;
constexpr float kStudioToolbarHeight = 38.0f;
constexpr float kStudioTopOffset = kStudioMenuHeight + kStudioToolbarHeight;
constexpr float kStudioStatusHeight = 20.0f;
constexpr float kStudioGap = 2.0f;

StudioLayout GetStudioLayout(bool showLeftRail = true, bool showRightRail = true) {
    ImGuiViewport* vp = ImGui::GetMainViewport();
    const float workTop = vp->Pos.y + kStudioTopOffset;
    const float workHeight = (std::max)(120.0f, vp->Size.y - kStudioTopOffset - kStudioStatusHeight);

    // V14.2: el Inspector gana aire horizontal y el rail de render queda mas fino.
    // Ambos laterales pueden ocultarse temporalmente para maximizar Scene View.
    const float leftWidth = showLeftRail
        ? (std::min)(238.0f, (std::max)(210.0f, vp->Size.x * 0.135f))
        : 0.0f;
    const float rightWidth = showRightRail
        ? (std::min)(390.0f, (std::max)(342.0f, vp->Size.x * 0.215f))
        : 0.0f;
    const float leftGap = showLeftRail ? kStudioGap : 0.0f;
    const float rightGap = showRightRail ? kStudioGap : 0.0f;
    const float centerWidth = (std::max)(240.0f, vp->Size.x - leftWidth - rightWidth - leftGap - rightGap);
    const float leftTopHeight = workHeight * 0.48f;
    const float rightTopHeight = workHeight * 0.30f;

    StudioLayout layout{};
    layout.leftTop = { ImVec2(vp->Pos.x, workTop), ImVec2(leftWidth, leftTopHeight - kStudioGap * 0.5f) };
    layout.leftBottom = { ImVec2(vp->Pos.x, workTop + leftTopHeight + kStudioGap * 0.5f), ImVec2(leftWidth, workHeight - leftTopHeight - kStudioGap * 0.5f) };
    layout.viewport = { ImVec2(vp->Pos.x + leftWidth + leftGap, workTop), ImVec2(centerWidth, workHeight) };
    layout.rightTop = { ImVec2(vp->Pos.x + leftWidth + leftGap + centerWidth + rightGap, workTop), ImVec2(rightWidth, rightTopHeight - kStudioGap * 0.5f) };
    layout.rightBottom = { ImVec2(vp->Pos.x + leftWidth + leftGap + centerWidth + rightGap, workTop + rightTopHeight + kStudioGap * 0.5f), ImVec2(rightWidth, workHeight - rightTopHeight - kStudioGap * 0.5f) };
    return layout;
}

void ApplyLockedPanelRect(const StudioRect& rect, bool lockLayout) {
    if (!lockLayout) return;
#if defined(IMGUI_HAS_DOCK)
    ImGui::SetNextWindowDockID(0, ImGuiCond_Always);
#endif
    ImGui::SetNextWindowPos(rect.pos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(rect.size, ImGuiCond_Always);
}

ImGuiWindowFlags StudioPanelFlags(bool lockLayout) {
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse;
    if (lockLayout) flags |= ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize;
    return flags;
}

ImU32 AccentU32(const ImVec4& color) {
	return ImGui::ColorConvertFloat4ToU32(color);
}

float RadToDeg(float radians) {
	return XMConvertToDegrees(radians);
}

float DegToRad(float degrees) {
	return XMConvertToRadians(degrees);
}

float ClosestEquivalentDegrees(float candidate, float reference) {
	if (!std::isfinite(candidate) || !std::isfinite(reference)) return candidate;
	while (candidate - reference > 180.0f) candidate -= 360.0f;
	while (candidate - reference < -180.0f) candidate += 360.0f;
	return candidate;
}

float Length3(const EU::Vector3& value) {
	const float lengthSq = value.x * value.x + value.y * value.y + value.z * value.z;
	return (std::isfinite(lengthSq) && lengthSq > 0.0f) ? std::sqrt(lengthSq) : 0.0f;
}

bool GetActorFocusPoint(const EU::TSharedPointer<Actor>& actor, EU::Vector3& outPoint) {
	if (actor.isNull()) return false;
	auto transform = actor->getComponent<Transform>();
	if (transform.isNull()) return false;
	outPoint = transform->getPosition();
	return std::isfinite(outPoint.x) && std::isfinite(outPoint.y) && std::isfinite(outPoint.z);
}

void DollyCamera(Camera& camera, const EU::TSharedPointer<Actor>& actor, float wheelDelta) {
	if (!std::isfinite(wheelDelta) || std::fabs(wheelDelta) <= 1e-6f) return;

	EU::Vector3 target;
	if (!GetActorFocusPoint(actor, target)) {
		// Sin seleccion, la rueda se comporta como un dolly libre hacia delante/atras.
		camera.walk(wheelDelta * 0.85f);
		return;
	}

	const EU::Vector3 position = camera.getPosition();
	const EU::Vector3 toTarget(target.x - position.x, target.y - position.y, target.z - position.z);
	const float distance = Length3(toTarget);
	if (distance <= 1e-5f) return;

	const EU::Vector3 direction(toTarget.x / distance, toTarget.y / distance, toTarget.z / distance);
	// Zoom proporcional a la distancia: rapido cuando estas lejos y preciso cerca del modelo.
	const float zoomStep = (std::max)(0.12f, (std::min)(distance * 0.14f, 12.0f));
	const float newDistance = (std::max)(0.18f, distance - wheelDelta * zoomStep);
	const EU::Vector3 newPosition(
		target.x - direction.x * newDistance,
		target.y - direction.y * newDistance,
		target.z - direction.z * newDistance);
	camera.lookAt(newPosition, target, EU::Vector3(0.0f, 1.0f, 0.0f));
}

void FocusCamera(Camera& camera, const EU::TSharedPointer<Actor>& actor) {
	EU::Vector3 target;
	if (!GetActorFocusPoint(actor, target)) return;

	const EU::Vector3 position = camera.getPosition();
	EU::Vector3 toTarget(target.x - position.x, target.y - position.y, target.z - position.z);
	float distance = Length3(toTarget);
	EU::Vector3 direction;
	if (distance > 1e-5f) {
		direction = EU::Vector3(toTarget.x / distance, toTarget.y / distance, toTarget.z / distance);
	}
	else {
		direction = camera.GetForward();
		if (Length3(direction) <= 1e-5f) direction = EU::Vector3(0.0f, 0.0f, 1.0f);
	}

	// Los OBJ importados por Wildvine se normalizan aproximadamente a 2.5 unidades.
	// Esta distancia los encuadra de forma util sin depender de sus unidades originales.
	const float focusDistance = 3.60f;
	const EU::Vector3 newPosition(
		target.x - direction.x * focusDistance,
		target.y - direction.y * focusDistance,
		target.z - direction.z * focusDistance);
	camera.lookAt(newPosition, target, EU::Vector3(0.0f, 1.0f, 0.0f));
}


void TranslateCamera(Camera& camera, const EU::Vector3& delta) {
	const EU::Vector3 position = camera.getPosition();
	camera.setPosition(EU::Vector3(
		position.x + delta.x,
		position.y + delta.y,
		position.z + delta.z));
}

void RaiseCamera(Camera& camera, float amount) {
	if (!std::isfinite(amount) || std::fabs(amount) <= 1e-7f) return;
	const EU::Vector3 position = camera.getPosition();
	camera.setPosition(EU::Vector3(position.x, position.y + amount, position.z));
}

void PanCamera(Camera& camera, const EU::TSharedPointer<Actor>& actor, float deltaX, float deltaY) {
	if (!std::isfinite(deltaX) || !std::isfinite(deltaY)) return;

	float referenceDistance = 6.0f;
	EU::Vector3 target;
	if (GetActorFocusPoint(actor, target)) {
		const EU::Vector3 position = camera.getPosition();
		referenceDistance = Length3(EU::Vector3(
			target.x - position.x,
			target.y - position.y,
			target.z - position.z));
	}
	const float worldPerPixel = (std::max)(0.0015f, (std::min)(referenceDistance * 0.0018f, 0.06f));
	const EU::Vector3 right = camera.GetRight();
	const EU::Vector3 up = camera.GetUp();
	TranslateCamera(camera, EU::Vector3(
		right.x * (-deltaX * worldPerPixel) + up.x * (deltaY * worldPerPixel),
		right.y * (-deltaX * worldPerPixel) + up.y * (deltaY * worldPerPixel),
		right.z * (-deltaX * worldPerPixel) + up.z * (deltaY * worldPerPixel)));
}

void OrbitCamera(Camera& camera, const EU::TSharedPointer<Actor>& actor, float deltaX, float deltaY) {
	EU::Vector3 target;
	if (!GetActorFocusPoint(actor, target)) return;
	if (!std::isfinite(deltaX) || !std::isfinite(deltaY)) return;

	const EU::Vector3 position = camera.getPosition();
	XMVECTOR offset = XMVectorSet(
		position.x - target.x,
		position.y - target.y,
		position.z - target.z,
		0.0f);
	const float distance = XMVectorGetX(XMVector3Length(offset));
	if (!std::isfinite(distance) || distance <= 0.05f) return;

	constexpr float orbitSensitivity = 0.0045f;
	const float yawAmount = -deltaX * orbitSensitivity;
	const float pitchAmount = -deltaY * orbitSensitivity;

	// Yaw around global up keeps the editor horizon stable.
	offset = XMVector3TransformNormal(offset, XMMatrixRotationY(yawAmount));

	XMVECTOR directionToTarget = XMVector3Normalize(XMVectorNegate(offset));
	XMVECTOR worldUp = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	XMVECTOR right = XMVector3Cross(worldUp, directionToTarget);
	if (XMVectorGetX(XMVector3LengthSq(right)) > 1e-8f) {
		right = XMVector3Normalize(right);
		XMVECTOR pitched = XMVector3TransformNormal(offset, XMMatrixRotationAxis(right, pitchAmount));
		XMVECTOR pitchedDir = XMVector3Normalize(pitched);
		const float upDot = std::fabs(XMVectorGetX(XMVector3Dot(pitchedDir, worldUp)));
		// Avoid crossing the poles, which would flip the editor camera.
		if (upDot < 0.985f) offset = pitched;
	}

	XMFLOAT3 result{};
	XMStoreFloat3(&result, offset);
	const EU::Vector3 newPosition(
		target.x + result.x,
		target.y + result.y,
		target.z + result.z);
	camera.lookAt(newPosition, target, EU::Vector3(0.0f, 1.0f, 0.0f));
}

bool IsEditorKeyDown(ImGuiKey key) {
#if defined(IMGUI_VERSION_NUM) && IMGUI_VERSION_NUM >= 18700
	return ImGui::IsKeyDown(key);
#else
	return ImGui::IsKeyDown(static_cast<int>(key));
#endif
}

void DrawInspectorPill(const char* text, const ImVec4& color) {
	ImGui::PushStyleColor(ImGuiCol_Button, color);
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, color);
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, color);
	ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 2.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6.0f, 1.0f));
	ImGui::Button(text);
	ImGui::PopStyleVar(2);
	ImGui::PopStyleColor(3);
}

bool BeginInspectorSection(const char* label, bool defaultOpen = true) {
	ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanAvailWidth;
	if (defaultOpen) {
		flags |= ImGuiTreeNodeFlags_DefaultOpen;
	}

	ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.095f, 0.105f, 0.122f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.125f, 0.142f, 0.168f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.145f, 0.170f, 0.205f, 1.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6.0f, 2.0f));
	const bool open = ImGui::CollapsingHeader(label, flags);
	ImGui::PopStyleVar();
	ImGui::PopStyleColor(3);
	return open;
}

bool BeginInspectorPropertyTable(const char* id, float firstColumnWidth = 118.0f) {
	if (!ImGui::BeginTable(id, 2, ImGuiTableFlags_SizingStretchProp)) {
		return false;
	}

	ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, firstColumnWidth);
	ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
	return true;
}

void DrawPropertyLabel(const char* label) {
	ImGui::TableNextRow();
	ImGui::TableSetColumnIndex(0);
	ImGui::AlignTextToFramePadding();
	ImGui::TextDisabled("%s", label);
	ImGui::TableSetColumnIndex(1);
	ImGui::SetNextItemWidth(-FLT_MIN);
}

void DrawPropertyValueText(const char* label, const char* value) {
	ImGui::TableNextRow();
	ImGui::TableSetColumnIndex(0);
	ImGui::AlignTextToFramePadding();
	ImGui::TextDisabled("%s", label);
	ImGui::TableSetColumnIndex(1);
	ImGui::TextUnformatted(value);
}

void DrawPropertyValueBool(const char* label, bool value) {
	DrawPropertyValueText(label, value ? "Yes" : "No");
}

bool DrawPropertyToggle(const char* label, const char* id, bool* value) {
	ImGui::TableNextRow();
	ImGui::TableSetColumnIndex(0);
	ImGui::AlignTextToFramePadding();
	ImGui::TextDisabled("%s", label);
	ImGui::TableSetColumnIndex(1);
	ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
	const bool changed = ImGui::Checkbox(id, value);
	ImGui::PopStyleVar();
	return changed;
}

const char* GetActorTypeLabel(EU::TSharedPointer<Actor> actor) {
	if (actor.isNull()) {
		return "Actor";
	}

	auto lightComponent = actor->getComponent<LightComponent>();
	if (!lightComponent.isNull()) {
		return GetLightTypeLabel(lightComponent->getLightData().type);
	}

	if (!actor->getComponent<MeshRendererComponent>().isNull()) {
		return "Static Mesh Actor";
	}

	if (!actor->getComponent<Transform>().isNull()) {
		return "Empty Actor";
	}

	return "Actor";
}

ImVec4 GetActorTypeColor(EU::TSharedPointer<Actor> actor) {
	if (actor.isNull()) {
		return ImVec4(0.45f, 0.47f, 0.52f, 1.0f);
	}

	auto lightComponent = actor->getComponent<LightComponent>();
	if (!lightComponent.isNull()) {
		return ImVec4(0.92f, 0.68f, 0.22f, 1.0f);
	}

	if (!actor->getComponent<MeshRendererComponent>().isNull()) {
		return ImVec4(0.24f, 0.50f, 0.92f, 1.0f);
	}

	return ImVec4(0.36f, 0.72f, 0.46f, 1.0f);
}

void DrawInspectorComponentChips(bool hasTransform, bool hasMeshRenderer, bool hasLight) {
	if (hasTransform) {
		DrawInspectorPill("Transform", ImVec4(0.18f, 0.50f, 0.28f, 1.0f));
	}
	if (hasMeshRenderer) {
		if (hasTransform) {
			ImGui::SameLine();
		}
		DrawInspectorPill("Renderer", ImVec4(0.22f, 0.42f, 0.76f, 1.0f));
	}
	if (hasLight) {
		if (hasTransform || hasMeshRenderer) {
			ImGui::SameLine();
		}
		DrawInspectorPill("Light", ImVec4(0.62f, 0.46f, 0.14f, 1.0f));
	}
}

const char* GetLightTypeLabel(LightType type) {
	switch (type) {
	case LightType::Directional: return "Directional";
	case LightType::Point: return "Point";
	case LightType::Spot: return "Spot";
	default: return "Unknown";
	}
}

const char* GetMaterialDomainLabel(MaterialDomain domain) {
	switch (domain) {
	case MaterialDomain::Opaque: return "Opaque";
	case MaterialDomain::Masked: return "Masked";
	case MaterialDomain::Transparent: return "Transparent";
	default: return "Unknown";
	}
}

const char* GetBlendModeLabel(BlendMode blendMode) {
	switch (blendMode) {
	case BlendMode::Opaque: return "Opaque";
	case BlendMode::Alpha: return "Alpha";
	case BlendMode::Additive: return "Additive";
	case BlendMode::PremultipliedAlpha: return "Premultiplied";
	default: return "Unknown";
	}
}

const char* GetMaterialTextureChannelLabel(MaterialTextureChannel channel) {
	switch (channel) {
	case MaterialTextureChannel::Albedo: return "Albedo";
	case MaterialTextureChannel::Normal: return "Normal";
	case MaterialTextureChannel::Metallic: return "Metallic";
	case MaterialTextureChannel::Roughness: return "Roughness";
	case MaterialTextureChannel::AO: return "Ambient Occlusion";
	case MaterialTextureChannel::Emissive: return "Emissive";
	default: return "Texture";
	}
}

Texture* GetMaterialTexture(MaterialInstance* materialInstance, MaterialTextureChannel channel) {
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

std::string GetTextureDisplayName(const Texture* texture) {
	if (!texture || texture->m_textureName.empty()) return "Built-in fallback";
	const std::string& path = texture->m_textureName;
	const size_t slash = path.find_last_of("/\\");
	return slash == std::string::npos ? path : path.substr(slash + 1);
}

const char* GetAssetBrowserTypeLabel(AssetBrowserItemType type) {
	switch (type) {
	case AssetBrowserItemType::ModelOBJ: return "OBJ";
	case AssetBrowserItemType::Texture: return "Texture";
	case AssetBrowserItemType::MaterialMTL: return "MTL";
	default: return "Asset";
	}
}

bool AssetBrowserTextMatches(const std::string& text, const char* search) {
	if (!search || !search[0]) return true;
	std::string haystack = text;
	std::string needle = search;
	std::transform(haystack.begin(), haystack.end(), haystack.begin(),
		[](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	std::transform(needle.begin(), needle.end(), needle.begin(),
		[](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	return haystack.find(needle) != std::string::npos;
}

bool AssetMatchesCategory(const AssetBrowserItem& asset, int category) {
	switch (category) {
	case 1: return asset.type == AssetBrowserItemType::ModelOBJ;
	case 2: return asset.type == AssetBrowserItemType::Texture;
	case 3: return asset.type == AssetBrowserItemType::MaterialMTL;
	default: return true;
	}
}
}
void
GUI::awake() {
}

bool
GUI::init(Window& window, Device& device, DeviceContext& deviceContext) {
	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;       // Enable Keyboard Controls
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;           // Enable Docking
	// Setup Dear ImGui style
	ImGui::StyleColorsDark();

	// When viewports are enabled we tweak WindowRounding/WindowBg so platform windows can look identical to regular ones.
	ImGuiStyle& style = ImGui::GetStyle();
	if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
	{
		style.WindowRounding = 0.0f;
		style.Colors[ImGuiCol_WindowBg].w = 1.0f;
	}

	appleLiquidStyle(0.72f, ImVec4(0.0f, 0.515f, 1.0f, 1.0f));

	// Setup Platform/Renderer backends. Ambos devuelven false si el backend
	// no puede asociarse a la ventana/dispositivo actuales.
	if (!window.m_hWnd || !device.m_device || !deviceContext.m_deviceContext) {
		ImGui::DestroyContext();
		return false;
	}

	if (!ImGui_ImplWin32_Init(window.m_hWnd)) {
		ImGui::DestroyContext();
		return false;
	}

	if (!ImGui_ImplDX11_Init(device.m_device, deviceContext.m_deviceContext)) {
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();
		return false;
	}

	// Inicializar datos de UI solo despues de que ambos backends esten listos.
	toolTipData();
	selectedActorIndex = 0;
	return true;
}

void
GUI::update(Viewport& viewport, Window& window) {
	// Start the Dear ImGui frame
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	ImGuizmo::BeginFrame();
	ImGuiIO& io = ImGui::GetIO();
#if defined(IMGUI_VERSION_NUM) && IMGUI_VERSION_NUM >= 18700
	const bool savePressed = ImGui::IsKeyPressed(ImGuiKey_S, false);
	const bool openPressed = ImGui::IsKeyPressed(ImGuiKey_O, false);
	const bool newPressed = ImGui::IsKeyPressed(ImGuiKey_N, false);
	const bool duplicatePressed = ImGui::IsKeyPressed(ImGuiKey_D, false);
	const bool undoPressed = ImGui::IsKeyPressed(ImGuiKey_Z, false);
	const bool redoPressed = ImGui::IsKeyPressed(ImGuiKey_Y, false);
	const bool deletePressed = ImGui::IsKeyPressed(ImGuiKey_Delete, false);
	const bool renamePressed = ImGui::IsKeyPressed(ImGuiKey_F2, false);
#else
	const bool savePressed = ImGui::IsKeyPressed('S', false);
	const bool openPressed = ImGui::IsKeyPressed('O', false);
	const bool newPressed = ImGui::IsKeyPressed('N', false);
	const bool duplicatePressed = ImGui::IsKeyPressed('D', false);
	const bool undoPressed = ImGui::IsKeyPressed('Z', false);
	const bool redoPressed = ImGui::IsKeyPressed('Y', false);
	const bool deletePressed = ImGui::IsKeyPressed(VK_DELETE, false);
	const bool renamePressed = ImGui::IsKeyPressed(VK_F2, false);
#endif

	// Atajos de escena. Las acciones destructivas se difieren al siguiente frame,
	// cuando los paneles de ImGui ya no conservan punteros al actor seleccionado.
	if (!io.WantTextInput) {
		if (io.KeyCtrl && undoPressed && !io.KeyShift) {
			m_sceneEditorRequest = { true, SceneEditorAction::Undo, -1, std::string() };
		}
		else if (io.KeyCtrl && (redoPressed || (io.KeyShift && undoPressed))) {
			m_sceneEditorRequest = { true, SceneEditorAction::Redo, -1, std::string() };
		}
		else if (io.KeyCtrl && savePressed) {
			if (io.KeyShift) {
				m_sceneEditorRequest = { true, SceneEditorAction::SaveSceneAs, -1, std::string() };
			}
			else {
				m_requestSaveScene = true;
			}
		}
		else if (io.KeyCtrl && openPressed) {
			m_sceneEditorRequest = { true, SceneEditorAction::OpenScene, -1, std::string() };
		}
		else if (io.KeyCtrl && newPressed) {
			m_sceneEditorRequest = { true, SceneEditorAction::NewScene, -1, std::string() };
		}
		else if (io.KeyCtrl && duplicatePressed && selectedActorIndex >= 0) {
			m_sceneEditorRequest = { true, SceneEditorAction::DuplicateActor, selectedActorIndex, std::string() };
		}
		else if (deletePressed && selectedActorIndex >= 0) {
			m_sceneEditorRequest = { true, SceneEditorAction::DeleteActor, selectedActorIndex, std::string() };
		}
		else if (renamePressed && selectedActorIndex >= 0) {
			m_renameActorIndex = selectedActorIndex;
			m_renameActorBuffer[0] = '\0';
			m_openRenameActorPopup = true;
		}
	}
	ImGuizmo::SetOrthographic(false);
	//ImGuizmo::SetRect(0, 0, (float)window.m_width, (float)window.m_height);

	// In Program always
	drawStudioTopRibbon();
	drawEditorDockspace();
	drawEditorStatusBar();
	closeApp();
	drawGizmoToolbar();
}

void
GUI::render() {
	ImGui::Render();
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
	ImGuiIO& io = ImGui::GetIO();
	// Update and Render additional Platform Windows
	if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
	{
		ImGui::UpdatePlatformWindows();
		ImGui::RenderPlatformWindowsDefault();
	}
}

void
GUI::destroy() {
	// Cleanup
	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
}

void 
GUI::vec3Control(const std::string& label, float* values, float resetValue, float columnWidth, bool displayAsDegrees) {
	ImFont* boldFont = ImGui::GetFont();
	float displayValues[3] = { values[0], values[1], values[2] };
	if (displayAsDegrees) {
		displayValues[0] = RadToDeg(values[0]);
		displayValues[1] = RadToDeg(values[1]);
		displayValues[2] = RadToDeg(values[2]);
	}

	ImGui::PushID(label.c_str());
	if (!ImGui::BeginTable(("##Vec3Table" + label).c_str(), 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_BordersInnerV)) {
		ImGui::PopID();
		return;
	}

	ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, columnWidth);
	ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
	ImGui::TableNextRow();
	ImGui::TableSetColumnIndex(0);
	ImGui::AlignTextToFramePadding();
	ImGui::TextDisabled("%s", label.c_str());
	ImGui::TableSetColumnIndex(1);
	ImGui::PushItemWidth(-1.0f);

	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{ 3.0f, 4.0f });
	ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
	float lineHeight = ImGui::GetFontSize() + ImGui::GetStyle().FramePadding.y * 2.0f;
	ImVec2 buttonSize = { lineHeight, lineHeight };
	const float spacing = ImGui::GetStyle().ItemSpacing.x;
	const float availableWidth = ImGui::GetContentRegionAvail().x;
	const float dragWidth = (availableWidth - (buttonSize.x * 3.0f) - (spacing * 5.0f)) / 3.0f;
	const float safeDragWidth = dragWidth > 24.0f ? dragWidth : 24.0f;
	const char* dragFormat = displayAsDegrees ? "%.2f" : "%.3f";

	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.8f, 0.1f, 0.15f, 1.0f });
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.9f, 0.2f, 0.2f, 1.0f });
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.8f, 0.1f, 0.15f, 1.0f });
	ImGui::PushFont(boldFont);
	if (ImGui::Button("X", buttonSize)) {
		values[0] = resetValue;
		displayValues[0] = displayAsDegrees ? RadToDeg(resetValue) : resetValue;
			requestHistoryCommit("Edit Transform");
	}
	ImGui::PopFont();
	ImGui::PopStyleColor(3);

	ImGui::SameLine();
	ImGui::SetNextItemWidth(safeDragWidth);
	if (ImGui::InputFloat("##X", &displayValues[0], 0.0f, 0.0f, dragFormat, ImGuiInputTextFlags_AutoSelectAll)) {
		if (std::isfinite(displayValues[0])) {
			values[0] = displayAsDegrees ? DegToRad(displayValues[0]) : displayValues[0];
		}
	}
	if (ImGui::IsItemDeactivatedAfterEdit()) requestHistoryCommit("Edit Transform");
	if (ImGui::IsItemHovered()) ImGui::SetTooltip("Click and type a numeric value");
	ImGui::SameLine();

	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.2f, 0.7f, 0.2f, 1.0f });
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.3f, 0.8f, 0.3f, 1.0f });
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.2f, 0.7f, 0.2f, 1.0f });
	ImGui::PushFont(boldFont);
	if (ImGui::Button("Y", buttonSize)) {
		values[1] = resetValue;
		displayValues[1] = displayAsDegrees ? RadToDeg(resetValue) : resetValue;
			requestHistoryCommit("Edit Transform");
	}
	ImGui::PopFont();
	ImGui::PopStyleColor(3);

	ImGui::SameLine();
	ImGui::SetNextItemWidth(safeDragWidth);
	if (ImGui::InputFloat("##Y", &displayValues[1], 0.0f, 0.0f, dragFormat, ImGuiInputTextFlags_AutoSelectAll)) {
		if (std::isfinite(displayValues[1])) {
			values[1] = displayAsDegrees ? DegToRad(displayValues[1]) : displayValues[1];
		}
	}
	if (ImGui::IsItemDeactivatedAfterEdit()) requestHistoryCommit("Edit Transform");
	if (ImGui::IsItemHovered()) ImGui::SetTooltip("Click and type a numeric value");
	ImGui::SameLine();

	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.1f, 0.25f, 0.8f, 1.0f });
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.2f, 0.35f, 0.9f, 1.0f });
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.1f, 0.25f, 0.8f, 1.0f });
	ImGui::PushFont(boldFont);
	if (ImGui::Button("Z", buttonSize)) {
		values[2] = resetValue;
		displayValues[2] = displayAsDegrees ? RadToDeg(resetValue) : resetValue;
			requestHistoryCommit("Edit Transform");
	}
	ImGui::PopFont();
	ImGui::PopStyleColor(3);

	ImGui::SameLine();
	ImGui::SetNextItemWidth(safeDragWidth);
	if (ImGui::InputFloat("##Z", &displayValues[2], 0.0f, 0.0f, dragFormat, ImGuiInputTextFlags_AutoSelectAll)) {
		if (std::isfinite(displayValues[2])) {
			values[2] = displayAsDegrees ? DegToRad(displayValues[2]) : displayValues[2];
		}
	}
	if (ImGui::IsItemDeactivatedAfterEdit()) requestHistoryCommit("Edit Transform");
	if (ImGui::IsItemHovered()) ImGui::SetTooltip("Click and type a numeric value");

	ImGui::PopStyleVar(2);
	ImGui::PopItemWidth();
	ImGui::EndTable();

	ImGui::PopID();
}

void 
GUI::toolTipData() {
}

void
GUI::appleLiquidStyle(float opacity, ImVec4 accent) {
    (void)opacity;
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;

    // V141: tema de editor profesional. Menos redondeo, mayor densidad y
    // superficies opacas para que el viewport sea el protagonista.
    style.WindowRounding = 2.0f;
    style.ChildRounding = 2.0f;
    style.PopupRounding = 3.0f;
    style.FrameRounding = 2.0f;
    style.GrabRounding = 2.0f;
    style.ScrollbarRounding = 3.0f;
    style.TabRounding = 2.0f;

    style.WindowBorderSize = 1.0f;
    style.ChildBorderSize = 0.0f;
    style.FrameBorderSize = 0.0f;
    style.PopupBorderSize = 1.0f;
    style.TabBorderSize = 0.0f;

    style.WindowPadding = ImVec2(6.0f, 5.0f);
    style.FramePadding = ImVec2(6.0f, 2.0f);
    style.CellPadding = ImVec2(4.0f, 1.0f);
    style.ItemSpacing = ImVec2(5.0f, 3.0f);
    style.ItemInnerSpacing = ImVec2(4.0f, 3.0f);
    style.IndentSpacing = 14.0f;
    style.ScrollbarSize = 9.0f;
    style.GrabMinSize = 7.0f;

    const ImVec4 bg0(0.026f, 0.029f, 0.035f, 1.0f);
    const ImVec4 bg1(0.038f, 0.042f, 0.050f, 1.0f);
    const ImVec4 bg2(0.055f, 0.060f, 0.071f, 1.0f);
    const ImVec4 bg3(0.078f, 0.085f, 0.101f, 1.0f);
    const ImVec4 border(0.105f, 0.115f, 0.135f, 1.0f);
    const ImVec4 text(0.86f, 0.88f, 0.91f, 1.0f);
    const ImVec4 muted(0.46f, 0.50f, 0.57f, 1.0f);
    const ImVec4 accentSoft(accent.x, accent.y, accent.z, 0.72f);
    const ImVec4 accentHover((std::min)(1.0f, accent.x + 0.08f),
                             (std::min)(1.0f, accent.y + 0.08f),
                             (std::min)(1.0f, accent.z + 0.08f), 0.86f);

    colors[ImGuiCol_Text] = text;
    colors[ImGuiCol_TextDisabled] = muted;
    colors[ImGuiCol_WindowBg] = bg1;
    colors[ImGuiCol_ChildBg] = bg1;
    colors[ImGuiCol_PopupBg] = bg2;
    colors[ImGuiCol_Border] = border;
    colors[ImGuiCol_BorderShadow] = ImVec4(0, 0, 0, 0);

    colors[ImGuiCol_FrameBg] = bg2;
    colors[ImGuiCol_FrameBgHovered] = bg3;
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.12f, 0.13f, 0.16f, 1.0f);

    colors[ImGuiCol_TitleBg] = bg0;
    colors[ImGuiCol_TitleBgActive] = bg0;
    colors[ImGuiCol_TitleBgCollapsed] = bg0;
    colors[ImGuiCol_MenuBarBg] = bg0;

    colors[ImGuiCol_ScrollbarBg] = bg0;
    colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.20f, 0.22f, 0.26f, 1.0f);
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.27f, 0.29f, 0.34f, 1.0f);
    colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.34f, 0.36f, 0.42f, 1.0f);

    colors[ImGuiCol_CheckMark] = accent;
    colors[ImGuiCol_SliderGrab] = accentSoft;
    colors[ImGuiCol_SliderGrabActive] = accent;
    colors[ImGuiCol_Button] = bg2;
    colors[ImGuiCol_ButtonHovered] = bg3;
    colors[ImGuiCol_ButtonActive] = accentSoft;
    colors[ImGuiCol_Header] = ImVec4(accent.x, accent.y, accent.z, 0.28f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(accent.x, accent.y, accent.z, 0.42f);
    colors[ImGuiCol_HeaderActive] = accentSoft;
    colors[ImGuiCol_Separator] = border;
    colors[ImGuiCol_SeparatorHovered] = accentSoft;
    colors[ImGuiCol_SeparatorActive] = accent;
    colors[ImGuiCol_ResizeGrip] = ImVec4(accent.x, accent.y, accent.z, 0.12f);
    colors[ImGuiCol_ResizeGripHovered] = ImVec4(accent.x, accent.y, accent.z, 0.45f);
    colors[ImGuiCol_ResizeGripActive] = accent;
    colors[ImGuiCol_Tab] = bg1;
    colors[ImGuiCol_TabHovered] = bg3;
    colors[ImGuiCol_TabActive] = ImVec4(accent.x, accent.y, accent.z, 0.34f);
    colors[ImGuiCol_TabUnfocused] = bg1;
    colors[ImGuiCol_TabUnfocusedActive] = bg2;
    colors[ImGuiCol_DockingPreview] = ImVec4(accent.x, accent.y, accent.z, 0.35f);
    colors[ImGuiCol_DockingEmptyBg] = bg0;
    colors[ImGuiCol_TableHeaderBg] = bg2;
    colors[ImGuiCol_TableBorderStrong] = border;
    colors[ImGuiCol_TableBorderLight] = ImVec4(border.x, border.y, border.z, 0.55f);
    colors[ImGuiCol_TableRowBg] = ImVec4(0, 0, 0, 0);
    colors[ImGuiCol_TableRowBgAlt] = ImVec4(1, 1, 1, 0.018f);
    colors[ImGuiCol_TextSelectedBg] = ImVec4(accent.x, accent.y, accent.z, 0.35f);
    colors[ImGuiCol_NavHighlight] = accentHover;
    colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0, 0, 0, 0.62f);
}


void
GUI::ToolBar() {
	if (ImGui::BeginMainMenuBar()) {
		if (ImGui::BeginMenu("File")) {
			if (ImGui::MenuItem("New")) {
				// Acci�n para "New"
			}
			if (ImGui::MenuItem("Open")) {
				// Acci�n para "Open"
			}
			if (ImGui::MenuItem("Save")) {
				// Acci�n para "Save"
			}
			if (ImGui::MenuItem("Exit")) {
				// Acci�n para "Exit"
				show_exit_popup = true;
				ImGui::OpenPopup("Exit?");
				//closeApp();
			}
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Edit")) {
			if (ImGui::MenuItem("Undo")) {
				// Acci�n para "Undo"
			}
			if (ImGui::MenuItem("Redo")) {
				// Acci�n para "Redo"
			}
			if (ImGui::MenuItem("Cut")) {
				// Acci�n para "Cut"
			}
			if (ImGui::MenuItem("Copy")) {
				// Acci�n para "Copy"
			}
			if (ImGui::MenuItem("Paste")) {
				// Acci�n para "Paste"
			}
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Tools")) {
			if (ImGui::MenuItem("Options")) {
				// Acci�n para "Options"
			}
			if (ImGui::MenuItem("Settings")) {
				// Acci�n para "Settings"
			}
			ImGui::EndMenu();
		}
		ImGui::EndMainMenuBar();
	}
}

void
GUI::closeApp() {
	if (show_exit_popup) {
		ImGui::OpenPopup("Exit?");
		show_exit_popup = false; // Reset the flag
	}
	// Centrar el popup en la pantalla
	ImVec2 center = ImGui::GetMainViewport()->GetCenter();
	ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

	if (ImGui::BeginPopupModal("Exit?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
		ImGui::Text("Estas a punto de salir de la aplicacion.\nEstas seguro?\n\n");
		ImGui::Separator();

		if (ImGui::Button("OK", ImVec2(120, 0))) {
			// Solicitar el cierre de la ventana permite que BaseApp::destroy()
			// libere ImGui y los recursos D3D11 en el orden normal.
			ImGui::CloseCurrentPopup();
			PostQuitMessage(0);
		}
		ImGui::SetItemDefaultFocus();
		ImGui::SameLine();
		if (ImGui::Button("Cancel", ImVec2(120, 0))) {
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}
}

void
GUI::inspectorGeneral(EU::TSharedPointer<Actor> actor) {
	if (!m_showRightRail || !m_showInspectorPanel) return;
	const StudioLayout layout = GetStudioLayout(m_showLeftRail, m_showRightRail);
	ApplyLockedPanelRect(layout.rightBottom, m_lockEditorLayout);
	ImGuiWindowFlags panelFlags = StudioPanelFlags(m_lockEditorLayout);
	if (!ImGui::Begin("Properties", &m_showInspectorPanel, panelFlags)) {
		ImGui::End();
		return;
	}
	if (actor.isNull()) {
		ImGui::Dummy(ImVec2(0.0f, 12.0f));
		ImGui::TextDisabled("No actor selected");
		ImGui::TextWrapped("Select an actor in the Hierarchy to inspect transforms, materials, lights and renderer data.");
		ImGui::End();
		return;
	}

	static char objectName[128] = {};
	static Actor* cachedActor = nullptr;
	if (cachedActor != actor.get() || std::string(objectName) != actor->getName()) {
		cachedActor = actor.get();
		strncpy_s(objectName, actor->getName().c_str(), _TRUNCATE);
	}

	auto meshRenderer = actor->getComponent<MeshRendererComponent>();
	auto lightComponent = actor->getComponent<LightComponent>();
	auto transform = actor->getComponent<Transform>();
	const bool hasMeshRenderer = !meshRenderer.isNull();
	const bool hasLightComponent = !lightComponent.isNull();
	const bool hasTransform = !transform.isNull();
	const ImVec4 accentColor = GetActorTypeColor(actor);
	const char* actorTypeLabel = GetActorTypeLabel(actor);

	ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 2.0f);
	ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.040f, 0.044f, 0.052f, 1.0f));
	ImGui::BeginChild("##InspectorHeader", ImVec2(0.0f, 52.0f), false);
	ImDrawList* drawList = ImGui::GetWindowDrawList();
	ImVec2 headerMin = ImGui::GetWindowPos();
	ImVec2 headerMax = ImVec2(headerMin.x + ImGui::GetWindowSize().x, headerMin.y + ImGui::GetWindowSize().y);
	drawList->AddRectFilled(headerMin, ImVec2(headerMin.x + 2.0f, headerMax.y), AccentU32(accentColor));

	ImGui::TextDisabled("%s", actorTypeLabel);
	ImGui::SameLine();
	DrawInspectorComponentChips(hasTransform, hasMeshRenderer, hasLightComponent);
	ImGui::SetNextItemWidth(-1.0f);
	if (ImGui::InputText("##ObjectName", objectName, IM_ARRAYSIZE(objectName))) {
		actor->setName(objectName);
	}
	if (ImGui::IsItemDeactivatedAfterEdit()) requestHistoryCommit("Rename Actor");
	ImGui::EndChild();
	ImGui::PopStyleColor();
	ImGui::PopStyleVar();

	if (BeginInspectorSection("Identity", false)) {
		if (BeginInspectorPropertyTable("##IdentityProperties")) {
			DrawPropertyValueText("Name", actor->getName().c_str());
			DrawPropertyValueText("Type", actorTypeLabel);
			DrawPropertyValueBool("Transform", hasTransform);
			DrawPropertyValueBool("Renderer", hasMeshRenderer);
			DrawPropertyValueBool("Light", hasLightComponent);
			ImGui::EndTable();
		}
	}

	if (hasTransform && BeginInspectorSection("Transform")) {
		inspectorContainer(actor);
	}

	if (hasMeshRenderer) {
		const std::vector<MaterialInstance*>& materialInstances = meshRenderer->getMaterialInstances();
		Mesh* mesh = meshRenderer->getMesh();

		if (BeginInspectorSection("Renderer")) {
			const int submeshCount = mesh ? static_cast<int>(mesh->getSubmeshes().size()) : 0;
			const int materialCount = static_cast<int>(materialInstances.size());
			if (BeginInspectorPropertyTable("##RendererProperties")) {
				bool isVisible = meshRenderer->isVisible();
				if (DrawPropertyToggle("Visible", "##RendererVisible", &isVisible))
					requestHistoryCommit("Edit Renderer");
				meshRenderer->setVisible(isVisible);

				bool castShadow = meshRenderer->canCastShadow();
				if (DrawPropertyToggle("Cast Shadow", "##RendererCastShadow", &castShadow))
					requestHistoryCommit("Edit Renderer");
				meshRenderer->setCastShadow(castShadow);

				char countBuffer[32] = {};
				sprintf_s(countBuffer, "%d", submeshCount);
				DrawPropertyValueText("Submeshes", countBuffer);

				sprintf_s(countBuffer, "%d", materialCount);
				DrawPropertyValueText("Material Slots", countBuffer);
				ImGui::EndTable();
			}
		}

		if (!materialInstances.empty() && BeginInspectorSection("Materials")) {
			for (size_t i = 0; i < materialInstances.size(); ++i) {
				MaterialInstance* materialInstance = materialInstances[i];
				if (!materialInstance) {
					continue;
				}

				MaterialParams& params = materialInstance->getParams();
				Material* material = materialInstance->getMaterial();
				std::string header = "Material Slot " + std::to_string(i);
				if (ImGui::TreeNodeEx(header.c_str(), ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth)) {
					if (material) {
						if (BeginInspectorPropertyTable(("##MaterialMeta" + std::to_string(i)).c_str())) {
							DrawPropertyValueText("Domain", GetMaterialDomainLabel(material->getDomain()));
							if (material->getDomain() == MaterialDomain::Transparent) {
								DrawPropertyValueText("Blend", GetBlendModeLabel(material->getBlendMode()));
							}
							ImGui::EndTable();
						}

						static const char* kMaterialDomains[] = { "Opaque", "Masked", "Transparent" };
						int currentDomain = static_cast<int>(material->getDomain());
						if (BeginInspectorPropertyTable(("##MaterialEditor" + std::to_string(i)).c_str())) {
							DrawPropertyLabel("Domain");
							if (ImGui::Combo(("##Domain" + std::to_string(i)).c_str(), &currentDomain, kMaterialDomains, IM_ARRAYSIZE(kMaterialDomains))) {
								material->setDomain(static_cast<MaterialDomain>(currentDomain));
								requestHistoryCommit("Edit Material");
							}

							if (material->getDomain() == MaterialDomain::Transparent) {
								static const char* kBlendModes[] = { "Opaque", "Alpha", "Additive", "Premultiplied" };
								int currentBlendMode = static_cast<int>(material->getBlendMode());
								DrawPropertyLabel("Blend Mode");
								if (ImGui::Combo(("##BlendMode" + std::to_string(i)).c_str(), &currentBlendMode, kBlendModes, IM_ARRAYSIZE(kBlendModes))) {
									material->setBlendMode(static_cast<BlendMode>(currentBlendMode));
									requestHistoryCommit("Edit Material");
								}
							}

							DrawPropertyLabel("Base Color");
							ImGui::ColorEdit4(("##BaseColor" + std::to_string(i)).c_str(), &params.baseColor.x);
							if (ImGui::IsItemDeactivatedAfterEdit()) requestHistoryCommit("Edit Material");
							DrawPropertyLabel("Metallic");
							ImGui::SliderFloat(("##Metallic" + std::to_string(i)).c_str(), &params.metallic, 0.0f, 1.0f);
							if (ImGui::IsItemDeactivatedAfterEdit()) requestHistoryCommit("Edit Material");
							DrawPropertyLabel("Roughness");
							ImGui::SliderFloat(("##Roughness" + std::to_string(i)).c_str(), &params.roughness, 0.0f, 1.0f);
							if (ImGui::IsItemDeactivatedAfterEdit()) requestHistoryCommit("Edit Material");
							DrawPropertyLabel("Ambient Occlusion");
							ImGui::SliderFloat(("##AO" + std::to_string(i)).c_str(), &params.ao, 0.0f, 1.0f);
							if (ImGui::IsItemDeactivatedAfterEdit()) requestHistoryCommit("Edit Material");
							DrawPropertyLabel("Normal Scale");
							ImGui::SliderFloat(("##NormalScale" + std::to_string(i)).c_str(), &params.normalScale, 0.0f, 2.0f);
							if (ImGui::IsItemDeactivatedAfterEdit()) requestHistoryCommit("Edit Material");
							if (materialInstance->getEmissive()) {
								DrawPropertyLabel("Emissive Strength");
								ImGui::SliderFloat(("##EmissiveStrength" + std::to_string(i)).c_str(), &params.emissiveStrength, 0.0f, 8.0f);
							if (ImGui::IsItemDeactivatedAfterEdit()) requestHistoryCommit("Edit Material");
							}
							if (material->getDomain() == MaterialDomain::Masked) {
								DrawPropertyLabel("Alpha Cutoff");
								ImGui::SliderFloat(("##AlphaCutoff" + std::to_string(i)).c_str(), &params.alphaCutoff, 0.0f, 1.0f);
							if (ImGui::IsItemDeactivatedAfterEdit()) requestHistoryCommit("Edit Material");
							}
							ImGui::EndTable();
						}
					}
					ImGui::TreePop();
				}
			}
		}
	}

	if (hasLightComponent && BeginInspectorSection("Light")) {
		LightData& light = lightComponent->getLightData();
		if (BeginInspectorPropertyTable("##LightProperties")) {
			DrawPropertyValueText("Type", GetLightTypeLabel(light.type));
			bool castShadow = lightComponent->canCastShadow();
			if (DrawPropertyToggle("Cast Shadow", "##LightCastShadow", &castShadow))
				requestHistoryCommit("Edit Light");
			lightComponent->setCastShadow(castShadow);
			DrawPropertyLabel("Color");
			ImGui::ColorEdit3("##LightColor", &light.color.x);
			if (ImGui::IsItemDeactivatedAfterEdit()) requestHistoryCommit("Edit Light");
			DrawPropertyLabel("Intensity");
			ImGui::SliderFloat("##LightIntensity", &light.intensity, 0.0f, 10.0f);
			if (ImGui::IsItemDeactivatedAfterEdit()) requestHistoryCommit("Edit Light");
			if (light.type == LightType::Directional || light.type == LightType::Spot) {
				DrawPropertyLabel("Direction");
				ImGui::SliderFloat3("##LightDirection", &light.direction.x, -1.0f, 1.0f);
			if (ImGui::IsItemDeactivatedAfterEdit()) requestHistoryCommit("Edit Light");
			}
			if (light.type == LightType::Point || light.type == LightType::Spot) {
				DrawPropertyLabel("Range");
				ImGui::SliderFloat("##LightRange", &light.range, 0.0f, 100.0f);
			if (ImGui::IsItemDeactivatedAfterEdit()) requestHistoryCommit("Edit Light");
			}
			ImGui::EndTable();
		}
	}
	ImGui::End();
}

void
GUI::inspectorContainer(EU::TSharedPointer<Actor> actor) {
	if (actor.isNull()) return;
	auto transform = actor->getComponent<Transform>();
	if (transform.isNull()) return;

	EU::Vector3 position = transform->getPosition();
	EU::Vector3 rotation = transform->getRotation();
	EU::Vector3 scale = transform->getScale();

	vec3Control("Position", position.data(), 0.0f, 78.0f, false);
	vec3Control("Rotation", rotation.data(), 0.0f, 78.0f, true);
	vec3Control("Scale", scale.data(), 1.0f, 78.0f, false);

	transform->setPosition(position);
	transform->setRotation(rotation);
	transform->setScale(scale);
}

void 
GUI::outliner(const std::vector<EU::TSharedPointer<Actor>>& actors) {
	if (!m_showRightRail || !m_showHierarchyPanel) return;
	const StudioLayout layout = GetStudioLayout(m_showLeftRail, m_showRightRail);
	ApplyLockedPanelRect(layout.rightTop, m_lockEditorLayout);
	ImGuiWindowFlags panelFlags = StudioPanelFlags(m_lockEditorLayout);
	if (!ImGui::Begin("Hierarchy", &m_showHierarchyPanel, panelFlags)) {
		ImGui::End();
		return;
	}

	ImGui::TextDisabled("SCENE   %d ACTORS", static_cast<int>(actors.size()));
	static ImGuiTextFilter filter;
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6.0f, 2.0f));
	filter.Draw("Search actors...", -1.0f);
	ImGui::PopStyleVar();

	if (selectedActorIndex >= static_cast<int>(actors.size())) {
		selectedActorIndex = actors.empty() ? -1 : static_cast<int>(actors.size()) - 1;
	}

	for (int i = 0; i < static_cast<int>(actors.size()); ++i) {
		const auto& actor = actors[i];
		std::string actorName = actor ? actor->getName() : "Actor";
		const char* actorTypeLabel = GetActorTypeLabel(actor);
		ImVec4 actorTypeColor = GetActorTypeColor(actor);
		std::string filterLabel = actorName + " " + actorTypeLabel;
		if (!filter.PassFilter(filterLabel.c_str())) {
			continue;
		}

		auto meshRenderer = actor ? actor->getComponent<MeshRendererComponent>() : EU::TSharedPointer<MeshRendererComponent>();
		auto lightComponent = actor ? actor->getComponent<LightComponent>() : EU::TSharedPointer<LightComponent>();
		const bool hasMeshRenderer = !meshRenderer.isNull();
		const bool hasLightComponent = !lightComponent.isNull();

		ImGui::PushID(i);
		const bool isSelected = (selectedActorIndex == i);
		if (isSelected) {
			ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.18f, 0.32f, 0.58f, 0.70f));
			ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.22f, 0.38f, 0.66f, 0.85f));
			ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.24f, 0.42f, 0.72f, 0.95f));
		}

		ImVec2 rowSize(ImGui::GetContentRegionAvail().x, 25.0f);
		if (ImGui::Selectable("##actorRow", isSelected, ImGuiSelectableFlags_None, rowSize)) {
			selectedActorIndex = i;
		}

		// Menu contextual por actor. Las acciones se ejecutan de forma diferida
		// en BaseApp para no invalidar punteros que ImGui usa durante este frame.
		if (ImGui::BeginPopupContextItem("ActorContext")) {
			selectedActorIndex = i;
			if (ImGui::MenuItem("Rename", "F2")) {
				m_renameActorIndex = i;
				std::snprintf(m_renameActorBuffer, sizeof(m_renameActorBuffer), "%s", actorName.c_str());
				m_openRenameActorPopup = true;
			}
			if (ImGui::MenuItem("Duplicate", "Ctrl+D")) {
				m_sceneEditorRequest = { true, SceneEditorAction::DuplicateActor, i, std::string() };
			}
			ImGui::Separator();
			if (ImGui::MenuItem("Delete", "Delete")) {
				m_sceneEditorRequest = { true, SceneEditorAction::DeleteActor, i, std::string() };
			}
			ImGui::EndPopup();
		}

		ImVec2 min = ImGui::GetItemRectMin();
		ImVec2 max = ImGui::GetItemRectMax();
		ImDrawList* drawList = ImGui::GetWindowDrawList();
		drawList->AddText(ImVec2(min.x + 9.0f, min.y + 2.0f), ImGui::GetColorU32(ImGuiCol_Text), actorName.c_str());
		drawList->AddText(ImVec2(min.x + 9.0f, min.y + 13.0f), AccentU32(actorTypeColor), actorTypeLabel);

		float badgeX = max.x - 48.0f;
		if (hasMeshRenderer) {
			drawList->AddRectFilled(ImVec2(badgeX, min.y + 5.0f), ImVec2(badgeX + 16.0f, min.y + 20.0f), IM_COL32(55, 99, 157, 210), 3.0f);
			drawList->AddText(ImVec2(badgeX + 5.0f, min.y + 5.0f), IM_COL32(238, 243, 255, 255), "M");
			badgeX += 22.0f;
		}
		if (hasLightComponent) {
			drawList->AddRectFilled(ImVec2(badgeX, min.y + 5.0f), ImVec2(badgeX + 16.0f, min.y + 20.0f), IM_COL32(157, 117, 35, 220), 3.0f);
			drawList->AddText(ImVec2(badgeX + 5.0f, min.y + 5.0f), IM_COL32(255, 247, 225, 255), "L");
		}

		if (isSelected) {
			ImGui::PopStyleColor(3);
		}
		ImGui::PopID();
	}

	// El popup de rename vive fuera del row context popup para que no quede
	// anidado en una ventana que desaparece cuando cambia el filtro/seleccion.
	if (m_openRenameActorPopup && m_renameActorIndex >= 0 &&
		m_renameActorIndex < static_cast<int>(actors.size())) {
		if (m_renameActorBuffer[0] == '\0' && !actors[m_renameActorIndex].isNull()) {
			std::snprintf(m_renameActorBuffer, sizeof(m_renameActorBuffer), "%s",
				actors[m_renameActorIndex]->getName().c_str());
		}
		ImGui::OpenPopup("Rename Actor");
		m_openRenameActorPopup = false;
	}

	if (ImGui::BeginPopupModal("Rename Actor", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
		ImGui::TextUnformatted("Actor name");
		ImGui::SetNextItemWidth(320.0f);
		const bool submitted = ImGui::InputText("##ActorName", m_renameActorBuffer,
			sizeof(m_renameActorBuffer), ImGuiInputTextFlags_EnterReturnsTrue);
		const bool canRename = m_renameActorIndex >= 0 &&
			m_renameActorIndex < static_cast<int>(actors.size()) &&
			m_renameActorBuffer[0] != '\0';

		if ((submitted || ImGui::Button("Rename")) && canRename) {
			m_sceneEditorRequest = { true, SceneEditorAction::RenameActor,
				m_renameActorIndex, std::string(m_renameActorBuffer) };
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel")) {
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}

	ImGui::End();
}

void GUI::editTransform(Camera& cam, Window& window, EU::TSharedPointer<Actor> actor)
{
	(void)window;
	if (actor.isNull()) return;
	auto transform = actor->getComponent<Transform>();
	if (transform.isNull()) return;

	const float rectX = m_viewportPos.x;
	const float rectY = m_viewportPos.y;
	const float rectW = m_viewportSize.x;
	const float rectH = m_viewportSize.y;

	if (rectW < 64.0f || rectH < 64.0f)
	{
		m_isUsingGizmo = false;
		m_wasUsingGizmoLastFrame = false;
		return;
	}

	const EU::Vector3 currentPosition = transform->getPosition();
	const EU::Vector3 currentRotation = transform->getRotation();
	const EU::Vector3 currentScale = transform->getScale();
	float pos[3] = { currentPosition.x, currentPosition.y, currentPosition.z };
	float sca[3] = { currentScale.x, currentScale.y, currentScale.z };
	float gizmoRotation[3] = {
		RadToDeg(currentRotation.x),
		RadToDeg(currentRotation.y),
		RadToDeg(currentRotation.z)
	};

	float matrix[16];
	ImGuizmo::RecomposeMatrixFromComponents(pos, gizmoRotation, sca, matrix);

	float view[16], projection[16];
	ToFloatArray(cam.getView(), view);
	ToFloatArray(cam.getProj(), projection);

	ImGuizmo::SetOrthographic(false);
	if (m_viewportDrawList)
		ImGuizmo::SetDrawlist(m_viewportDrawList);
	else
		ImGuizmo::SetDrawlist(ImGui::GetForegroundDrawList());

	// V15: el gizmo de rotacion necesita ser facil de agarrar. Dear ImGuizmo
	// realiza el hit-test alrededor del arco proyectado; engrosar el arco hace que
	// la zona visual y la zona interactiva se sientan como un mismo control.
	ImGuizmo::Style& gizmoStyle = ImGuizmo::GetStyle();
	gizmoStyle.TranslationLineThickness = 3.0f;
	gizmoStyle.TranslationLineArrowSize = 7.0f;
	gizmoStyle.RotationLineThickness = 5.5f;
	gizmoStyle.RotationOuterLineThickness = 4.0f;
	gizmoStyle.ScaleLineThickness = 3.0f;
	gizmoStyle.ScaleLineCircleSize = 7.0f;

	ImGuizmo::SetID(selectedActorIndex >= 0 ? selectedActorIndex + 1 : 1);
	ImGuizmo::SetGizmoSizeClipSpace(
		mCurrentGizmoOperation == ImGuizmo::ROTATE ? 0.145f : 0.12f);
	// Al rotar, mantener los ejes estables evita que el aro cambie de lado justo
	// cuando el cursor intenta capturarlo.
	ImGuizmo::AllowAxisFlip(mCurrentGizmoOperation != ImGuizmo::ROTATE);
	ImGuizmo::SetRect(rectX, rectY, rectW, rectH);

	float snapValue = 25.0f;
	if (mCurrentGizmoOperation == ImGuizmo::ROTATE) snapValue = 5.0f;
	if (mCurrentGizmoOperation == ImGuizmo::TRANSLATE) snapValue = 0.5f;
	float snap[3] = { snapValue, snapValue, snapValue };
	const bool useSnap = ImGui::GetIO().KeyCtrl;

	ImGuizmo::MODE activeMode = mCurrentGizmoMode;
	if (mCurrentGizmoOperation == ImGuizmo::SCALE) activeMode = ImGuizmo::LOCAL;

	const bool wasUsing = m_isUsingGizmo;
	ImGuizmo::OPERATION manipulateOperation = mCurrentGizmoOperation;
	if (mCurrentGizmoOperation == ImGuizmo::ROTATE) {
		manipulateOperation = static_cast<ImGuizmo::OPERATION>(
			ImGuizmo::ROTATE_X | ImGuizmo::ROTATE_Y | ImGuizmo::ROTATE_Z);
	}
	ImGuizmo::Manipulate(
		view,
		projection,
		manipulateOperation,
		activeMode,
		matrix,
		nullptr,
		useSnap ? snap : nullptr);

	const bool isUsing = ImGuizmo::IsUsing();
	m_isUsingGizmo = isUsing;

	if (isUsing)
	{
		float newPos[3], newRot[3], newSca[3];
		ImGuizmo::DecomposeMatrixToComponents(matrix, newPos, newRot, newSca);

		// No volver a escribir los tres componentes en cada operacion. La version
		// anterior descomponia toda la matriz durante ROTATE, lo que introducia
		// pequenos cambios de posicion/escala y hacia la manipulacion poco natural.
		if (mCurrentGizmoOperation == ImGuizmo::TRANSLATE) {
			transform->setPosition(EU::Vector3(newPos[0], newPos[1], newPos[2]));
		}
		else if (mCurrentGizmoOperation == ImGuizmo::ROTATE) {
			// Mantener la representacion Euler mas cercana a la rotacion previa evita
			// saltos visuales 179 -> -181 al cruzar el limite de 180 grados.
			newRot[0] = ClosestEquivalentDegrees(newRot[0], gizmoRotation[0]);
			newRot[1] = ClosestEquivalentDegrees(newRot[1], gizmoRotation[1]);
			newRot[2] = ClosestEquivalentDegrees(newRot[2], gizmoRotation[2]);
			transform->setRotation(EU::Vector3(
				DegToRad(newRot[0]),
				DegToRad(newRot[1]),
				DegToRad(newRot[2])));
		}
		else if (mCurrentGizmoOperation == ImGuizmo::SCALE) {
			const float safeX = (std::max)(0.0001f, newSca[0]);
			const float safeY = (std::max)(0.0001f, newSca[1]);
			const float safeZ = (std::max)(0.0001f, newSca[2]);
			transform->setScale(EU::Vector3(safeX, safeY, safeZ));
		}
	}

	// Un drag completo equivale a UNA accion del historial, no a un snapshot por
	// frame. Esto hace Ctrl+Z util y mantiene el historial compacto.
	if (wasUsing && !isUsing) {
		const char* label = "Transform Actor";
		if (mCurrentGizmoOperation == ImGuizmo::ROTATE) label = "Rotate Actor";
		else if (mCurrentGizmoOperation == ImGuizmo::TRANSLATE) label = "Move Actor";
		else if (mCurrentGizmoOperation == ImGuizmo::SCALE) label = "Scale Actor";
		requestHistoryCommit(label);
	}
	m_wasUsingGizmoLastFrame = isUsing;
}

void GUI::drawGizmoToolbar()
{
    // Overlay de transformacion flotante. Se mantiene independiente del layout
    // para no robar espacio al Scene View.
    if (m_viewportSize.x < 100.0f || m_viewportSize.y < 80.0f) return;
    ImGui::SetNextWindowPos(ImVec2(m_viewportPos.x + 10.0f, m_viewportPos.y + 10.0f), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.94f);

    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoNav;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 3.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4.0f, 4.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(2.0f, 2.0f));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.020f, 0.023f, 0.028f, 0.95f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.12f, 0.13f, 0.16f, 1.0f));

    if (ImGui::Begin("##ViewportToolsV142", nullptr, flags)) {
        auto modeButton = [&](const char* label, ImGuizmo::OPERATION operation, const char* tooltip) {
            const bool active = mCurrentGizmoOperation == operation;
            if (active) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.34f, 0.64f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.19f, 0.40f, 0.73f, 1.0f));
            }
            if (ImGui::Button(label, ImVec2(24.0f, 22.0f))) {
                if (operation == ImGuizmo::ROTATE && mCurrentGizmoOperation != ImGuizmo::ROTATE)
                    mCurrentGizmoMode = ImGuizmo::WORLD;
                mCurrentGizmoOperation = operation;
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tooltip);
            if (active) ImGui::PopStyleColor(2);
            ImGui::SameLine();
        };
        modeButton("W", ImGuizmo::TRANSLATE, "Move");
        modeButton("E", ImGuizmo::ROTATE, "Rotate");
        modeButton("R", ImGuizmo::SCALE, "Scale");

        ImGui::TextDisabled("|");
        ImGui::SameLine();
        const bool localWorldAvailable = mCurrentGizmoOperation != ImGuizmo::SCALE;
        if (!localWorldAvailable) ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.45f);
        const bool modePressed = ImGui::Button(mCurrentGizmoMode == ImGuizmo::WORLD ? "World" : "Local", ImVec2(44.0f, 22.0f));
        if (localWorldAvailable && modePressed)
            mCurrentGizmoMode = (mCurrentGizmoMode == ImGuizmo::WORLD) ? ImGuizmo::LOCAL : ImGuizmo::WORLD;
        if (!localWorldAvailable) ImGui::PopStyleVar();
    }
    ImGui::End();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(3);
}

void GUI::drawStudioTopRibbon()
{
    ImGuiViewport* viewport = ImGui::GetMainViewport();

    // Menu superior fino.
    ImGui::SetNextWindowPos(viewport->Pos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(viewport->Size.x, kStudioMenuHeight), ImGuiCond_Always);
    ImGuiWindowFlags menuFlags =
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_MenuBar;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(7.0f, 2.0f));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.020f, 0.023f, 0.028f, 1.0f));
    if (ImGui::Begin("##StudioMenuBarV142", nullptr, menuFlags)) {
        if (ImGui::BeginMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("New Scene", "Ctrl+N"))
                    m_sceneEditorRequest = { true, SceneEditorAction::NewScene, -1, std::string() };
                if (ImGui::MenuItem("Open Scene...", "Ctrl+O"))
                    m_sceneEditorRequest = { true, SceneEditorAction::OpenScene, -1, std::string() };
                if (ImGui::MenuItem("Save", "Ctrl+S")) m_requestSaveScene = true;
                if (ImGui::MenuItem("Save As...", "Ctrl+Shift+S"))
                    m_sceneEditorRequest = { true, SceneEditorAction::SaveSceneAs, -1, std::string() };
                ImGui::Separator();
                if (ImGui::MenuItem("Exit")) show_exit_popup = true;
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Edit")) {
                if (ImGui::MenuItem("Undo", "Ctrl+Z", false, m_canUndo))
                    m_sceneEditorRequest = { true, SceneEditorAction::Undo, -1, std::string() };
                if (ImGui::MenuItem("Redo", "Ctrl+Y", false, m_canRedo))
                    m_sceneEditorRequest = { true, SceneEditorAction::Redo, -1, std::string() };
                ImGui::Separator();
                const bool hasSelection = selectedActorIndex >= 0;
                if (ImGui::MenuItem("Rename Actor", "F2", false, hasSelection)) {
                    m_renameActorIndex = selectedActorIndex;
                    m_renameActorBuffer[0] = '\0';
                    m_openRenameActorPopup = true;
                }
                if (ImGui::MenuItem("Duplicate Actor", "Ctrl+D", false, hasSelection))
                    m_sceneEditorRequest = { true, SceneEditorAction::DuplicateActor, selectedActorIndex, std::string() };
                if (ImGui::MenuItem("Delete Actor", "Delete", false, hasSelection))
                    m_sceneEditorRequest = { true, SceneEditorAction::DeleteActor, selectedActorIndex, std::string() };
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("View")) {
                ImGui::MenuItem("Left Rendering Rail", nullptr, &m_showLeftRail);
                ImGui::MenuItem("Right Inspector Rail", nullptr, &m_showRightRail);
                ImGui::Separator();
                ImGui::MenuItem("Hierarchy", nullptr, &m_showHierarchyPanel);
                ImGui::MenuItem("Inspector", nullptr, &m_showInspectorPanel);
                ImGui::MenuItem("GBuffer", nullptr, &m_showGBufferPanel);
                ImGui::MenuItem("Render Debug", nullptr, &m_showRenderDebugPanel);
                ImGui::Separator();
                ImGui::MenuItem("Material Editor", nullptr, &m_showMaterialEditor);
                ImGui::MenuItem("Asset Browser", nullptr, &m_showAssetBrowser);
                ImGui::Separator();
                ImGui::MenuItem("Lock Studio Layout", nullptr, &m_lockEditorLayout);
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Window")) {
                if (ImGui::MenuItem("Focus Scene View")) {
                    m_showLeftRail = false;
                    m_showRightRail = false;
                }
                if (ImGui::MenuItem("Restore Studio Layout")) {
                    m_showLeftRail = true;
                    m_showRightRail = true;
                    m_lockEditorLayout = true;
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Help")) {
                ImGui::MenuItem("Documentation");
                ImGui::MenuItem("About Wildvine Engine");
                ImGui::EndMenu();
            }
            ImGui::EndMenuBar();
        }
    }
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();

    // Toolbar V14.2: grupos visuales ligeros, sin aumentar la altura.
    ImGui::SetNextWindowPos(ImVec2(viewport->Pos.x, viewport->Pos.y + kStudioMenuHeight), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(viewport->Size.x, kStudioToolbarHeight), ImGuiCond_Always);
    ImGuiWindowFlags toolbarFlags =
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6.0f, 4.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(3.0f, 2.0f));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.029f, 0.033f, 0.040f, 1.0f));
    if (ImGui::Begin("##StudioToolbarV142", nullptr, toolbarFlags)) {
        auto toolButton = [&](const char* label, float width, bool active, const char* tooltip) -> bool {
            if (active) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.33f, 0.61f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.19f, 0.39f, 0.71f, 1.0f));
            }
            const bool pressed = ImGui::Button(label, ImVec2(width, 24.0f));
            if (ImGui::IsItemHovered() && tooltip) ImGui::SetTooltip("%s", tooltip);
            if (active) ImGui::PopStyleColor(2);
            return pressed;
        };
        auto finishGroup = [&]() {
            ImGui::EndGroup();
            const ImVec2 mn = ImGui::GetItemRectMin();
            const ImVec2 mx = ImGui::GetItemRectMax();
            ImGui::GetWindowDrawList()->AddRect(ImVec2(mn.x - 3.0f, mn.y - 2.0f), ImVec2(mx.x + 3.0f, mx.y + 2.0f),
                IM_COL32(55, 61, 72, 170), 2.0f);
            ImGui::SameLine(0.0f, 9.0f);
        };

        ImGui::BeginGroup();
        if (toolButton("Select", 48.0f, false, "Selection cursor")) {}
        ImGui::SameLine();
        if (toolButton("Move", 46.0f, mCurrentGizmoOperation == ImGuizmo::TRANSLATE, "Translate selected actor"))
            mCurrentGizmoOperation = ImGuizmo::TRANSLATE;
        ImGui::SameLine();
        if (toolButton("Rotate", 50.0f, mCurrentGizmoOperation == ImGuizmo::ROTATE, "Rotate selected actor")) {
            if (mCurrentGizmoOperation != ImGuizmo::ROTATE) mCurrentGizmoMode = ImGuizmo::WORLD;
            mCurrentGizmoOperation = ImGuizmo::ROTATE;
        }
        ImGui::SameLine();
        if (toolButton("Scale", 46.0f, mCurrentGizmoOperation == ImGuizmo::SCALE, "Scale selected actor"))
            mCurrentGizmoOperation = ImGuizmo::SCALE;
        finishGroup();

        ImGui::BeginGroup();
        if (toolButton("Mesh +", 52.0f, false, "Import OBJ mesh")) m_requestImportMesh = true;
        ImGui::SameLine();
        if (toolButton("Light +", 52.0f, false, "Create light actor")) m_requestCreateLightActor = true;
        finishGroup();

        ImGui::BeginGroup();
        if (toolButton("Material", 58.0f, m_showMaterialEditor, "Open Material Editor")) m_showMaterialEditor = !m_showMaterialEditor;
        ImGui::SameLine();
        if (toolButton("Assets", 50.0f, m_showAssetBrowser, "Open Asset Browser")) m_showAssetBrowser = !m_showAssetBrowser;
        finishGroup();

        ImGui::BeginGroup();
        if (toolButton("Undo", 46.0f, false, m_canUndo ? "Undo last editor action (Ctrl+Z)" : "Nothing to undo")) {
            if (m_canUndo) m_sceneEditorRequest = { true, SceneEditorAction::Undo, -1, std::string() };
        }
        ImGui::SameLine();
        if (toolButton("Redo", 46.0f, false, m_canRedo ? "Redo last editor action (Ctrl+Y)" : "Nothing to redo")) {
            if (m_canRedo) m_sceneEditorRequest = { true, SceneEditorAction::Redo, -1, std::string() };
        }
        finishGroup();

        ImGui::BeginGroup();
        if (toolButton("Render", 52.0f, m_showLeftRail, "Show/hide Rendering and Diagnostics")) m_showLeftRail = !m_showLeftRail;
        ImGui::SameLine();
        if (toolButton("Panels", 52.0f, m_showRightRail, "Show/hide Hierarchy and Properties")) m_showRightRail = !m_showRightRail;
        ImGui::SameLine();
        if (toolButton(m_lockEditorLayout ? "Lock" : "Free", 44.0f, m_lockEditorLayout, "Lock/unlock workspace"))
            m_lockEditorLayout = !m_lockEditorLayout;
        ImGui::EndGroup();
        ImGui::SameLine(0.0f, 9.0f);

        const char* modeText = (mCurrentGizmoMode == ImGuizmo::WORLD) ? "WORLD" : "LOCAL";
        const float textWidth = ImGui::CalcTextSize(modeText).x;
        const float rightEdge = ImGui::GetWindowContentRegionMax().x;
        if (ImGui::GetCursorPosX() + textWidth < rightEdge)
            ImGui::SetCursorPosX(rightEdge - textWidth);
        ImGui::TextDisabled("%s", modeText);
    }
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
}

void
GUI::drawMaterialEditor(EU::TSharedPointer<Actor> actor) {
	if (!m_showMaterialEditor) {
		return;
	}

	ImGui::SetNextWindowSize(ImVec2(560.0f, 680.0f), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin("Material Editor", &m_showMaterialEditor)) {
		ImGui::End();
		return;
	}

	if (actor.isNull()) {
		ImGui::TextDisabled("No actor selected");
		ImGui::TextWrapped("Select a Static Mesh Actor in the Hierarchy to edit its PBR material.");
		ImGui::End();
		return;
	}

	auto meshRenderer = actor->getComponent<MeshRendererComponent>();
	if (meshRenderer.isNull()) {
		ImGui::Text("%s", actor->getName().c_str());
		ImGui::Separator();
		ImGui::TextDisabled("The selected actor does not have a MeshRendererComponent.");
		ImGui::End();
		return;
	}

	const std::vector<MaterialInstance*>& materialInstances = meshRenderer->getMaterialInstances();
	if (materialInstances.empty()) {
		ImGui::Text("%s", actor->getName().c_str());
		ImGui::Separator();
		ImGui::TextDisabled("The selected mesh does not contain material slots.");
		ImGui::End();
		return;
	}

	if (m_materialEditorSlot < 0) m_materialEditorSlot = 0;
	if (m_materialEditorSlot >= static_cast<int>(materialInstances.size())) {
		m_materialEditorSlot = static_cast<int>(materialInstances.size()) - 1;
	}

	ImGui::Text("Actor: %s", actor->getName().c_str());
	ImGui::TextDisabled("PBR material parameters and texture maps");
	ImGui::Separator();

	if (materialInstances.size() > 1) {
		ImGui::SetNextItemWidth(180.0f);
		ImGui::SliderInt("Material Slot", &m_materialEditorSlot, 0, static_cast<int>(materialInstances.size()) - 1);
	}
	else {
		ImGui::TextDisabled("Material Slot 0");
	}

	MaterialInstance* materialInstance = materialInstances[static_cast<size_t>(m_materialEditorSlot)];
	if (!materialInstance) {
		ImGui::TextDisabled("Invalid material instance.");
		ImGui::End();
		return;
	}

	Material* material = materialInstance->getMaterial();
	MaterialParams& params = materialInstance->getParams();

	ImGui::Spacing();
	ImGui::TextUnformatted("Material Properties");
	ImGui::Separator();
	if (material) {
		static const char* kMaterialDomains[] = { "Opaque", "Masked", "Transparent" };
		int domain = static_cast<int>(material->getDomain());
		ImGui::SetNextItemWidth(220.0f);
		if (ImGui::Combo("Domain", &domain, kMaterialDomains, IM_ARRAYSIZE(kMaterialDomains))) {
			material->setDomain(static_cast<MaterialDomain>(domain));
			requestHistoryCommit("Edit Material");
		}

		if (material->getDomain() == MaterialDomain::Transparent) {
			static const char* kBlendModes[] = { "Opaque", "Alpha", "Additive", "Premultiplied" };
			int blend = static_cast<int>(material->getBlendMode());
			ImGui::SetNextItemWidth(220.0f);
			if (ImGui::Combo("Blend Mode", &blend, kBlendModes, IM_ARRAYSIZE(kBlendModes))) {
				material->setBlendMode(static_cast<BlendMode>(blend));
				requestHistoryCommit("Edit Material");
			}
		}
	}

	ImGui::ColorEdit4("Base Color", &params.baseColor.x);
	if (ImGui::IsItemDeactivatedAfterEdit()) requestHistoryCommit("Edit Material");
	ImGui::SliderFloat("Metallic", &params.metallic, 0.0f, 1.0f, "%.3f");
	if (ImGui::IsItemDeactivatedAfterEdit()) requestHistoryCommit("Edit Material");
	ImGui::SliderFloat("Roughness", &params.roughness, 0.0f, 1.0f, "%.3f");
	if (ImGui::IsItemDeactivatedAfterEdit()) requestHistoryCommit("Edit Material");
	ImGui::SliderFloat("Ambient Occlusion", &params.ao, 0.0f, 1.0f, "%.3f");
	if (ImGui::IsItemDeactivatedAfterEdit()) requestHistoryCommit("Edit Material");
	ImGui::SliderFloat("Normal Intensity", &params.normalScale, 0.0f, 4.0f, "%.2f");
	if (ImGui::IsItemDeactivatedAfterEdit()) requestHistoryCommit("Edit Material");
	ImGui::SliderFloat("Emissive Strength", &params.emissiveStrength, 0.0f, 16.0f, "%.2f");
	if (ImGui::IsItemDeactivatedAfterEdit()) requestHistoryCommit("Edit Material");
	if (material && material->getDomain() == MaterialDomain::Masked) {
		ImGui::SliderFloat("Alpha Cutoff", &params.alphaCutoff, 0.0f, 1.0f, "%.3f");
	if (ImGui::IsItemDeactivatedAfterEdit()) requestHistoryCommit("Edit Material");
	}

	if (ImGui::Button("Reset PBR Values")) {
		params.baseColor = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
		params.metallic = 0.0f;
		params.roughness = 0.55f;
		params.ao = 1.0f;
		params.normalScale = 1.0f;
		params.emissiveStrength = 0.0f;
		params.alphaCutoff = 0.5f;
		requestHistoryCommit("Reset Material");
	}
	ImGui::SameLine();
	if (ImGui::Button("Save Scene  Ctrl+S")) {
		m_requestSaveScene = true;
	}

	ImGui::Spacing();
	ImGui::TextUnformatted("Texture Maps");
	ImGui::Separator();
	ImGui::TextDisabled("Replace copies the selected image into Assets/Textures/MaterialOverrides.");

	const MaterialTextureChannel channels[] = {
		MaterialTextureChannel::Albedo,
		MaterialTextureChannel::Normal,
		MaterialTextureChannel::Metallic,
		MaterialTextureChannel::Roughness,
		MaterialTextureChannel::AO,
		MaterialTextureChannel::Emissive
	};

	if (ImGui::BeginTable("##MaterialTextureGrid", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_BordersInnerV)) {
		for (int channelIndex = 0; channelIndex < IM_ARRAYSIZE(channels); ++channelIndex) {
			const MaterialTextureChannel channel = channels[channelIndex];
			Texture* texture = GetMaterialTexture(materialInstance, channel);
			ImGui::TableNextColumn();
			ImGui::PushID(channelIndex);
			ImGui::BeginChild("##TextureCard", ImVec2(0.0f, 154.0f), true);
			ImGui::TextUnformatted(GetMaterialTextureChannelLabel(channel));
			ImGui::Separator();
			if (texture && texture->m_textureFromImg) {
				ImGui::Image((ImTextureID)texture->m_textureFromImg, ImVec2(64.0f, 64.0f));
			}
			else {
				ImGui::Dummy(ImVec2(64.0f, 64.0f));
			}
			ImGui::SameLine();
			ImGui::BeginGroup();
			const std::string textureName = GetTextureDisplayName(texture);
			ImGui::TextWrapped("%s", textureName.c_str());
			if (ImGui::Button("Replace...")) {
				m_materialTextureRequest.pending = true;
				m_materialTextureRequest.clear = false;
				m_materialTextureRequest.materialSlot = static_cast<size_t>(m_materialEditorSlot);
				m_materialTextureRequest.channel = channel;
			}
			ImGui::SameLine();
			if (ImGui::Button("Reset")) {
				m_materialTextureRequest.pending = true;
				m_materialTextureRequest.clear = true;
				m_materialTextureRequest.materialSlot = static_cast<size_t>(m_materialEditorSlot);
				m_materialTextureRequest.channel = channel;
			}
			ImGui::EndGroup();
			ImGui::EndChild();
			ImGui::PopID();
		}
		ImGui::EndTable();
	}

	ImGui::Spacing();
	ImGui::TextDisabled("Texture replacements and material values are persisted when the scene is saved.");
	ImGui::End();
}


void GUI::drawAssetBrowser(const std::vector<AssetBrowserItem>& assets,
	EU::TSharedPointer<Actor> selectedActor)
{
	if (!m_showAssetBrowser) {
		return;
	}

	ImGui::SetNextWindowSize(ImVec2(760.0f, 560.0f), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin("Asset Browser", &m_showAssetBrowser)) {
		ImGui::End();
		return;
	}

	if (ImGui::Button("Refresh")) {
		m_assetBrowserRequest.pending = true;
		m_assetBrowserRequest.action = AssetBrowserAction::Refresh;
	}
	ImGui::SameLine();
	if (ImGui::Button("Open Assets Folder")) {
		m_assetBrowserRequest.pending = true;
		m_assetBrowserRequest.action = AssetBrowserAction::OpenAssetsFolder;
	}

	ImGui::SameLine();
	ImGui::TextDisabled("%d assets", static_cast<int>(assets.size()));

	ImGui::SetNextItemWidth(260.0f);
	ImGui::InputText("Search", m_assetBrowserSearch, IM_ARRAYSIZE(m_assetBrowserSearch));
	ImGui::SameLine();
	static const char* kCategories[] = { "All", "Models", "Textures", "Materials" };
	ImGui::SetNextItemWidth(140.0f);
	ImGui::Combo("##AssetCategory", &m_assetBrowserCategory, kCategories, IM_ARRAYSIZE(kCategories));

	ImGui::Separator();

	const float detailsHeight = 150.0f;
	ImVec2 available = ImGui::GetContentRegionAvail();
	const float browserHeight = (available.y > detailsHeight + 80.0f) ? (available.y - detailsHeight) : 180.0f;

	if (ImGui::BeginChild("##AssetGridRegion", ImVec2(0.0f, browserHeight), true)) {
		const float cardWidth = 130.0f;
		const int columns = std::max(1, static_cast<int>(ImGui::GetContentRegionAvail().x / cardWidth));
		if (ImGui::BeginTable("##AssetGrid", columns, ImGuiTableFlags_SizingFixedFit)) {
			for (size_t index = 0; index < assets.size(); ++index) {
				const AssetBrowserItem& asset = assets[index];
				if (!AssetMatchesCategory(asset, m_assetBrowserCategory)) continue;
				if (!AssetBrowserTextMatches(asset.name, m_assetBrowserSearch) &&
					!AssetBrowserTextMatches(asset.relativePath, m_assetBrowserSearch)) {
					continue;
				}

				ImGui::TableNextColumn();
				ImGui::PushID(static_cast<int>(index));
				const bool selected = (m_assetBrowserSelectedIndex == static_cast<int>(index));

				if (selected) {
					ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.13f, 0.22f, 0.36f, 0.92f));
				}
				ImGui::BeginChild("##AssetCard", ImVec2(120.0f, 126.0f), true);
				if (selected) {
					ImGui::PopStyleColor();
				}

				const ImVec2 previewSize(76.0f, 76.0f);
				if (asset.type == AssetBrowserItemType::Texture && asset.previewSRV) {
					ImGui::Image((ImTextureID)asset.previewSRV, previewSize);
				}
				else {
					ImVec2 p = ImGui::GetCursorScreenPos();
					ImGui::InvisibleButton("##AssetPreview", previewSize);
					ImDrawList* draw = ImGui::GetWindowDrawList();
					const ImU32 background = asset.type == AssetBrowserItemType::ModelOBJ
						? IM_COL32(54, 94, 150, 255)
						: IM_COL32(112, 82, 45, 255);
					draw->AddRectFilled(p, ImVec2(p.x + previewSize.x, p.y + previewSize.y), background, 8.0f);
					const char* typeLabel = GetAssetBrowserTypeLabel(asset.type);
					const ImVec2 textSize = ImGui::CalcTextSize(typeLabel);
					draw->AddText(ImVec2(p.x + (previewSize.x - textSize.x) * 0.5f,
						p.y + (previewSize.y - textSize.y) * 0.5f),
						IM_COL32(240, 240, 245, 255), typeLabel);
				}

				const bool previewHovered = ImGui::IsItemHovered();
				if (previewHovered && ImGui::IsMouseClicked(0)) {
					m_assetBrowserSelectedIndex = static_cast<int>(index);
				}
				if (previewHovered && ImGui::IsMouseDoubleClicked(0) &&
					asset.type == AssetBrowserItemType::ModelOBJ) {
					m_assetBrowserRequest.pending = true;
					m_assetBrowserRequest.action = AssetBrowserAction::ImportOBJ;
					m_assetBrowserRequest.path = asset.relativePath;
				}

				if (asset.type == AssetBrowserItemType::ModelOBJ && ImGui::BeginDragDropSource()) {
					ImGui::SetDragDropPayload("WV_ASSET_OBJ",
						asset.relativePath.c_str(),
						asset.relativePath.size() + 1);
					ImGui::TextUnformatted("Import OBJ");
					ImGui::TextDisabled("%s", asset.name.c_str());
					ImGui::EndDragDropSource();
				}

				ImGui::TextWrapped("%s", asset.name.c_str());
				ImGui::EndChild();

				if (ImGui::IsItemClicked()) {
					m_assetBrowserSelectedIndex = static_cast<int>(index);
				}
				ImGui::PopID();
			}
			ImGui::EndTable();
		}
	}
	ImGui::EndChild();

	ImGui::Separator();
	if (m_assetBrowserSelectedIndex < 0 ||
		m_assetBrowserSelectedIndex >= static_cast<int>(assets.size())) {
		ImGui::TextDisabled("Select an asset. Double-click an OBJ to import it, or drag it into the Viewport.");
		ImGui::End();
		return;
	}

	const AssetBrowserItem& selectedAsset = assets[static_cast<size_t>(m_assetBrowserSelectedIndex)];
	ImGui::Text("%s", selectedAsset.name.c_str());
	ImGui::TextDisabled("%s  |  %s",
		GetAssetBrowserTypeLabel(selectedAsset.type),
		selectedAsset.relativePath.c_str());

	if (selectedAsset.type == AssetBrowserItemType::ModelOBJ) {
		if (ImGui::Button("Import Model")) {
			m_assetBrowserRequest.pending = true;
			m_assetBrowserRequest.action = AssetBrowserAction::ImportOBJ;
			m_assetBrowserRequest.path = selectedAsset.relativePath;
		}
		ImGui::SameLine();
		ImGui::TextDisabled("You can also double-click or drag this OBJ into the Viewport.");
	}
	else if (selectedAsset.type == AssetBrowserItemType::Texture) {
		auto meshRenderer = selectedActor.isNull()
			? EU::TSharedPointer<MeshRendererComponent>()
			: selectedActor->getComponent<MeshRendererComponent>();

		if (meshRenderer.isNull() || meshRenderer->getMaterialInstances().empty()) {
			ImGui::TextDisabled("Select a Static Mesh Actor to apply this texture.");
		}
		else {
			const std::vector<MaterialInstance*>& materials = meshRenderer->getMaterialInstances();
			if (m_assetBrowserMaterialSlot < 0) m_assetBrowserMaterialSlot = 0;
			if (m_assetBrowserMaterialSlot >= static_cast<int>(materials.size())) {
				m_assetBrowserMaterialSlot = static_cast<int>(materials.size()) - 1;
			}

			if (materials.size() > 1) {
				ImGui::SetNextItemWidth(140.0f);
				ImGui::SliderInt("Material Slot", &m_assetBrowserMaterialSlot, 0, static_cast<int>(materials.size()) - 1);
			}
			else {
				ImGui::TextDisabled("Material Slot 0");
			}

			static const char* kTextureChannels[] = {
				"Albedo", "Normal", "Metallic", "Roughness", "AO", "Emissive"
			};
			ImGui::SetNextItemWidth(170.0f);
			ImGui::Combo("Texture Channel", &m_assetBrowserTextureChannel,
				kTextureChannels, IM_ARRAYSIZE(kTextureChannels));

			if (ImGui::Button("Apply To Selected Material")) {
				m_assetBrowserRequest.pending = true;
				m_assetBrowserRequest.action = AssetBrowserAction::ApplyTexture;
				m_assetBrowserRequest.path = selectedAsset.relativePath;
				m_assetBrowserRequest.materialSlot = static_cast<size_t>(m_assetBrowserMaterialSlot);
				m_assetBrowserRequest.channel =
					static_cast<MaterialTextureChannel>(m_assetBrowserTextureChannel);
			}
		}
	}
	else {
		ImGui::TextDisabled("MTL files are consumed automatically by their OBJ models.");
	}

	ImGui::End();
}

void GUI::drawViewportPanel(ID3D11ShaderResourceView* viewportSRV,
	const std::vector<EU::TSharedPointer<Actor>>& actors,
	Camera& camera,
	Window& window,
	EU::TSharedPointer<Actor> selectedActor,
	ID3D11ShaderResourceView* lightIconSRV)
{
	(void)actors;
	(void)lightIconSRV;
	const StudioLayout layout = GetStudioLayout(m_showLeftRail, m_showRightRail);
	ApplyLockedPanelRect(layout.viewport, m_lockEditorLayout);
	ImGuiWindowFlags flags =
		ImGuiWindowFlags_NoScrollbar |
		ImGuiWindowFlags_NoScrollWithMouse |
		StudioPanelFlags(m_lockEditorLayout);

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

	if (ImGui::Begin("Scene View", nullptr, flags))
	{
		m_viewportDrawList = ImGui::GetWindowDrawList();

		ImVec2 panelMin = ImGui::GetCursorScreenPos();
		ImVec2 panelSize = ImGui::GetContentRegionAvail();

		if (panelSize.x < 1.0f) panelSize.x = 1.0f;
		if (panelSize.y < 1.0f) panelSize.y = 1.0f;

		if (viewportSRV)
		{
			ImGui::Image((ImTextureID)viewportSRV, panelSize);
		}
		else
		{
			ImGui::InvisibleButton("##ViewportSurface", panelSize);
			ImVec2 itemMin = ImGui::GetItemRectMin();
			ImVec2 itemMax = ImGui::GetItemRectMax();
			ImDrawList* drawList = ImGui::GetWindowDrawList();

			drawList->AddRectFilled(itemMin, itemMax, IM_COL32(20, 20, 25, 255));
			drawList->AddText(
				ImVec2(itemMin.x + 12.0f, itemMin.y + 12.0f),
				IM_COL32(220, 220, 220, 255),
				"Viewport sin textura"
			);
		}

		if (ImGui::BeginDragDropTarget()) {
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("WV_ASSET_OBJ")) {
				const char* droppedPath = static_cast<const char*>(payload->Data);
				if (droppedPath && droppedPath[0] != '\0') {
					m_assetBrowserRequest.pending = true;
					m_assetBrowserRequest.action = AssetBrowserAction::ImportOBJ;
					m_assetBrowserRequest.path = droppedPath;
				}
			}
			ImGui::EndDragDropTarget();
		}

		ImVec2 itemMin = ImGui::GetItemRectMin();
		ImVec2 itemMax = ImGui::GetItemRectMax();
		m_viewportPos = itemMin;
		m_viewportSize = ImVec2(itemMax.x - itemMin.x, itemMax.y - itemMin.y);

		// INPUT STATE: obtain it immediately after the viewport image item.
		m_viewportHovered = ImGui::IsItemHovered();
		m_viewportActive = ImGui::IsItemActive();
		m_viewportFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

		ImGuiIO& io = ImGui::GetIO();
		const bool canNavigate = !io.WantTextInput && !m_isUsingGizmo;
		static bool flyLookActive = false;
		static bool panActive = false;
		static bool orbitActive = false;

		if (m_viewportHovered && canNavigate) {
			if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) flyLookActive = true;
			if (ImGui::IsMouseClicked(ImGuiMouseButton_Middle)) panActive = true;
			if (io.KeyAlt && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !selectedActor.isNull()) {
				orbitActive = true;
			}
		}
		if (!ImGui::IsMouseDown(ImGuiMouseButton_Right)) flyLookActive = false;
		if (!ImGui::IsMouseDown(ImGuiMouseButton_Middle)) panActive = false;
		if (!ImGui::IsMouseDown(ImGuiMouseButton_Left) || !io.KeyAlt) orbitActive = false;

		bool cameraChanged = false;
		if (canNavigate) {
			// Wheel: content-aware dolly. Works even when no actor is selected.
			if (m_viewportHovered && std::fabs(io.MouseWheel) > 1e-6f) {
				DollyCamera(camera, selectedActor, io.MouseWheel);
				cameraChanged = true;
			}

			// F: frame/focus selected actor.
#if defined(IMGUI_VERSION_NUM) && IMGUI_VERSION_NUM >= 18700
			const bool focusPressed = ImGui::IsKeyPressed(ImGuiKey_F, false);
#else
			const bool focusPressed = ImGui::IsKeyPressed('F', false);
#endif
			if ((m_viewportHovered || flyLookActive || panActive || orbitActive) && focusPressed && !selectedActor.isNull()) {
				FocusCamera(camera, selectedActor);
				cameraChanged = true;
			}

			// RMB + mouse = FPS/free-look. RMB + WASD/QE = editor fly camera.
			if (flyLookActive) {
				constexpr float lookSensitivity = 0.0042f;
				if (std::fabs(io.MouseDelta.x) > 0.001f) {
					camera.yaw(io.MouseDelta.x * lookSensitivity);
					cameraChanged = true;
				}
				if (std::fabs(io.MouseDelta.y) > 0.001f) {
					camera.pitch(io.MouseDelta.y * lookSensitivity);
					cameraChanged = true;
				}

				float speed = 4.5f;
				if (io.KeyShift) speed *= 3.25f;
				if (io.KeyCtrl) speed *= 0.28f;
				const float step = speed * (std::max)(io.DeltaTime, 1.0f / 240.0f);

#if defined(IMGUI_VERSION_NUM) && IMGUI_VERSION_NUM >= 18700
				if (IsEditorKeyDown(ImGuiKey_W)) { camera.walk(step); cameraChanged = true; }
				if (IsEditorKeyDown(ImGuiKey_S)) { camera.walk(-step); cameraChanged = true; }
				if (IsEditorKeyDown(ImGuiKey_D)) { camera.strafe(step); cameraChanged = true; }
				if (IsEditorKeyDown(ImGuiKey_A)) { camera.strafe(-step); cameraChanged = true; }
				if (IsEditorKeyDown(ImGuiKey_E)) { RaiseCamera(camera, step); cameraChanged = true; }
				if (IsEditorKeyDown(ImGuiKey_Q)) { RaiseCamera(camera, -step); cameraChanged = true; }
#else
				if (ImGui::IsKeyDown('W')) { camera.walk(step); cameraChanged = true; }
				if (ImGui::IsKeyDown('S')) { camera.walk(-step); cameraChanged = true; }
				if (ImGui::IsKeyDown('D')) { camera.strafe(step); cameraChanged = true; }
				if (ImGui::IsKeyDown('A')) { camera.strafe(-step); cameraChanged = true; }
				if (ImGui::IsKeyDown('E')) { RaiseCamera(camera, step); cameraChanged = true; }
				if (ImGui::IsKeyDown('Q')) { RaiseCamera(camera, -step); cameraChanged = true; }
#endif
			}

			// MMB = pan without changing view direction.
			if (panActive && (std::fabs(io.MouseDelta.x) > 0.001f || std::fabs(io.MouseDelta.y) > 0.001f)) {
				PanCamera(camera, selectedActor, io.MouseDelta.x, io.MouseDelta.y);
				cameraChanged = true;
			}

			// Alt + LMB = orbit the selected actor, preserving a stable world-up horizon.
			if (orbitActive && !selectedActor.isNull() &&
				(std::fabs(io.MouseDelta.x) > 0.001f || std::fabs(io.MouseDelta.y) > 0.001f)) {
				OrbitCamera(camera, selectedActor, io.MouseDelta.x, io.MouseDelta.y);
				cameraChanged = true;
			}
		}

		if (cameraChanged) camera.updateViewMatrix();

		// Minimal viewport HUD. Navigation help appears only while the viewport is hovered,
		// keeping the scene visually clean the rest of the time.
		if (m_viewportSize.x > 260.0f && m_viewportSize.y > 120.0f) {
			ImDrawList* overlay = ImGui::GetWindowDrawList();
			const char* viewLabel = "Perspective  |  Lit";
			const ImVec2 textSize = ImGui::CalcTextSize(viewLabel);
			ImVec2 hudMax(itemMax.x - 10.0f, itemMin.y + 28.0f);
			ImVec2 hudMin(hudMax.x - textSize.x - 14.0f, itemMin.y + 8.0f);
			overlay->AddRectFilled(hudMin, hudMax, IM_COL32(8, 10, 13, 190), 2.0f);
			overlay->AddText(ImVec2(hudMin.x + 8.0f, hudMin.y + 4.0f), IM_COL32(176, 184, 198, 255), viewLabel);

			if (m_viewportHovered && m_viewportSize.x > 650.0f) {
				const char* nav = "RMB Look  |  WASD Fly  |  MMB Pan  |  Alt+LMB Orbit  |  Wheel Zoom  |  F Frame";
				const ImVec2 navSize = ImGui::CalcTextSize(nav);
				ImVec2 navMin(itemMin.x + 10.0f, itemMax.y - navSize.y - 17.0f);
				ImVec2 navMax(navMin.x + navSize.x + 14.0f, itemMax.y - 7.0f);
				overlay->AddRectFilled(navMin, navMax, IM_COL32(8, 10, 13, 155), 2.0f);
				overlay->AddText(ImVec2(navMin.x + 7.0f, navMin.y + 3.0f), IM_COL32(155, 164, 180, 220), nav);
			}
		}
	}
	ImGui::End();

	if (!selectedActor.isNull() && m_viewportFocused) {
		editTransform(camera, window, selectedActor);
	}

	ImGui::PopStyleVar();
}

void GUI::drawRenderDebugPanel(ID3D11ShaderResourceView* preShadowSRV,
    ID3D11ShaderResourceView* finalViewportSRV,
    ID3D11ShaderResourceView* shadowMapSRV)
{
    if (!m_showLeftRail || !m_showRenderDebugPanel) return;
    const StudioLayout layout = GetStudioLayout(m_showLeftRail, m_showRightRail);
    ApplyLockedPanelRect(layout.leftBottom, m_lockEditorLayout);
    ImGuiWindowFlags panelFlags = StudioPanelFlags(m_lockEditorLayout);
    if (!ImGui::Begin("Diagnostics", &m_showRenderDebugPanel, panelFlags)) {
        ImGui::End();
        return;
    }

    struct DebugViewItem { const char* shortLabel; const char* label; ID3D11ShaderResourceView* srv; };
    DebugViewItem items[] = {
        { "Pre", "Pre-Shadow", preShadowSRV },
        { "Final", "Scene Final", finalViewportSRV },
        { "Shadow", "Shadow Map", shadowMapSRV }
    };

    static int selectedView = 0;
    ImGui::TextDisabled("PASS OUTPUT");
    const float total = ImGui::GetContentRegionAvail().x;
    const float buttonW = (std::max)(42.0f, (total - 6.0f) / 3.0f);
    for (int i = 0; i < IM_ARRAYSIZE(items); ++i) {
        if (i > 0) ImGui::SameLine(0.0f, 3.0f);
        ImGui::PushID(i);
        const bool active = selectedView == i;
        if (active) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.12f, 0.28f, 0.52f, 0.95f));
        if (ImGui::Button(items[i].shortLabel, ImVec2(buttonW, 22.0f))) selectedView = i;
        if (active) ImGui::PopStyleColor();
        ImGui::PopID();
    }

    ImGui::Spacing();
    ImGui::TextDisabled("%s", items[selectedView].label);
    ImVec2 available = ImGui::GetContentRegionAvail();
    if (available.x < 1.0f) available.x = 1.0f;
    if (available.y < 1.0f) available.y = 1.0f;
    if (items[selectedView].srv) {
        const float aspect = 1.6f;
        float imageW = available.x;
        float imageH = imageW / aspect;
        if (imageH > available.y) { imageH = available.y; imageW = imageH * aspect; }
        const float xOffset = (std::max)(0.0f, (available.x - imageW) * 0.5f);
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + xOffset);
        ImGui::Image((ImTextureID)items[selectedView].srv, ImVec2(imageW, imageH));
    }
    else {
        ImGui::TextDisabled("No texture bound");
    }
    ImGui::End();
}

void GUI::drawGBufferDebugPanel(ID3D11ShaderResourceView* albedoMetallicSRV,
    ID3D11ShaderResourceView* normalRoughnessSRV,
    ID3D11ShaderResourceView* worldAoSRV,
    ID3D11ShaderResourceView* emissiveAlphaSRV,
    EU::TSharedPointer<Actor> selectedActor)
{
    (void)selectedActor;
    if (!m_showLeftRail || !m_showGBufferPanel) return;
    const StudioLayout layout = GetStudioLayout(m_showLeftRail, m_showRightRail);
    ApplyLockedPanelRect(layout.leftTop, m_lockEditorLayout);
    ImGuiWindowFlags panelFlags = StudioPanelFlags(m_lockEditorLayout);
    if (!ImGui::Begin("Rendering", &m_showGBufferPanel, panelFlags)) {
        ImGui::End();
        return;
    }

    ImGui::TextDisabled("GBUFFER / LIGHTING");
    const char* debugModes[] = { "Lit", "Shadow factor", "Albedo / Metallic", "Normal / Roughness", "World / AO", "Emissive / Alpha" };
    if (m_deferredDebugViewMode < 0 || m_deferredDebugViewMode >= IM_ARRAYSIZE(debugModes)) m_deferredDebugViewMode = 0;
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::Combo("##LightingDebugMode", &m_deferredDebugViewMode, debugModes, IM_ARRAYSIZE(debugModes));
    ImGui::Checkbox("Visualize shadow factor", &m_visualizeDeferredShadowFactor);

    ImGui::Spacing();
    if (ImGui::CollapsingHeader("Post Process", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::TextDisabled("HDR / Deferred final pass");
        ImGui::Checkbox("Enable stack", &m_postProcessEnabled);

        ImGui::Checkbox("Bloom", &m_bloomEnabled);
        ImGui::SameLine();
        ImGui::Checkbox("Tonemap", &m_tonemappingEnabled);
        ImGui::SameLine();
        ImGui::Checkbox("FXAA", &m_fxaaEnabled);

        // The rendering rail is intentionally narrow.  Using the visible text as
        // SliderFloat's label made ImGui place the label to the right of a slider
        // that already consumed the full available width, so the text was clipped.
        // Draw the descriptive label above the control and give the slider a hidden
        // ID instead.  This keeps the compact layout while making every parameter
        // unambiguous during tuning and demonstrations.
        if (m_bloomEnabled) {
            ImGui::Spacing();
            ImGui::TextUnformatted("Bloom Threshold");
            ImGui::SetNextItemWidth(-1.0f);
            ImGui::SliderFloat("##PostBloomThreshold", &m_bloomThreshold, 0.05f, 3.0f, "%.2f");

            ImGui::TextUnformatted("Bloom Intensity");
            ImGui::SetNextItemWidth(-1.0f);
            ImGui::SliderFloat("##PostBloomIntensity", &m_bloomIntensity, 0.0f, 2.5f, "%.2f");
        }
        if (m_tonemappingEnabled) {
            ImGui::TextUnformatted("Exposure");
            ImGui::SetNextItemWidth(-1.0f);
            ImGui::SliderFloat("##PostExposure", &m_postExposure, 0.10f, 4.0f, "%.2f");
        }
        if (m_fxaaEnabled) {
            ImGui::TextUnformatted("FXAA Strength");
            ImGui::SetNextItemWidth(-1.0f);
            ImGui::SliderFloat("##PostFXAAStrength", &m_fxaaStrength, 0.25f, 2.0f, "%.2f");
        }
    }


    ImGui::Spacing();
    if (ImGui::CollapsingHeader("Skybox", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::TextDisabled("Panoramic / Equirectangular 2:1");

        if (ImGui::Checkbox("Enabled##Skybox", &m_skyboxEnabled)) {
            requestHistoryCommit("Edit Skybox");
        }

        ImGui::TextUnformatted("Texture");
        ImGui::TextDisabled("%s", m_skyboxTextureDisplayName.c_str());
        if (ImGui::Button("Browse...##Skybox", ImVec2(82.0f, 0.0f))) {
            m_skyboxEditorRequest.pending = true;
            m_skyboxEditorRequest.action = SkyboxEditorAction::BrowseTexture;
        }
        ImGui::SameLine();
        if (ImGui::Button("Default##Skybox", ImVec2(70.0f, 0.0f))) {
            m_skyboxEditorRequest.pending = true;
            m_skyboxEditorRequest.action = SkyboxEditorAction::ResetDefault;
        }

        ImGui::TextUnformatted("Intensity");
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::SliderFloat("##SkyboxIntensity", &m_skyboxIntensity, 0.0f, 4.0f, "%.2f");
        if (ImGui::IsItemDeactivatedAfterEdit()) requestHistoryCommit("Edit Skybox");

        ImGui::TextUnformatted("Rotation");
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::SliderFloat("##SkyboxRotation", &m_skyboxRotationDegrees, -180.0f, 180.0f, "%.1f deg");
        if (ImGui::IsItemDeactivatedAfterEdit()) requestHistoryCommit("Edit Skybox");

        ImGui::TextUnformatted("Tint");
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::ColorEdit3("##SkyboxTint", m_skyboxTint);
        if (ImGui::IsItemDeactivatedAfterEdit()) requestHistoryCommit("Edit Skybox");
    }

    struct GBufferItem { const char* shortLabel; const char* label; ID3D11ShaderResourceView* srv; };
    GBufferItem items[] = {
        { "A/M", "Albedo / Metallic", albedoMetallicSRV },
        { "N/R", "Normal / Roughness", normalRoughnessSRV },
        { "W/A", "World / AO", worldAoSRV },
        { "E/A", "Emissive / Alpha", emissiveAlphaSRV }
    };

    static int selectedBuffer = 0;
    ImGui::Spacing();
    const float total = ImGui::GetContentRegionAvail().x;
    const float buttonW = (std::max)(36.0f, (total - 9.0f) / 4.0f);
    for (int i = 0; i < IM_ARRAYSIZE(items); ++i) {
        if (i > 0) ImGui::SameLine(0.0f, 3.0f);
        ImGui::PushID(i);
        const bool active = selectedBuffer == i;
        if (active) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.12f, 0.28f, 0.52f, 0.95f));
        if (ImGui::Button(items[i].shortLabel, ImVec2(buttonW, 21.0f))) selectedBuffer = i;
        if (active) ImGui::PopStyleColor();
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", items[i].label);
        ImGui::PopID();
    }

    ImGui::TextDisabled("%s", items[selectedBuffer].label);
    ImVec2 available = ImGui::GetContentRegionAvail();
    if (items[selectedBuffer].srv && available.x > 1.0f && available.y > 1.0f) {
        const float aspect = 1.6f;
        float imageW = available.x;
        float imageH = imageW / aspect;
        if (imageH > available.y) { imageH = available.y; imageW = imageH * aspect; }
        const float xOffset = (std::max)(0.0f, (available.x - imageW) * 0.5f);
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + xOffset);
        ImGui::Image((ImTextureID)items[selectedBuffer].srv, ImVec2(imageW, imageH));
    }
    ImGui::End();
}

void GUI::drawEditorDockspace()
{
    ImGuiViewport* mainViewport = ImGui::GetMainViewport();
    ImVec2 dockPos(mainViewport->Pos.x, mainViewport->Pos.y + kStudioTopOffset);
    ImVec2 dockSize(mainViewport->Size.x,
        (std::max)(1.0f, mainViewport->Size.y - kStudioTopOffset - kStudioStatusHeight));

    ImGuiWindowFlags windowFlags =
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoNavFocus |
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoSavedSettings;

    ImGui::SetNextWindowPos(dockPos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(dockSize, ImGuiCond_Always);
    ImGui::SetNextWindowViewport(mainViewport->ID);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.018f, 0.021f, 0.026f, 1.0f));

    ImGui::Begin("##MainEditorWorkspaceV142", nullptr, windowFlags);
    if (!m_lockEditorLayout) {
        ImGuiID dockspaceId = ImGui::GetID("##EditorDockspaceV142");
        ImGui::DockSpace(dockspaceId, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);
    }
    ImGui::End();

    ImGui::PopStyleColor();
    ImGui::PopStyleVar(3);
}

void GUI::drawEditorStatusBar()
{
    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(vp->Pos.x, vp->Pos.y + vp->Size.y - kStudioStatusHeight), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(vp->Size.x, kStudioStatusHeight), ImGuiCond_Always);

    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(7.0f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.018f, 0.021f, 0.026f, 1.0f));
    if (ImGui::Begin("##StudioStatusBarV142", nullptr, flags)) {
        const float fps = ImGui::GetIO().Framerate;
        ImVec2 statusPos = ImGui::GetCursorScreenPos();
        ImGui::GetWindowDrawList()->AddCircleFilled(
            ImVec2(statusPos.x + 4.0f, statusPos.y + 7.0f), 2.5f, IM_COL32(80, 190, 112, 255));
        ImGui::Dummy(ImVec2(10.0f, 0.0f));
        ImGui::SameLine(0.0f, 2.0f);
        ImGui::TextDisabled("Ready");
        ImGui::SameLine();
        ImGui::TextDisabled("|  D3D11");
        ImGui::SameLine();
        if (selectedActorIndex >= 0) ImGui::TextDisabled("|  Selected #%d", selectedActorIndex + 1);
        else ImGui::TextDisabled("|  No selection");
        ImGui::SameLine();
        ImGui::TextDisabled("|  Viewport %.0fx%.0f", m_viewportSize.x, m_viewportSize.y);

        char fpsText[64] = {};
        sprintf_s(fpsText, "%.0f FPS", fps > 0.0f ? fps : 0.0f);
        const float textWidth = ImGui::CalcTextSize(fpsText).x;
        const float right = ImGui::GetWindowContentRegionMax().x;
        ImGui::SameLine((std::max)(ImGui::GetCursorPosX(), right - textWidth));
        ImGui::TextDisabled("%s", fpsText);
    }
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
}
