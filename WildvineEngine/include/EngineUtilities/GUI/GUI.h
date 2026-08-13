/**
 * @file GUI.h
 * @brief Declara la API de GUI dentro del subsistema GUI.
 * @ingroup gui
 */
#pragma once
#include "Prerequisites.h"
#include <cstring>
#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include "ImGuizmo.h"

class Viewport;
class Window;
class Device;
class DeviceContext;
class Actor;
class Camera;

/**
 * @enum MaterialTextureChannel
 * @brief Canales de textura editables desde el Material Editor.
 */
enum class MaterialTextureChannel {
  Albedo = 0,
  Normal,
  Metallic,
  Roughness,
  AO,
  Emissive
};

/**
 * @struct MaterialTextureEditRequest
 * @brief Solicitud diferida para reemplazar o restaurar una textura de material.
 *
 * La operacion se consume al inicio del siguiente frame para evitar destruir un
 * SRV que ImGui todavia tenga referenciado en sus draw commands del frame actual.
 */
struct MaterialTextureEditRequest {
  bool pending = false;
  bool clear = false;
  size_t materialSlot = 0;
  MaterialTextureChannel channel = MaterialTextureChannel::Albedo;
};


/**
 * @enum AssetBrowserItemType
 * @brief Tipo de recurso mostrado por el navegador de Assets.
 */
enum class AssetBrowserItemType {
  ModelOBJ = 0,
  Texture,
  MaterialMTL
};

/**
 * @struct AssetBrowserItem
 * @brief Entrada ligera del catalogo de Assets que la GUI puede presentar.
 *
 * La vida del SRV de preview pertenece a BaseApp; GUI solo lo consume durante
 * el frame actual.
 */
struct AssetBrowserItem {
  AssetBrowserItemType type = AssetBrowserItemType::Texture;
  std::string name;
  std::string relativePath;
  ID3D11ShaderResourceView* previewSRV = nullptr;
};

/**
 * @enum AssetBrowserAction
 * @brief Acciones diferidas emitidas por el Asset Browser.
 */
enum class AssetBrowserAction {
  None = 0,
  ImportOBJ,
  ApplyTexture,
  Refresh,
  OpenAssetsFolder
};

/**
 * @struct AssetBrowserRequest
 * @brief Solicitud que BaseApp consume al inicio del siguiente frame.
 */
struct AssetBrowserRequest {
  bool pending = false;
  AssetBrowserAction action = AssetBrowserAction::None;
  std::string path;
  size_t materialSlot = 0;
  MaterialTextureChannel channel = MaterialTextureChannel::Albedo;
};


/**
 * @enum SceneEditorAction
 * @brief Acciones de gestion de escena/actores emitidas por la interfaz.
 */
enum class SceneEditorAction {
  None = 0,
  NewScene,
  OpenScene,
  SaveSceneAs,
  DuplicateActor,
  DeleteActor,
  RenameActor
};

/**
 * @struct SceneEditorRequest
 * @brief Solicitud diferida que BaseApp ejecuta al inicio del siguiente frame.
 *
 * Las operaciones destructivas (eliminar/cargar/nueva escena) no se ejecutan
 * mientras ImGui conserva referencias al actor seleccionado durante el frame.
 */
struct SceneEditorRequest {
  bool pending = false;
  SceneEditorAction action = SceneEditorAction::None;
  int actorIndex = -1;
  std::string text;
};

/**
 * @class GUI
 * @brief Centraliza la interfaz del editor construida sobre ImGui e ImGuizmo.
 *
 * La clase expone paneles de viewport, depuracion de render, outliner e inspector.
 * Tambien recopila interacciones del usuario que despues consume `BaseApp`.
 */
class 
GUI {
public:
	GUI()  = default;
	~GUI() = default;

  /**
   * @brief Inicializa estado interno previo a la integracion con ImGui.
   */
  void 
  awake();

  /**
   * @brief Configura los backends de ImGui para Win32 y Direct3D 11.
   */
	bool
  init(Window& window, Device& device, DeviceContext& deviceContext);

  /**
   * @brief Actualiza el frame de ImGui y el estado de la ventana del editor.
   */
  void 
  update(Viewport& viewport, Window& window);
  
  /**
   * @brief Renderiza todos los paneles activos del editor.
   */
  void 
  render();
  
  void 
  destroy();

  void 
  ToolBar();

  
  void 
  closeApp();

  void
  toolTipData();

  void
  appleLiquidStyle(float opacity /*0..1f*/, ImVec4 accent /*=#0A84FF*/);

  void
  vec3Control(const std::string& label,
              float* values,
              float resetValues = 0.0f,
              float columnWidth = 100.0f,
              bool displayAsDegrees = false);

  void
  inspectorGeneral(EU::TSharedPointer<Actor> actor);

  void
  inspectorContainer(EU::TSharedPointer<Actor> actor);

  void
  outliner(const std::vector<EU::TSharedPointer<Actor>>& actors);

  void 
  editTransform(Camera& cam, Window& window, EU::TSharedPointer<Actor> actor);

  void 
  drawGizmoToolbar();

  void ToFloatArray(const XMMATRIX& mat, float* dest) {
    if (!dest) {
      return;
    }
    XMFLOAT4X4 temp;
    XMStoreFloat4x4(&temp, mat);
    std::memcpy(dest, &temp, sizeof(float) * 16);
  }

  void
  drawStudioTopRibbon();

  void drawViewportPanel(ID3D11ShaderResourceView* viewportSRV,
                         const std::vector<EU::TSharedPointer<Actor>>& actors,
                         Camera& camera,
                         Window& window,
                         EU::TSharedPointer<Actor> selectedActor,
                         ID3D11ShaderResourceView* lightIconSRV);

  void drawRenderDebugPanel(ID3D11ShaderResourceView* preShadowSRV,
                            ID3D11ShaderResourceView* finalViewportSRV,
                            ID3D11ShaderResourceView* shadowMapSRV);

  void drawGBufferDebugPanel(ID3D11ShaderResourceView* albedoMetallicSRV,
                             ID3D11ShaderResourceView* normalRoughnessSRV,
                             ID3D11ShaderResourceView* worldAoSRV,
                             ID3D11ShaderResourceView* emissiveAlphaSRV,
                             EU::TSharedPointer<Actor> selectedActor);

  /**
   * @brief Dibuja el editor dedicado del material del actor seleccionado.
   */
  void drawMaterialEditor(EU::TSharedPointer<Actor> actor);


  /**
   * @brief Dibuja el navegador interno de Assets.
   */
  void drawAssetBrowser(const std::vector<AssetBrowserItem>& assets,
                        EU::TSharedPointer<Actor> selectedActor);

  /**
   * @brief Consume una accion emitida por el Asset Browser.
   */
  bool consumeAssetBrowserRequest(AssetBrowserRequest& outRequest) {
    if (!m_assetBrowserRequest.pending) {
      return false;
    }
    outRequest = m_assetBrowserRequest;
    m_assetBrowserRequest.pending = false;
    return true;
  }

  /**
   * @brief Consume una accion de gestion de escena/actor.
   */
  bool consumeSceneEditorRequest(SceneEditorRequest& outRequest) {
    if (!m_sceneEditorRequest.pending) {
      return false;
    }
    outRequest = m_sceneEditorRequest;
    m_sceneEditorRequest.pending = false;
    return true;
  }

  /**
   * @brief Consume una solicitud de reemplazo/restauracion de textura.
   */
  bool consumeMaterialTextureEditRequest(MaterialTextureEditRequest& outRequest) {
    if (!m_materialTextureRequest.pending) {
      return false;
    }
    outRequest = m_materialTextureRequest;
    m_materialTextureRequest.pending = false;
    return true;
  }

  bool consumeCreateLightActorRequest() {
    const bool requested = m_requestCreateLightActor;
    m_requestCreateLightActor = false;
    return requested;
  }

  bool consumeImportMeshRequest() {
    const bool requested = m_requestImportMesh;
    m_requestImportMesh = false;
    return requested;
  }

  void drawEditorDockspace();

  /**
   * @brief Consume de forma atomica la solicitud de guardado emitida desde la UI.
   * @return `true` una sola vez por peticion de guardado.
   */
  bool
  consumeSaveSceneRequest() {
    const bool requested = m_requestSaveScene;
    m_requestSaveScene = false;
    return requested;
  }

private:

  bool checkboxValue = true;
  bool checkboxValue2 = false;
  std::vector<const char*> m_objectsNames;
  std::vector<const char*> m_tooltips;

  bool show_exit_popup = false; // Variable de estado para el popup
  bool m_requestSaveScene = false;
  bool m_requestCreateLightActor = false;
  bool m_requestImportMesh = false;
  bool m_showMaterialEditor = false;
  bool m_showAssetBrowser = false;
  int m_materialEditorSlot = 0;
  int m_assetBrowserMaterialSlot = 0;
  int m_assetBrowserTextureChannel = 0;
  int m_assetBrowserCategory = 0;
  char m_assetBrowserSearch[128] = {};
  int m_assetBrowserSelectedIndex = -1;
  MaterialTextureEditRequest m_materialTextureRequest{};
  AssetBrowserRequest m_assetBrowserRequest{};
  SceneEditorRequest m_sceneEditorRequest{};
  bool m_openRenameActorPopup = false;
  int m_renameActorIndex = -1;
  char m_renameActorBuffer[128] = {};
  ImDrawList* m_viewportDrawList = nullptr;
  bool m_viewportActive = false;

public:
  bool m_isUsingGizmo = false;               ///< Indica si el gizmo esta capturando entrada del usuario.
  int selectedActorIndex = -1;               ///< Indice del actor seleccionado en el outliner.
  ImVec2 m_viewportPos = ImVec2(0.0f, 0.0f); ///< Posicion del panel de viewport en pantalla.
  ImVec2 m_viewportSize = ImVec2(0.0f, 0.0f);///< Tamano actual del viewport del editor.
  bool m_viewportHovered = false;            ///< Indica si el cursor esta sobre el viewport.
  bool m_viewportFocused = false;            ///< Indica si el viewport tiene foco de entrada.
  bool m_visualizeDeferredShadowFactor = false;
  int m_deferredDebugViewMode = 0;
};


