/**
 * @file BaseApp.h
 * @brief Declara la API de BaseApp dentro del subsistema Core.
 * @ingroup core
 */
#pragma once
#include "Prerequisites.h"
#include "Window.h"
#include "Device.h"
#include "DeviceContext.h"
#include "SwapChain.h"
#include "Texture.h"
#include "RenderTargetView.h"
#include "DepthStencilView.h"
#include "Viewport.h"
#include "ShaderProgram.h"
#include "MeshComponent.h"
#include "Buffer.h"
#include "SamplerState.h"
#include "Model3D.h"
#include "ECS/Actor.h"
#include "EngineUtilities/GUI/GUI.h"
#include "SceneGraph/SceneGraph.h"
#include "EngineUtilities/Utilities/Camera.h"
#include "EngineUtilities/Utilities/Skybox.h"
#include "EngineUtilities/Utilities/LayoutBuilder.h"
#include "EngineUtilities/Utilities/EditorViewportPass.h"
#include "ECS/LightComponent.h"
#include "ECS/MeshRendererComponent.h"
#include "Rendering/Material.h"
#include "Rendering/MaterialInstance.h"
#include "Rendering/Mesh.h"
#include "Rendering/RenderPipeline.h"
#include "Rendering/RenderScene.h"
#include <string>
extern IMGUI_IMPL_API
LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

/**
 * @class BaseApp
 * @brief Coordina el ciclo de vida principal de Wildvine Engine.
 *
 * `BaseApp` inicializa la ventana, el dispositivo grafico, la interfaz del editor,
 * la escena de prueba y el pipeline de render. Tambien administra el bucle principal
 * de actualizacion, la serializacion basica de escena y la respuesta a cambios de tamano.
 */
class
	BaseApp {
public:
	BaseApp() = default;
	~BaseApp() { destroy(); }

	/**
	 * @brief Prepara subsistemas previos al render.
	 * @return `S_OK` si la aplicacion queda lista para continuar la inicializacion.
	 */
	HRESULT
		awake();

	/**
	 * @brief Ejecuta el bucle principal de la aplicacion.
	 * @param hInst Instancia Win32 actual.
	 * @param nCmdShow Modo inicial de visualizacion de la ventana.
	 * @return Codigo de salida del proceso.
	 */
	int
		run(HINSTANCE hInst, int nCmdShow);

	/**
	 * @brief Inicializa recursos graficos, escena, materiales y renderer.
	 * @return `S_OK` cuando todos los recursos base quedan listos.
	 */
	HRESULT
		init();

	/**
	 * @brief Ejecuta la logica por frame y sincroniza GUI, camara y escena.
	 * @param deltaTime Tiempo transcurrido desde el frame anterior.
	 */
	void
		update(float deltaTime);

	/**
	 * @brief Emite el frame actual en el viewport del editor y en el back buffer final.
	 */
	void
		render();

	/**
	 * @brief Libera recursos del motor en orden seguro de destruccion.
	 */
	void
		destroy();

	/**
	 * @brief Reconstuye recursos dependientes de la resolucion principal.
	 * @param newW Nuevo ancho del area cliente.
	 * @param newH Nuevo alto del area cliente.
	 */
	void
		onResize(unsigned int newW, unsigned int newH);

	/**
	 * @brief Atiende cambios diferidos del viewport interno del editor.
	 */
	void handleEditorViewportResize();

	/**
	 * @brief Serializa la escena actual a disco.
	 * @param path Ruta de salida del archivo `.wvscene`.
	 * @return `true` si la escena se guarda correctamente.
	 */
	bool saveScene(const std::string& path);

	/**
	 * @brief Carga una escena serializada previamente.
	 * @param path Ruta del archivo `.wvscene`.
	 * @return `true` si el contenido se pudo leer y aplicar.
	 */
	bool loadScene(const std::string& path);

	/**
	 * @brief Devuelve la ruta por defecto usada por el editor para persistencia rapida.
	 */
	std::string getDefaultScenePath() const;
private:
	enum class BuiltinMeshKind {
		None = 0,
		Cube = 1,
		Pyramid = 2,
		Floor = 3
	};

	struct ImportedMeshAsset {
		std::unique_ptr<Model3D> model;
		std::unique_ptr<Mesh> renderMesh;
		std::vector<std::unique_ptr<Material>> materialResources;
		std::vector<std::unique_ptr<MaterialInstance>> materials;
		std::vector<std::unique_ptr<Texture>> textures;
		EU::TSharedPointer<Actor> actor;
		std::string sourcePath;
		BuiltinMeshKind builtinKind = BuiltinMeshKind::None;
	};

	struct MaterialTextureOverride {
		Actor* actor = nullptr;
		size_t materialSlot = 0;
		MaterialTextureChannel channel = MaterialTextureChannel::Albedo;
		std::unique_ptr<Texture> texture;
		Texture* originalTexture = nullptr;
		std::string sourcePath;
	};

	struct SceneHistorySnapshot {
		std::string snapshotPath;
		std::string scenePath;
		std::string label;
		int selectedActorIndex = -1;
	};

	EU::TSharedPointer<Actor> createLightActor(const std::string& name = std::string());
	bool importOBJFromDialog();
	bool importOBJModel(const std::string& path);
	bool attachOBJAssetToActor(const std::string& path, const EU::TSharedPointer<Actor>& actor, bool autoPlace);
	bool tryRestoreLegacyOBJActor(const EU::TSharedPointer<Actor>& actor);
	const ImportedMeshAsset* findImportedMeshAsset(const Actor* actor) const;
	void handlePendingMaterialTextureEdit();
	void handlePendingAssetBrowserAction();
	void handlePendingSkyboxEdit();
	bool loadPanoramicSkybox(const std::string& path);
	bool copySkyboxTextureIntoProject(const std::string& sourcePath, std::string& outPortablePath);
	void resetSkyboxToDefault();
	void updateAssetBrowserCatalog(float deltaTime);
	bool refreshAssetBrowserCatalog(bool force = false);
	bool applyMaterialTextureOverride(const EU::TSharedPointer<Actor>& actor, size_t materialSlot, MaterialTextureChannel channel, const std::string& path);
	bool clearMaterialTextureOverride(const EU::TSharedPointer<Actor>& actor, size_t materialSlot, MaterialTextureChannel channel);
	MaterialTextureOverride* findMaterialTextureOverride(const Actor* actor, size_t materialSlot, MaterialTextureChannel channel);
	const MaterialTextureOverride* findMaterialTextureOverride(const Actor* actor, size_t materialSlot, MaterialTextureChannel channel) const;

	void handlePendingSceneEditorAction();
	bool openSceneFromDialog();
	bool saveSceneAsFromDialog();
	void createNewScene();
	void clearCurrentSceneActors();
	bool duplicateActorAtIndex(int actorIndex);
	bool deleteActorAtIndex(int actorIndex);
	bool renameActorAtIndex(int actorIndex, const std::string& newName);
	std::string makeUniqueActorName(const std::string& desiredName, const Actor* ignoreActor = nullptr) const;
	bool attachBuiltinMeshToActor(BuiltinMeshKind kind, const EU::TSharedPointer<Actor>& actor);
	bool tryRestoreLegacyBuiltinActor(const EU::TSharedPointer<Actor>& actor);
	void registerBuiltinActorMetadata(const EU::TSharedPointer<Actor>& actor, BuiltinMeshKind kind);
	void removeActorOwnedResources(const Actor* actor);
	void copyActorEditableState(const EU::TSharedPointer<Actor>& source, const EU::TSharedPointer<Actor>& destination);
	bool isValidSceneFileHeader(const std::string& path) const;

	// Historial real del editor. Cada estado confirmado se serializa a un snapshot
	// temporal reutilizando el formato WVSCENE, lo que permite deshacer tambien
	// actores importados, materiales y eliminaciones sin duplicar la serializacion.
	void resetSceneHistory(const std::string& label = "Initial Scene");
	void commitSceneHistory(const std::string& label);
	bool undoSceneHistory();
	bool redoSceneHistory();
	bool restoreSceneHistorySnapshot(size_t historyIndex);
	bool captureSceneHistorySnapshot(const std::string& label, SceneHistorySnapshot& outSnapshot);
	void cleanupSceneHistory();
	void trimSceneHistory();
	bool canUndoSceneHistory() const;
	bool canRedoSceneHistory() const;

	static LRESULT CALLBACK
		WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);


private:
	Window                              m_window;
	Device															m_device;
	DeviceContext										m_deviceContext;
	SwapChain                           m_swapChain;
	Texture                             m_backBuffer;
	RenderTargetView									  m_renderTargetView;
	Texture                             m_depthStencil;
	DepthStencilView									  m_depthStencilView;
	Viewport                            m_viewport;
	ShaderProgram												m_shaderProgram;
	//Buffer															m_cbNeverChanges;
	//Buffer															m_cbChangeOnResize;
	bool m_d3dReady = false;
	Buffer m_constantBuffer;
	CBMain m_constantBufferStruct;

	// Textures
	Texture m_AlbedoSRV;
	Texture m_MetallicSRV;
	Texture m_RoughnessSRV;
	Texture m_AOSRV;
	Texture m_NormalSRV;
	Texture m_EmissiveSRV;
	Texture m_drakefireAlbedoSRV;
	Texture m_drakefireNormalSRV;
	Texture m_drakefireMetallicSRV;
	Texture m_drakefireRoughnessSRV;
	Texture m_drakefireAOSRV;
	Texture m_toadAlbedoSRV;
	Texture m_toadNormalSRV;
	Texture m_toadMetallicSRV;
	Texture m_toadRoughnessSRV;
	Texture m_toadAOSRV;
	Texture m_toadGlassAlbedoSRV;
	Texture m_toadGlassNormalSRV;
	Texture m_toadGlassRoughnessSRV;
	Texture m_toadHeadAlbedoSRV;
	Texture m_toadHeadNormalSRV;
	Texture m_toadHeadRoughnessSRV;

	Camera															m_camera;

	SceneGraph												m_sceneGraph;
	std::vector<EU::TSharedPointer<Actor>> m_actors;
	std::vector<std::unique_ptr<ImportedMeshAsset>> m_importedMeshAssets;
	std::vector<MaterialTextureOverride> m_materialTextureOverrides;
	std::vector<AssetBrowserItem> m_assetBrowserItems;
	std::vector<std::unique_ptr<Texture>> m_assetBrowserPreviewTextures;
	unsigned long long m_assetBrowserFingerprint = 0;
	float m_assetBrowserRefreshTimer = 0.0f;
	EU::TSharedPointer<Actor> m_cyberGun;
	EU::TSharedPointer<Actor> m_drakefirePistol;
	EU::TSharedPointer<Actor> m_sciFiToad;
	EU::TSharedPointer<Actor> m_directionalLightActor;


	Model3D* m_model = nullptr;
	Model3D* m_drakefireModel = nullptr;
	Model3D* m_toadModel = nullptr;

	//CBChangeOnResize										cbChangesOnResize;
	//CBNeverChanges											cbNeverChanges;
	GUI																m_gui;
	bool m_guiInitialized = false;
	std::string m_currentScenePath;
	std::vector<SceneHistorySnapshot> m_sceneHistory;
	size_t m_sceneHistoryCursor = 0;
	unsigned long long m_sceneHistorySequence = 0;
	std::string m_sceneHistoryDirectory;
	bool m_historyCaptureInProgress = false;
	bool m_historyRestoreInProgress = false;
	EU::Vector3 m_cameraPos;

	Skybox m_skybox;
	Texture m_skyboxTex;
	bool m_skyboxReady = false;
	std::string m_skyboxTexturePath;
	Texture m_lightIconTexture;
	RasterizerState m_defaultRasterizer;
	DepthStencilState m_defaultDepthStencil;
	SamplerState m_defaultSampler;
	Mesh m_cyberGunRenderMesh;
	Mesh m_drakefireRenderMesh;
	Mesh m_toadRenderMesh;
	Material m_pbrMaterial;
	Material m_transparentPbrMaterial;
	Material m_cyberGunPbrMaterial;
	Material m_drakefirePbrMaterial;
	Material m_toadPbrMaterial;
	Material m_toadGlassPbrMaterial;
	Material m_toadHeadPbrMaterial;
	MaterialInstance m_cyberGunMaterial;
	MaterialInstance m_drakefireMaterial;
	MaterialInstance m_toadMaterial;
	MaterialInstance m_toadGlassMaterial;
	MaterialInstance m_toadHeadMaterial;

	EditorViewportPass m_editorViewportPass;
	RenderPipeline m_renderPipeline;
	RenderScene m_renderScene;
	bool m_editorViewportResizePending = false;
	unsigned int m_pendingViewportWidth = 1;
	unsigned int m_pendingViewportHeight = 1;

	unsigned int m_lastRequestedViewportWidth = 1;
	unsigned int m_lastRequestedViewportHeight = 1;
	int m_viewportResizeStableFrames = 0;
};


