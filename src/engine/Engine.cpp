//#include "engine/EngineDependencies.h"
#include "engine/Engine.h"


void Engine::RecreateRenderTargets() {
	int w, h;
	m_rhi->GetSize(w, h);

	m_rtColor = m_rhi->CreateRenderTarget(w, h, 0);
	m_rtNormal = m_rhi->CreateRenderTarget(w, h, 1);
	m_rtPos = m_rhi->CreateRenderTarget(w, h, 2);

	m_rtSSAO = m_rhi->CreateRenderTarget(w, h, 0);
	m_rtSSAOBlur = m_rhi->CreateRenderTarget(w, h, 0);

	m_rtSceneFinal = m_rhi->CreateRenderTarget(w, h, 0);
	m_rtPingPong = m_rhi->CreateRenderTarget(w, h, 0);

	m_ssaoParams.screenSize = { (float)w, (float)h };
	if (m_Clouds) m_Clouds->OnResize(m_rhi.get(), w, h);
}


void Engine::InitSSAO() {
	for (int i = 0; i < 64; ++i) {
		XMFLOAT3 s(
			((float)rand() / RAND_MAX) * 2.0f - 1.0f,
			((float)rand() / RAND_MAX) * 2.0f - 1.0f,
			((float)rand() / RAND_MAX)
		);
		XMVECTOR vec = XMVector3Normalize(XMLoadFloat3(&s));
		float scale = (float)i / 64.0f;
		scale = std::lerp(0.1f, 1.0f, scale * scale);
		vec *= scale;
		XMStoreFloat4(&m_ssaoParams.samples[i], vec);
	}
}


Engine::Engine() {}


Engine::~Engine() {
	OnQuit();
}


bool Engine::OnInit(HINSTANCE hInstance, int nCmdShow) {
	m_apiChoice = 0;
	int res = MessageBoxW(0, L"Yes = DirectX 12\nNo = Other options (DX11 / Vulkan)", L"API Choice - Step 1", MB_YESNO | MB_ICONQUESTION);
	if (res == IDYES) { m_apiChoice = 1; }
	else {
		res = MessageBoxW(0, L"Yes = Vulkan\nNo = DirectX 11", L"API Choice - Step 2", MB_YESNO | MB_ICONQUESTION);
		if (res == IDYES) m_apiChoice = 2; else m_apiChoice = 0;
	}

	WNDCLASSEXW wc = { sizeof(WNDCLASSEXW), CS_CLASSDC, WndProc, 0, 0, hInstance, 0, 0, 0, 0, L"EngineClass", 0 };
	RegisterClassExW(&wc);
	m_hwnd = CreateWindowExW(0, L"EngineClass", g_config.windowTitle, WS_OVERLAPPEDWINDOW, 100, 100, g_config.windowWidth, g_config.windowHeight, 0, 0, hInstance, 0);

	RAWINPUTDEVICE rd = { 1, 2, 0, m_hwnd };
	RegisterRawInputDevices(&rd, 1, sizeof(rd));
	ShowWindow(m_hwnd, nCmdShow);

	if (m_apiChoice == 1) m_rhi = std::make_unique<RHI_DX12>();
	else if (m_apiChoice == 2) m_rhi = std::make_unique<RHI_Vulkan>();
	else m_rhi = std::make_unique<RHI_DX11>();

	if (!m_rhi->Init(m_hwnd, g_config.windowWidth, g_config.windowHeight)) return false;

	m_rhi->ImGuiInit(m_hwnd);
	SetupEngineUIStyle();

	m_loadDialog.SetTitle("Open Scene");
	m_loadDialog.SetTypeFilters({ ".scene" });
	m_loadDialog.SetPwd("assets");

	m_saveDialog = ImGui::FileBrowser(ImGuiFileBrowserFlags_EnterNewFilename | ImGuiFileBrowserFlags_CreateNewDir);
	m_saveDialog.SetTitle("Save Scene As...");
	m_saveDialog.SetTypeFilters({ ".scene" });
	m_saveDialog.SetPwd("assets");

	JPH::RegisterDefaultAllocator();
	JPH::Factory::sInstance = new JPH::Factory();
	JPH::RegisterTypes();

	m_tempAllocator = std::make_unique<JPH::TempAllocatorImpl>(50 * 1024 * 1024);
	m_jobSystem = std::make_unique<JPH::JobSystemThreadPool>(JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers, std::thread::hardware_concurrency() - 1);
	m_bpLayerInterface = std::make_unique<BPLayerInterfaceImpl>();
	m_objVsBpFilter = std::make_unique<ObjectVsBroadPhaseLayerFilterImpl>();
	m_objPairFilter = std::make_unique<ObjectLayerPairFilterImpl>();

	m_physics = std::make_unique<JPH::PhysicsSystem>();
	m_physics->Init(10240, 0, 65536, 20480, *m_bpLayerInterface, *m_objVsBpFilter, *m_objPairFilter);

	m_contactListener = std::make_unique<MyContactListener>(m_physics.get());
	m_physics->SetContactListener(m_contactListener.get());

	m_debugRenderer = std::make_unique<JoltDebugRenderer>();
	ma_result result = ma_engine_init(NULL, &m_audioEngine);
	if (result != MA_SUCCESS) {
		MessageBoxW(m_hwnd, L"Failed to initialize Miniaudio!", L"Audio Error", MB_OK);
		return false;
	}

	// init subsystems assets
	m_assets.Init(m_rhi.get());
	m_sceneManager.Init(m_rhi.get(), m_physics.get(), &m_assets, &m_audioEngine, &m_camera);

	LoadResourcesAndScene();
	m_lastTime = std::chrono::high_resolution_clock::now();

	return true;
}


void Engine::Run() {
	MSG msg = {};
	while (msg.message != WM_QUIT) {
		if (PeekMessage(&msg, 0, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else {
			auto ct = std::chrono::high_resolution_clock::now();
			float dt = std::chrono::duration<float>(ct - m_lastTime).count();
			m_lastTime = ct;
			if (dt > 0.1f) dt = 0.1f;

			if (g_resizeRequested) {
				m_rhi->Resize(g_resizeWidth, g_resizeHeight);
				RecreateRenderTargets();
				g_resizeRequested = false;
			}

			OnInput(dt);
			OnPhysicsUpdate(dt);
			OnUpdate(dt);
			OnRender();

			m_sceneManager.Cleanup();
			Input::EndFrame();
		}
	}
}


void Engine::LoadResourcesAndScene() {
	RecreateRenderTargets();
	InitSSAO();

	// 1. PIPELINE & BUFFERS 
	PipelineConfig fsCfg;
	fsCfg.vsPath = L"shaders/rhi/fullscreen.vert.hlsl";
	fsCfg.cullMode = CullMode::None; fsCfg.depthTest = false; fsCfg.depthWrite = false; fsCfg.numRenderTargets = 1;
	fsCfg.psPath = L"shaders/rhi/ssao.frag.hlsl"; m_ssaoPipeline = m_rhi->CreatePipeline(fsCfg);
	fsCfg.psPath = L"shaders/rhi/ssao_blur.frag.hlsl"; m_ssaoBlurPipeline = m_rhi->CreatePipeline(fsCfg);
	fsCfg.psPath = L"shaders/rhi/ssao_combine.frag.hlsl"; m_ssaoCombinePipeline = m_rhi->CreatePipeline(fsCfg);
	fsCfg.psPath = L"shaders/rhi/ssr.frag.hlsl"; m_ssrPipeline = m_rhi->CreatePipeline(fsCfg);
	fsCfg.psPath = L"shaders/rhi/bloom.frag.hlsl"; m_bloomPipeline = m_rhi->CreatePipeline(fsCfg);
	fsCfg.psPath = L"shaders/rhi/vignette.frag.hlsl"; m_vignettePipeline = m_rhi->CreatePipeline(fsCfg);
	fsCfg.psPath = L"shaders/rhi/copy.frag.hlsl"; m_copyPipeline = m_rhi->CreatePipeline(fsCfg);

	m_shadowMap = m_rhi->CreateShadowTexture(2048, 2048);
	PipelineConfig shadowCfg; shadowCfg.vsPath = L"shaders/rhi/pbr.vert.hlsl"; shadowCfg.cullMode = CullMode::None; shadowCfg.useInstancing = true;
	m_shadowPipeline = m_rhi->CreatePipeline(shadowCfg);

	PipelineConfig sShadowCfg; sShadowCfg.vsPath = L"shaders/rhi/pbr-skinned-shadow.vert.hlsl"; sShadowCfg.cullMode = CullMode::None; sShadowCfg.useInstancing = false; sShadowCfg.isSkinned = true;
	m_skinnedShadowPipeline = m_rhi->CreatePipeline(sShadowCfg);

	m_irradianceMap = m_rhi->CreateDDSTexture(L"assets/textures/ibl/irradiance.dds");
	m_prefilterMap = m_rhi->CreateDDSTexture(L"assets/textures/ibl/prefiltered.dds");
	m_brdfLut = m_rhi->CreateDDSTexture(L"assets/textures/ibl/xBrdf.dds");

	m_globalBuffer = m_rhi->CreateBuffer(BufferType::Constant, nullptr, sizeof(GlobalData) * 500);
	m_instanceBuffer = m_rhi->CreateBuffer(BufferType::Instance, nullptr, sizeof(ObjectData) * 15000);
	m_computeUniformBuffer = m_rhi->CreateBuffer(BufferType::Constant, nullptr, 256 * 15000);
	m_skinnedObjectBuffer = m_rhi->CreateBuffer(BufferType::Constant, nullptr, sizeof(SkinnedObjectData) * 5000);
	m_boneBuffer = m_rhi->CreateBuffer(BufferType::Constant, nullptr, sizeof(SM::Matrix) * MAX_BONES * 5000);

	m_skybox = std::make_unique<Skybox>(); m_skybox->Init(m_rhi.get(), L"assets/textures/ibl/skybox.dds");
	m_atmosphere = std::make_unique<Atmosphere>(); m_atmosphere->Init(m_rhi.get());
	// Vytvoří constant buffer zarovnaný na DX12 standard (násobky 256)


	m_Clouds = std::make_unique<VolumetricClouds>();
	m_Clouds->Init(m_rhi.get());
	PipelineConfig lineCfg; lineCfg.vsPath = L"shaders/rhi/line.vert.hlsl"; lineCfg.psPath = L"shaders/rhi/line.frag.hlsl"; lineCfg.topology = Topology::LineList; lineCfg.cullMode = CullMode::None; lineCfg.depthTest = true; lineCfg.depthWrite = false;
	m_linePipeline = m_rhi->CreatePipeline(lineCfg);
	m_lineBuffer = m_rhi->CreateBuffer(BufferType::Vertex, nullptr, 2000000 * sizeof(Vertex));
	m_lineVertices.reserve(2000000);

	m_camera.pos = { 0.0f, 15.0f, -50.0f };
	m_camera.pitch = 0.1f;


	m_assets.LoadHardcodedAssets();
	m_assets.LoadPrimitiveMeshes(); //
	m_assets.LoadDiskAssets();

	m_simState = SimState::Stopped;

	std::string sceneToLoad = "assets/empty.scene";

	if (fs::exists(sceneToLoad)) {
		m_sceneManager.LoadScene(sceneToLoad);
	}
	else {
		std::cout << "[Engine] Scene level '" << sceneToLoad << "' not found. Generating bootstrapping scene!\n";
		BuildHardcodedScene(); // gen
	}
}


void Engine::BuildHardcodedScene() {
	JPH::BodyInterface& bi = m_physics->GetBodyInterface();

	//   AssetRegistry material 
	std::vector<std::shared_ptr<Material>> brickMats;
	for (int i = 0; i < 4; i++) 
		brickMats.push_back(m_assets.m_allMaterials["Mat_Brick_" + std::to_string(i)]);

	// --- Floor ---
	auto floorObj = std::make_unique<GameObject>("Floor");
	floorObj->transform.position = { 0, -1, 0 }; floorObj->transform.scale = { 2000.0, 2.0, 2000.0 };
	floorObj->AddComponent<MeshRenderer>(m_assets.m_meshes["FloorCube"], m_assets.m_allMaterials["Mat_Static_Floor"], false, "FloorCube");
	floorObj->AddComponent<BoxCollider>(SM::Vector3(2000.0f, 2.0f, 2000.0f));
	floorObj->AddComponent<Rigidbody>(&bi, false);
	m_sceneManager.AddObject(std::move(floorObj));

	// walls
	auto createWall = [&](std::string name, SM::Vector3 pos, SM::Vector3 scale, std::shared_ptr<Material> mat) {
		auto wall = std::make_unique<GameObject>(name);
		wall->transform.position = { (double)pos.x, (double)pos.y, (double)pos.z };
		wall->transform.scale = { scale.x, scale.y, scale.z };
		wall->AddComponent<MeshRenderer>(m_assets.m_meshes["Cube"], mat, true, "Cube");
		wall->AddComponent<BoxCollider>(scale);
		wall->AddComponent<Rigidbody>(&bi, false);
		m_sceneManager.AddObject(std::move(wall));
		};
	createWall("Wall_Left", { -24.65f, 15.0f, 0.0f }, { 2.0f, 40.0f, 2.0f }, m_assets.m_allMaterials["Mat_Wall"]);
	createWall("Wall_Right", { 24.65f, 15.0f, 0.0f }, { 2.0f, 40.0f, 2.0f }, m_assets.m_allMaterials["Mat_Wall"]);
	createWall("Wall_Top", { 0.0f, 34.0f, 0.0f }, { 52.0f, 2.0f, 2.0f }, m_assets.m_allMaterials["Mat_Wall"]);
	createWall("Wall_Back", { 0.0f, 15.0f, 2.0f }, { 52.0f, 40.0f, 1.0f }, m_assets.m_allMaterials["Mat_Mirror"]);

	auto dzoneObj = std::make_unique<GameObject>("DeathZone");
	dzoneObj->transform.position = { 0.0f, -1.0f, 0.0f };
	dzoneObj->AddComponent<BoxCollider>(SM::Vector3(200.0f, 2.0f, 200.0f));
	dzoneObj->AddComponent<Rigidbody>(&bi, false);
	auto dz = dzoneObj->AddComponent<DeathZone>();
	m_sceneManager.AddObject(std::move(dzoneObj));

	auto paddle = std::make_unique<GameObject>("Paddle");
	paddle->transform.position = { 0.0f, 0.0f, 0.0f }; paddle->transform.scale = { 6.0f, 1.0f, 1.0f };
	paddle->AddComponent<MeshRenderer>(m_assets.m_meshes["Cube"], m_assets.m_allMaterials["Mat_Paddle"], true, "Cube");
	paddle->AddComponent<BoxCollider>(SM::Vector3(6.0f, 1.0f, 1.0f));
	paddle->AddComponent<Rigidbody>(&bi, true);
	auto padComp = paddle->AddComponent<Paddle>();
	dz->paddleRef = padComp;
	auto padLight = paddle->AddComponent<PointLight>();
	padLight->color = { 0.0f, 0.5f, 1.0f }; padLight->intensity = 30.0f; padLight->radius = 15.0f;

	auto meshSphere = m_assets.m_meshes["Sphere"];
	auto matBall = m_assets.m_allMaterials["Mat_Ball"];
	padComp->spawnBallCallback = [this, meshSphere, matBall, &bi](float px, float py) {
		auto ball = std::make_unique<GameObject>("Ball");
		ball->transform.position = { (double)px, (double)py, 0.0 };
		ball->transform.scale = { 1.0f, 1.0f, 1.0f };
		ball->AddComponent<MeshRenderer>(meshSphere, matBall, true, "Sphere");
		ball->AddComponent<SphereCollider>(0.5f);
		auto rb = ball->AddComponent<Rigidbody>(&bi, true);
		rb->friction = 0.0f; rb->restitution = 1.0f;
		ball->AddComponent<Ball>();
		ball->AddComponent<ParticleSystemComponent>(m_rhi.get(), L"");
		auto light = ball->AddComponent<PointLight>();
		light->color = { 1.0f, 1.0f, 1.0f }; light->intensity = 20.0f; light->radius = 10.0f;
		ball->Start();
		m_sceneManager.AddObject(std::move(ball));
		};
	m_sceneManager.AddObject(std::move(paddle));

	int bCount = 0;
	for (int y = 0; y < 4; ++y) {
		for (int x = -8; x <= 8; ++x) {
			auto brick = std::make_unique<GameObject>("Brick_" + std::to_string(bCount++));
			brick->transform.position = { static_cast<double>(x * 2.8f), 25.0 - static_cast<double>(y * 1.5f), 0.0 };
			brick->transform.scale = { 2.5f, 1.0f, 1.0f };
			brick->AddComponent<MeshRenderer>(m_assets.m_meshes["Cube"], brickMats[y], true, "Cube");
			brick->AddComponent<BoxCollider>(SM::Vector3(2.5f, 1.0f, 1.0f));
			brick->AddComponent<Rigidbody>(&bi, false);
			brick->AddComponent<Brick>(&m_audioEngine);
			auto ps = brick->AddComponent<ParticleSystemComponent>(m_rhi.get(), L""); ps->localDirection = { 0, 0, -1 }; ps->Stop();
			m_sceneManager.AddObject(std::move(brick));
		}
	}

	// warning: this pipeline config temp is allready in use in AssetRegistry when loading the skeletal meshes, 
	// if you change it here, it will affect all skinned meshes that use the same config. For a more robust solution, consider creating separate pipeline configs for different types of skinned meshes or allowing dynamic configuration when loading assets.
	PipelineConfig skelCfg; 
	skelCfg.vsPath = L"shaders/rhi/pbr-skinned.vert.hlsl"; 
	skelCfg.psPath = L"shaders/rhi/pbr-skinned.frag.hlsl"; 
	skelCfg.useInstancing = false; 
	skelCfg.isSkinned = true; 
	skelCfg.numRenderTargets = 3;

	auto player1 = std::make_unique<GameObject>("scifi-marine");
	player1->transform.position = { -20, 0, -40 }; player1->transform.scale = { 0.05f, 0.05f, 0.05f };
	auto smr1 = player1->AddComponent<SkinnedMeshRenderer>(m_assets.LoadSkeletalMesh("assets/models/Player/Player.fbx", skelCfg), "Player", "assets/models/Player/Player.fbx");
	player1->AddComponent<BoxCollider>(SM::Vector3(15.0f, 15.0f, 15.0f)); player1->AddComponent<Rigidbody>(&bi, false);
	auto animator1 = player1->AddComponent<Animator>(smr1->mesh); animator1->Play(0, 99.0f, 100.0f, true);
	smr1->SetMaterial(0, m_assets.m_allMaterials["Mat_Skinned_ScifiMarine"]);
	player1->AddComponent<PlayerJumper>();

	auto shooterMat = m_assets.m_allMaterials.count("Mat_Wall") ? m_assets.m_allMaterials["Mat_Wall"] : m_assets.m_allMaterials.begin()->second;
	player1->AddComponent<PlayerShooter>(&m_camera, &bi, m_assets.m_meshes["Sphere"], shooterMat,
		[this](std::unique_ptr<GameObject> newObj) {

			this->m_sceneManager.AddObject(std::move(newObj));
		});
	m_sceneManager.AddObject(std::move(player1));

	auto player2 = std::make_unique<GameObject>("marine");
	player2->transform.position = { 20, 0, -40 };
	player2->transform.scale = { 0.05f, 0.05f, 0.05f };
	player2->transform.eulerAngles = SM::Vector3(-90.0f, 180.0f, 0.0f);
	player2->transform.UpdateRotation();
	auto smr2 = player2->AddComponent<SkinnedMeshRenderer>(m_assets.LoadSkeletalMesh("assets/models/USMarines/usmarine.fbx", skelCfg), "USMarine", "assets/models/USMarines/usmarine.fbx");
	// player2->AddComponent<BoxCollider>(SM::Vector3(15.0f, 15.0f, 15.0f));
	// player2->AddComponent<Rigidbody>(&bi, false);
	auto animator2 = player2->AddComponent<Animator>(smr2->mesh);
	auto particleComp1 = player2->AddComponent<ParticleSystemComponent>(m_rhi.get(), L"");
	particleComp1->localDirection = { 0, 0, -1 };
	animator2->Play(0, 10.0f, -1.0f, true);
	smr2->SetMaterial(0, m_assets.m_allMaterials["Mat_Skinned_USMarineBody"]);
	smr2->SetMaterial(1, m_assets.m_allMaterials["Mat_Skinned_USMarineGun"]);
	// NAHRADIT STAR� RAGDOLL T�MTO:
	auto rag2 = player2->AddComponent<SkeletalRagdollComponent>();
	rag2->m_physics = m_sceneManager.m_physics;
	// player2->AddComponent<BulletHitChecker>(&m_sceneManager);
	m_sceneManager.AddObject(std::move(player2));

	auto sunObj = std::make_unique<GameObject>("Sun");
	sunObj->transform.eulerAngles = SM::Vector3(30.0f, -40.0f, 0.0f); sunObj->transform.UpdateRotation();
	auto sl = sunObj->AddComponent<DirectionalLight>(); sl->intensity = 2.0f;
	m_sceneManager.AddObject(std::move(sunObj));

	auto radio = std::make_unique<GameObject>("Radio");
	radio->transform.position = { 10, 0, 0 };
	auto audio = radio->AddComponent<AudioSource>(&m_audioEngine, "assets/audio/loop.mp3");
	audio->isLooping = true;
	m_sceneManager.AddObject(std::move(radio));

	std::vector<std::shared_ptr<Material>> gridMats;
	struct MatDef { XMFLOAT4 c; float r; float m; };
	std::vector<MatDef> matDefs = {
		{ {1.0f, 0.2f, 0.2f, 1.0f}, 0.3f, 0.0f }, { {0.2f, 1.0f, 0.2f, 1.0f}, 0.5f, 0.0f },
		{ {0.2f, 0.5f, 1.0f, 0.3f}, 0.2f, 0.9f }, { {1.0f, 0.8f, 0.1f, 1.0f}, 0.2f, 1.0f },
		{ {0.8f, 0.2f, 0.8f, 1.0f}, 0.4f, 0.0f }, { {0.9f, 0.9f, 0.9f, 1.0f}, 0.1f, 0.1f },
		{ {0.2f, 0.2f, 0.2f, 1.0f}, 0.9f, 0.0f }
	};

	for (int i = 0; i < matDefs.size(); ++i) {
		auto mat = std::make_shared<Material>(m_rhi.get(), "Mat_Instanced_" + std::to_string(i), L"shaders/rhi/pbr.vert.hlsl", L"shaders/rhi/pbr.frag.hlsl", true, false);
		mat->baseColor = matDefs[i].c; mat->roughness = matDefs[i].r; mat->metalness = matDefs[i].m;
		m_assets.m_allMaterials[mat->name] = mat;
		gridMats.push_back(mat);
	}

	/*int entityCount = 0;
	float spacing = 2.5f;
	for (int y = 0; y < 12; ++y) {
	for (int x = -10; x < 10; ++x) {
	for (int z = -10; z < 10; ++z) {
	    auto go = std::make_unique<GameObject>("Entity_" + std::to_string(entityCount++));
	    float jitterX = ((rand() % 100) / 100.0f) * 0.8f - 0.4f;
	    float jitterY = ((rand() % 100) / 100.0f) * 0.8f - 0.4f;
	    float jitterZ = ((rand() % 100) / 100.0f) * 0.8f - 0.4f;

	    go->transform.position = { static_cast<double>(x * spacing) + jitterX, 10.0 + static_cast<double>(y * spacing) + jitterY, static_cast<double>(z * spacing) + jitterZ };
	    go->transform.eulerAngles = SM::Vector3(static_cast<float>(rand() % 360), static_cast<float>(rand() % 360), static_cast<float>(rand() % 360));
	    go->transform.UpdateRotation();

	    float s = 0.6f + ((rand() % 100) / 100.0f) * 0.9f;
	    go->transform.scale = { s, s, s };

	    auto randomMat = gridMats[rand() % gridMats.size()];
	    int shapeType = rand() % 3;

	    if (shapeType == 0) {
	        go->AddComponent<MeshRenderer>(m_assets.m_meshes["Cube"], randomMat, true, "Cube");
	        go->AddComponent<BoxCollider>(SM::Vector3(1.0f * s, 1.0f * s, 1.0f * s));
	        go->AddComponent<RotatingObstacle>();
	    }
	    else if (shapeType == 1) {
	        go->AddComponent<MeshRenderer>(m_assets.m_meshes["Sphere"], randomMat, true, "Sphere");
	        go->AddComponent<SphereCollider>(0.5f * s);
	        go->AddComponent<BouncingJumper>();
	    }
	    else {
	        go->AddComponent<MeshRenderer>(m_assets.m_meshes["Capsule"], randomMat, true, "Capsule");
	        go->AddComponent<CapsuleCollider>(0.5f * s, 0.5f * s);
	        go->AddComponent<PlayerJumper>();
	    }
	    go->AddComponent<Rigidbody>(&bi, true);
	    m_sceneManager.AddObject(std::move(go));
	}
	}
	}*/

	m_sceneManager.StartAll();
	std::cout << ">>> BOOTSTRAP HOTOV! Bezte v UI do File -> Save Scene a ulozte jako 'default_scene.json' <<<\n";
}


void Engine::OnInput(float dt) {
	if (Input::GetKeyDown(VK_TAB)) {
		m_cameraActive = !m_cameraActive;
		ShowCursor(!m_cameraActive);
	}

	if (m_cameraActive) {
		m_camera.Update(dt);
	}
}


void Engine::OnPhysicsUpdate(float dt) {
	if (m_simState == SimState::Playing) {
		m_accumulator += dt;
		const float fixedDt = 1.0f / 60.0f;

		while (m_accumulator >= fixedDt) {
			m_sceneManager.FixedUpdate(fixedDt);

			m_physics->Update(fixedDt, 2, m_tempAllocator.get(), m_jobSystem.get());

			if (m_contactListener) {
				m_contactListener->ProcessEvents();
			}

			m_accumulator -= fixedDt;
		}
	}
}


void Engine::OnUpdate(float dt) {
	if (m_simState == SimState::Playing) {
		m_sceneManager.Update(dt);
	}
}


void Engine::OnRender() {
	int curW, curH;
	m_rhi->GetSize(curW, curH);
	float aspect = (curH == 0) ? 1.0f : static_cast<float>(curW) / static_cast<float>(curH);

	XMMATRIX view = m_camera.GetViewMatrix();
	XMMATRIX proj = XMMatrixPerspectiveFovLH(XM_PIDIV4, aspect, 0.1f, 20000.0f);
	ma_engine_listener_set_position(&m_audioEngine, 0, m_camera.pos.x, m_camera.pos.y, m_camera.pos.z);

	XMMATRIX invView = XMMatrixInverse(nullptr, view);
	XMFLOAT3 camForward(invView.r[2].m128_f32[0], invView.r[2].m128_f32[1], invView.r[2].m128_f32[2]);
	XMFLOAT3 camUp(invView.r[1].m128_f32[0], invView.r[1].m128_f32[1], invView.r[1].m128_f32[2]);

	ma_engine_listener_set_direction(&m_audioEngine, 0, camForward.x, camForward.y, camForward.z);
	ma_engine_listener_set_world_up(&m_audioEngine, 0, camUp.x, camUp.y, camUp.z);

	std::map<Material*, std::map<Mesh*, std::vector<ObjectData>>> renderBatches;
	GameObject* floorObject = nullptr;

	DirectionalLight* mainSun = nullptr;
	Transform* sunTransform = nullptr;
	std::vector<PointLight*> activePointLights;
	std::vector<Transform*> pointLightTransforms;

	for (const auto& obj : m_sceneManager.m_objects) {
		if (!obj->isPendingDestroy) {
			if (auto dl = obj->GetComponent<DirectionalLight>()) {
				if (!mainSun) { mainSun = dl; sunTransform = &obj->transform; }
			}
			if (auto pl = obj->GetComponent<PointLight>()) {
				if (activePointLights.size() < 16) {
					activePointLights.push_back(pl);
					pointLightTransforms.push_back(&obj->transform);
				}
			}
		}

		if (obj->name == "Floor") { floorObject = obj.get(); continue; }
		if (obj->GetComponent<SkinnedMeshRenderer>()) continue;

		if (auto mr = obj->GetComponent<MeshRenderer>()) {
			if (mr->mesh && mr->material) {
				renderBatches[mr->material.get()][mr->mesh.get()].push_back(mr->GetInstanceData());
			}
		}
	}

	std::vector<ObjectData> allInstances;
	allInstances.reserve(15000);

	struct BatchInfo { Mesh* mesh; Material* mat; UINT offset; UINT count; };
	std::vector<BatchInfo> staticBatches;

	for (auto& matPair : renderBatches) {
		for (auto& meshPair : matPair.second) {
			staticBatches.push_back({ meshPair.first, matPair.first, static_cast<UINT>(allInstances.size()), static_cast<UINT>(meshPair.second.size()) });
			allInstances.insert(allInstances.end(), meshPair.second.begin(), meshPair.second.end());
		}
	}

	XMFLOAT3 dirLightDir = { 0.0f, -1.0f, 0.0f };
	float dirLightIntensity = 0.0f;
	XMFLOAT3 dirLightColor = { 1.0f, 1.0f, 1.0f };
	bool castShadows = false;

	XMMATRIX lightView = XMMatrixIdentity();
	XMMATRIX lightProj = XMMatrixOrthographicLH(150.0f, 150.0f, 1.0f, 300.0f);
	if (m_apiChoice == 2) lightProj = lightProj * XMMatrixScaling(1.0f, -1.0f, 1.0f);

	if (mainSun && sunTransform) {
		XMMATRIX rot = XMMatrixRotationRollPitchYaw(sunTransform->eulerAngles.x * (XM_PI / 180.0f), sunTransform->eulerAngles.y * (XM_PI / 180.0f), sunTransform->eulerAngles.z * (XM_PI / 180.0f));

		// forwardVektor je směr toku světla (od slunce dolů na mapu)
		XMVECTOR forwardVector = XMVector3TransformCoord(XMVectorSet(0, 0, 1, 0), rot);
		XMStoreFloat3(&dirLightDir, forwardVector);

		dirLightColor = { mainSun->color.x, mainSun->color.y, mainSun->color.z };
		dirLightIntensity = mainSun->intensity;
		castShadows = mainSun->castShadows;

		// Stínová kamera umístěná "ve slunci" a dívající se dolů
		XMFLOAT3 shadowPos = { -dirLightDir.x * 50.0f, -dirLightDir.y * 50.0f, -dirLightDir.z * 50.0f };
		lightView = XMMatrixLookToLH(XMLoadFloat3(&shadowPos), forwardVector, XMVectorSet(0, 1, 0, 0));
	}

	SM::Vector3 sunDirToSun = SM::Vector3(-dirLightDir.x, -dirLightDir.y, -dirLightDir.z);
	sunDirToSun.Normalize();

	// TOTO JE KLÍČOVÉ PRO ATMOSFÉRU A MRAKY: Potřebují vektor ukazující K SLUNCI!
		// 1. Získáme směr toku světla (dolů na zem)
	SM::Vector3 lightTravelDir(dirLightDir.x, dirLightDir.y, dirLightDir.z);
	lightTravelDir.Normalize();
	// 2. Vektor K SLUNCI (Zdola nahoru k obloze)
	//SM::Vector3 sunDirToSun1 = -lightTravelDir;

	GlobalData gData = {};
	gData.view = XMMatrixTranspose(view);
	gData.projection = XMMatrixTranspose(proj);
	gData.lightSpaceMatrix = XMMatrixTranspose(lightView * lightProj);
	gData.camPos = m_camera.pos;
	gData.hasIBL = (m_irradianceMap != nullptr) ? 1.0f : 0.0f;
	gData.dirLightDir = dirLightDir;
	gData.dirLightColor = dirLightColor;
	gData.dirLightIntensity = dirLightIntensity;
	gData.hasShadowMap = (m_shadowMap != nullptr && castShadows) ? 1 : 0;
	gData.enableSSAO = m_enableSSAO ? 1 : 0;
	gData.numPointLights = static_cast<int>(activePointLights.size());

	for (size_t i = 0; i < activePointLights.size(); i++) {
		gData.pointLights[i].position = { (float)pointLightTransforms[i]->position.x, (float)pointLightTransforms[i]->position.y, (float)pointLightTransforms[i]->position.z };
		gData.pointLights[i].radius = activePointLights[i]->radius;
		gData.pointLights[i].color = { activePointLights[i]->color.x, activePointLights[i]->color.y, activePointLights[i]->color.z };
		gData.pointLights[i].intensity = activePointLights[i]->intensity;
	}

	GlobalData shadowGlobal = {};
	shadowGlobal.view = XMMatrixTranspose(lightView);
	shadowGlobal.projection = XMMatrixTranspose(lightProj);

	m_rhi->BeginFrame();
	
	if (m_enableClouds && m_Clouds) {
		m_Clouds->GenerateNoise(m_rhi.get(), m_computeUniformBuffer.get());
	}

	// --- OPRAVA: ODESLÁNÍ DO ATMOSFÉRY ---
	if (m_enablePhysicallyBasedSky && m_atmosphere) {
		// TADY BYLA CHYBA! Musíme poslat sunDirToSun (ukazuje na oblohu), jinak se disk nakreslí pod zem!
		m_atmosphere->ComputeLUTs(m_rhi.get(), m_computeUniformBuffer.get(), sunDirToSun, { (float)m_camera.pos.x, (float)m_camera.pos.y, (float)m_camera.pos.z });
	}
	// Počítáme atmosféru pouze tehdy, je-li zapnutá
	
	if (m_simState == SimState::Playing) {
		for (const auto& obj : m_sceneManager.m_objects) {
			if (auto psc = obj->GetComponent<ParticleSystemComponent>()) psc->DispatchGPU(m_computeUniformBuffer.get());
		}
	}

	if (!allInstances.empty()) m_rhi->UpdateBuffer(m_instanceBuffer.get(), allInstances.data(), allInstances.size() * sizeof(ObjectData));

	// --- SHADOW PASS ---
	if (m_shadowMap && castShadows) {
		m_rhi->BeginShadowPass(m_shadowMap.get());
		m_rhi->SetPipeline(m_shadowPipeline.get());
		m_rhi->SetGlobalUniforms(m_globalBuffer.get(), &shadowGlobal, sizeof(GlobalData));

		for (const auto& batch : staticBatches) m_rhi->DrawIndexedInstanced(batch.mesh->vb.get(), batch.mesh->ib.get(), m_instanceBuffer.get(), batch.mesh->indexCount, batch.count, batch.offset);

		m_rhi->SetPipeline(m_skinnedShadowPipeline.get());
		m_rhi->SetGlobalUniforms(m_globalBuffer.get(), &shadowGlobal, sizeof(GlobalData));

		for (const auto& obj : m_sceneManager.m_objects) {
			if (auto smr = obj->GetComponent<SkinnedMeshRenderer>()) {
				if (!smr->mesh) continue; // Safety check

				if (auto anim = obj->GetComponent<Animator>()) m_rhi->SetBoneUniforms(m_boneBuffer.get(), anim->finalBoneMatrices.data(), sizeof(SM::Matrix) * MAX_BONES);
				for (size_t i = 0; i < smr->mesh->subMeshes.size(); i++) {
					const auto& subMesh = smr->mesh->subMeshes[i];
					if (!subMesh.vb) continue;
					SkinnedObjectData pData = smr->GetSubMeshData(i);
					m_rhi->SetObjectUniforms(m_skinnedObjectBuffer.get(), &pData, sizeof(SkinnedObjectData));
					m_rhi->DrawIndexed(subMesh.vb.get(), subMesh.ib.get(), subMesh.indexCount);
				}
			}
		}
	}

	// --- G-BUFFER PASS ---
	std::vector<RHITexture*> mrt = { m_rtColor.get(), m_rtNormal.get(), m_rtPos.get() };
	m_rhi->SetMRTTargets(mrt, nullptr);

	float clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	float clearZero[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	m_rhi->ClearRenderTarget(m_rtColor.get(), clearColor);
	m_rhi->ClearRenderTarget(m_rtNormal.get(), clearZero);
	m_rhi->ClearRenderTarget(m_rtPos.get(), clearZero);
	m_rhi->ClearDepthTarget(nullptr, 1.0f, 0);

	if (floorObject) {
		if (auto mr = floorObject->GetComponent<MeshRenderer>()) {
			if (mr->mesh && mr->material) {
				mr->material->BindTextures(m_rhi.get());
				m_rhi->SetGlobalUniforms(m_globalBuffer.get(), &gData, sizeof(GlobalData));
				if (m_shadowMap) m_rhi->SetTexture(m_shadowMap.get(), 4);
				if (m_irradianceMap) m_rhi->SetTexture(m_irradianceMap.get(), 5);
				if (m_prefilterMap) m_rhi->SetTexture(m_prefilterMap.get(), 6);
				if (m_brdfLut) m_rhi->SetTexture(m_brdfLut.get(), 7);

				ObjectData floorData = mr->GetInstanceData();
				floorData.model = XMMatrixTranspose(floorData.model);
				m_rhi->SetPushConstants(&floorData, sizeof(ObjectData));
				m_rhi->DrawIndexed(mr->mesh->vb.get(), mr->mesh->ib.get(), mr->mesh->indexCount);
			}
		}
	}

	for (const auto& batch : staticBatches) {
		batch.mat->BindTextures(m_rhi.get());
		m_rhi->SetGlobalUniforms(m_globalBuffer.get(), &gData, sizeof(GlobalData));
		if (m_shadowMap) m_rhi->SetTexture(m_shadowMap.get(), 4);
		if (m_irradianceMap) m_rhi->SetTexture(m_irradianceMap.get(), 5);
		if (m_prefilterMap) m_rhi->SetTexture(m_prefilterMap.get(), 6);
		if (m_brdfLut) m_rhi->SetTexture(m_brdfLut.get(), 7);
		m_rhi->DrawIndexedInstanced(batch.mesh->vb.get(), batch.mesh->ib.get(), m_instanceBuffer.get(), batch.mesh->indexCount, batch.count, batch.offset);
	}

	for (const auto& obj : m_sceneManager.m_objects) {
		if (auto smr = obj->GetComponent<SkinnedMeshRenderer>()) {
			if (!smr->mesh) continue; // Safety check

			bool hasBones = false;
			if (auto anim = obj->GetComponent<Animator>()) {
				m_rhi->SetBoneUniforms(m_boneBuffer.get(), anim->finalBoneMatrices.data(), sizeof(SM::Matrix) * MAX_BONES);
				hasBones = true;
			}

			m_rhi->SetGlobalUniforms(m_globalBuffer.get(), &gData, sizeof(GlobalData));
			if (m_shadowMap) m_rhi->SetTexture(m_shadowMap.get(), 4);
			if (m_irradianceMap) m_rhi->SetTexture(m_irradianceMap.get(), 5);
			if (m_prefilterMap) m_rhi->SetTexture(m_prefilterMap.get(), 6);
			if (m_brdfLut) m_rhi->SetTexture(m_brdfLut.get(), 7);

			Material* lastBoundMaterial = nullptr;
			for (size_t i = 0; i < smr->mesh->subMeshes.size(); i++) {
				const auto& subMesh = smr->mesh->subMeshes[i];
				if (!subMesh.vb) continue;

				// SAFE ACCESS TO MATERIALS
				Material* activeMat = subMesh.material.get();
				if (i < smr->materialOverrides.size() && smr->materialOverrides[i]) {
					activeMat = smr->materialOverrides[i].get();
				}

				if (activeMat && activeMat != lastBoundMaterial) {
					activeMat->BindTextures(m_rhi.get());
					if (m_apiChoice == 1) {
						m_rhi->SetGlobalUniforms(m_globalBuffer.get(), &gData, sizeof(GlobalData));
						if (m_shadowMap) m_rhi->SetTexture(m_shadowMap.get(), 4);
						if (m_irradianceMap) m_rhi->SetTexture(m_irradianceMap.get(), 5);
						if (m_prefilterMap) m_rhi->SetTexture(m_prefilterMap.get(), 6);
						if (m_brdfLut) m_rhi->SetTexture(m_brdfLut.get(), 7);
						if (hasBones) {
							auto anim = obj->GetComponent<Animator>();
							m_rhi->SetBoneUniforms(m_boneBuffer.get(), anim->finalBoneMatrices.data(), sizeof(SM::Matrix) * MAX_BONES);
						}
					}
					lastBoundMaterial = activeMat;
				}

				SkinnedObjectData pData = smr->GetSubMeshData(i);
				m_rhi->SetObjectUniforms(m_skinnedObjectBuffer.get(), &pData, sizeof(SkinnedObjectData));
				m_rhi->DrawIndexed(subMesh.vb.get(), subMesh.ib.get(), subMesh.indexCount);
			}
		}
	}

	if (m_enablePhysicallyBasedSky) {
		if (m_atmosphere) m_atmosphere->RenderSky(m_rhi.get(), m_globalBuffer.get(), gData);
	}
	else {
		if (m_skybox) m_skybox->Render(m_rhi.get(), m_globalBuffer.get(), gData);
	}


	for (const auto& obj : m_sceneManager.m_objects) {
		if (auto psc = obj->GetComponent<ParticleSystemComponent>()) psc->Render(m_globalBuffer.get(), gData);
	}

	if (m_debugPhysics) {
		m_lineVertices.clear();
		DebugDraw::Begin(m_lineVertices);
		JPH::BodyInterface& bi = m_physics->GetBodyInterface();

		for (const auto& obj : m_sceneManager.m_objects) {
			auto col = obj->GetComponent<Collider>();
			if (!col) continue;

			SM::Vector3 pos; SM::Quaternion rot; SM::Vector3 color;
			auto rb = obj->GetComponent<Rigidbody>();
			if (rb && !rb->bodyID.IsInvalid()) {
				JPH::RVec3 p; JPH::Quat q; bi.GetPositionAndRotation(rb->bodyID, p, q);
				pos = SM::Vector3(static_cast<float>(p.GetX() + g_physicsOrigin.x), static_cast<float>(p.GetY() + g_physicsOrigin.y), static_cast<float>(p.GetZ() + g_physicsOrigin.z));
				rot = SM::Quaternion(q.GetX(), q.GetY(), q.GetZ(), q.GetW());
				color = bi.IsActive(rb->bodyID) ? SM::Vector3(0.0f, 1.0f, 0.0f) : SM::Vector3(0.5f, 0.5f, 0.5f);
			}
			else {
				pos = SM::Vector3(static_cast<float>(obj->transform.position.x), static_cast<float>(obj->transform.position.y), static_cast<float>(obj->transform.position.z));
				rot = obj->transform.rotation;
				color = SM::Vector3(1.0f, 0.5f, 0.0f);
			}

			SM::Vector3 worldOffset = SM::Vector3::TransformNormal(col->center, SM::Matrix::CreateFromQuaternion(rot));
			SM::Vector3 finalPos = pos + worldOffset;

			if (auto box = dynamic_cast<BoxCollider*>(col)) DebugDraw::DrawBox(finalPos, rot, box->size / 2.0f, color);
			else if (auto sph = dynamic_cast<SphereCollider*>(col)) DebugDraw::DrawSphere(finalPos, rot, sph->radius, color);
			else if (auto cap = dynamic_cast<CapsuleCollider*>(col)) DebugDraw::DrawCapsule(finalPos, rot, cap->halfHeight, cap->radius, color);
			else if (auto mc = dynamic_cast<MeshCollider*>(col)) {
				// Added: Actual wireframe rendering from CPU data
				SM::Matrix modelMat = SM::Matrix::CreateScale(obj->transform.scale) * SM::Matrix::CreateFromQuaternion(rot) * SM::Matrix::CreateTranslation(finalPos);

				auto drawTriangle = [&](const SM::Vector3& a, const SM::Vector3& b, const SM::Vector3& c) {
					SM::Vector3 ta = SM::Vector3::Transform(a, modelMat);
					SM::Vector3 tb = SM::Vector3::Transform(b, modelMat);
					SM::Vector3 tc = SM::Vector3::Transform(c, modelMat);
					DebugDraw::DrawLine(ta, tb, color);
					DebugDraw::DrawLine(tb, tc, color);
					DebugDraw::DrawLine(tc, ta, color);
					};

				if (mc->collisionMesh && !mc->collisionMesh->vertices.empty()) {
					const auto& verts = mc->collisionMesh->vertices;
					const auto& inds = mc->collisionMesh->indices;
					for (size_t i = 0; i < inds.size(); i += 3) {
						drawTriangle(verts[inds[i]].pos, verts[inds[i + 1]].pos, verts[inds[i + 2]].pos);
					}
				}
				else if (mc->collisionSkelMesh) {
					for (const auto& sub : mc->collisionSkelMesh->subMeshes) {
						const auto& verts = sub.vertices;
						const auto& inds = sub.indices;
						for (size_t i = 0; i < inds.size(); i += 3) {
							SM::Vector3 v0 = { verts[inds[i]].pos.x, verts[inds[i]].pos.y, verts[inds[i]].pos.z };
							SM::Vector3 v1 = { verts[inds[i + 1]].pos.x, verts[inds[i + 1]].pos.y, verts[inds[i + 1]].pos.z };
							SM::Vector3 v2 = { verts[inds[i + 2]].pos.x, verts[inds[i + 2]].pos.y, verts[inds[i + 2]].pos.z };
							drawTriangle(v0, v1, v2);
						}
					}
				}
				else {
					// backup - bounding box
					DebugDraw::DrawBox(finalPos, rot, SM::Vector3(0.5f, 0.5f, 0.5f), color);
				}
			}
		}
		// draw SkeletalRagdollComponent
		for (const auto& obj : m_sceneManager.m_objects) {
			if (auto rag = obj->GetComponent<SkeletalRagdollComponent>()) {

				// 1. draw bones (yellow spheres - always, regardless of ragdoll state)
				if (rag->showDebugBones) {
					if (auto anim = obj->GetComponent<Animator>()) {
						if (anim->skelMesh) {
							SM::Matrix worldMat = SM::Matrix::CreateScale(obj->transform.scale) *
								SM::Matrix::CreateFromQuaternion(obj->transform.rotation) *
								SM::Matrix::CreateTranslation(static_cast<float>(obj->transform.position.x), static_cast<float>(obj->transform.position.y), static_cast<float>(obj->transform.position.z));

							for (const auto& [name, info] : anim->skelMesh->boneInfoMap) {
								// Inverse matrix gives us the exact initial position of the joint (rotation point) of the bone
								SM::Matrix boneLocal = info.offsetMatrix.Invert();
								SM::Vector3 boneWorldPos = SM::Vector3::Transform(boneLocal.Translation(), worldMat);

								// Draw yellow spheres of size 3 cm representing the skeleton
								DebugDraw::DrawSphere(boneWorldPos, SM::Quaternion::Identity, 0.03f, SM::Vector3(1.0f, 1.0f, 0.0f));
							}
						}
					}
				}

				// 2. draw physics (purple capsules - only when ragdoll is active)
				for (const auto& part : rag->m_mappedBones) {
					JPH::BodyID partID = part.bodyID;
					if (!partID.IsInvalid() && bi.IsAdded(partID)) {
						JPH::RVec3 p; JPH::Quat q; bi.GetPositionAndRotation(partID, p, q);
						SM::Vector3 pos(static_cast<float>(p.GetX()), static_cast<float>(p.GetY()), static_cast<float>(p.GetZ()));
						SM::Quaternion rot(q.GetX(), q.GetY(), q.GetZ(), q.GetW());

						JPH::RefConst<JPH::Shape> shape = bi.GetShape(partID);
						if (shape->GetSubType() == JPH::EShapeSubType::Capsule) {
							auto cap = static_cast<const JPH::CapsuleShape*>(shape.GetPtr());
							DebugDraw::DrawCapsule(pos, rot, cap->GetHalfHeightOfCylinder(), cap->GetRadius(), SM::Vector3(1.0f, 0.0f, 1.0f));
						}
						else if (shape->GetSubType() == JPH::EShapeSubType::Sphere) {
							auto sph = static_cast<const JPH::SphereShape*>(shape.GetPtr());
							DebugDraw::DrawSphere(pos, rot, sph->GetRadius(), SM::Vector3(1.0f, 0.0f, 1.0f));
						}
					}
				}
			}
		}
		if (!m_lineVertices.empty()) {
			m_rhi->UpdateBuffer(m_lineBuffer.get(), m_lineVertices.data(), m_lineVertices.size() * sizeof(Vertex));
			m_rhi->SetPipeline(m_linePipeline.get());
			m_rhi->SetGlobalUniforms(m_globalBuffer.get(), &gData, sizeof(GlobalData));
			m_rhi->Draw(m_lineBuffer.get(), static_cast<UINT>(m_lineVertices.size()));
		}
	}
	// --- 2. SSAO PASS ---
	if (m_enableSSAO) {
		m_rhi->SetMRTTargets({ m_rtSSAO.get() }, nullptr);
		m_rhi->ClearRenderTarget(m_rtSSAO.get(), clearColor);
		m_rhi->SetPipeline(m_ssaoPipeline.get());
		m_ssaoParams.viewProjection = XMMatrixTranspose(view * proj);
		m_ssaoParams.camPos = m_camera.pos;
		m_rhi->SetGlobalUniforms(m_globalBuffer.get(), &m_ssaoParams, sizeof(SSAOParams));
		m_rhi->SetTexture(m_rtPos.get(), 0);
		m_rhi->SetTexture(m_rtNormal.get(), 1);
		m_rhi->Draw(nullptr, 3);

		m_rhi->SetMRTTargets({ m_rtSSAOBlur.get() }, nullptr);
		m_rhi->ClearRenderTarget(m_rtSSAOBlur.get(), clearColor);
		m_rhi->SetPipeline(m_ssaoBlurPipeline.get());
		m_rhi->SetTexture(m_rtSSAO.get(), 0);
		m_rhi->Draw(nullptr, 3);
	}

	// --- 4. COMPOSITION & POST-PROCESSING CHAIN ---
	float screenClear[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	m_rhi->SetMRTTargets({ m_rtSceneFinal.get() }, nullptr);
	m_rhi->ClearRenderTarget(m_rtSceneFinal.get(), screenClear);

	if (m_enableSSR) m_rhi->SetPipeline(m_ssrPipeline.get());
	else m_rhi->SetPipeline(m_ssaoCombinePipeline.get());

	m_rhi->SetGlobalUniforms(m_globalBuffer.get(), &gData, sizeof(GlobalData));
	m_rhi->SetTexture(m_rtColor.get(), 0); m_rhi->SetTexture(m_rtNormal.get(), 1); m_rhi->SetTexture(m_rtPos.get(), 2);
	m_rhi->SetTexture(m_rtSSAOBlur.get(), 3);
	m_rhi->Draw(nullptr, 3);

	RHITexture* currentImage = m_rtSceneFinal.get();

	if (m_enableBloom) {
		m_rhi->SetMRTTargets({ m_rtPingPong.get() }, nullptr);
		m_rhi->ClearRenderTarget(m_rtPingPong.get(), screenClear);
		m_rhi->SetPipeline(m_bloomPipeline.get());
		m_rhi->SetTexture(currentImage, 0);
		m_rhi->Draw(nullptr, 3);
		currentImage = m_rtPingPong.get();
	}
	if (m_enableClouds && m_Clouds) {
		ZoneScopedNC("Draw Clouds", 0xFFFFFF);
		float time = (float)ImGui::GetTime();
		SM::Vector3 camPos = { (float)m_camera.pos.x, (float)m_camera.pos.y, (float)m_camera.pos.z };
		SM::Matrix smView(view); SM::Matrix smProj(proj);
		m_Clouds->Render(m_rhi.get(), m_computeUniformBuffer.get(), m_globalBuffer.get(), smView, smProj,
			m_camera.pos.x, m_camera.pos.y, m_camera.pos.z, // syrové DOUBLE!
			sunDirToSun, time, m_rtPos.get(), currentImage);
}
	m_rhi->SetMainPassTarget();
	m_rhi->ClearRenderTarget(m_rhi->GetBackBuffer(), screenClear);

	if (m_enableVignette) m_rhi->SetPipeline(m_vignettePipeline.get());
	else m_rhi->SetPipeline(m_copyPipeline.get());

	m_rhi->SetTexture(currentImage, 0);
	m_rhi->Draw(nullptr, 3);

	m_rhi->ImGuiBegin();
	ImGuizmo::BeginFrame();

	const ImGuiViewport* vp_imgui = ImGui::GetMainViewport();
	float screenW = vp_imgui->WorkSize.x;
	float screenH = vp_imgui->WorkSize.y;
	ImVec2 mPos = ImGui::GetIO().MousePos;

	if (ImGui::IsMouseClicked(0) && !ImGui::GetIO().WantCaptureMouse && !m_cameraActive && !ImGuizmo::IsOver()) {
		JPH::RRayCast ray = m_camera.GetMouseRay(mPos.x - vp_imgui->WorkPos.x, mPos.y - vp_imgui->WorkPos.y, screenW, screenH, g_physicsOrigin);
		JPH::RayCastResult hit;
		if (m_physics->GetNarrowPhaseQuery().CastRay(ray, hit)) {
			JPH::BodyLockRead lock(m_physics->GetBodyLockInterface(), hit.mBodyID);
			if (lock.Succeeded()) {
				m_sceneManager.m_selectedObject = reinterpret_cast<GameObject*>(lock.GetBody().GetUserData());
			}
		}
		else {
			m_sceneManager.m_selectedObject = nullptr;
		}
	}

	RenderEditorUI(vp_imgui, screenW, screenH, proj, view);

	m_rhi->ImGuiEnd();
	m_rhi->EndFrame();
}


void Engine::SpawnModelFromAsset(const std::string& path, float mouseX, float mouseY, float screenW, float screenH) {
	PipelineConfig skelCfg;
	skelCfg.vsPath = L"shaders/rhi/pbr-skinned.vert.hlsl";
	skelCfg.psPath = L"shaders/rhi/pbr-skinned.frag.hlsl";
	skelCfg.isSkinned = true;
	skelCfg.numRenderTargets = 3;

	try {
		auto newMesh = m_assets.LoadSkeletalMesh(path, skelCfg);
		std::string meshName = fs::path(path).filename().string();

		static int spawnCounter = 0;
		auto go = std::make_unique<GameObject>(meshName + "_" + std::to_string(spawnCounter++));

		if (mouseX >= 0.0f && mouseY >= 0.0f) {
			JPH::RRayCast ray = m_camera.GetMouseRay(mouseX, mouseY, screenW, screenH, g_physicsOrigin);
			JPH::RayCastResult hit;
			if (m_physics->GetNarrowPhaseQuery().CastRay(ray, hit)) {
				JPH::RVec3 hitPos = ray.mOrigin + ray.mDirection * hit.mFraction;
				go->transform.position = { hitPos.GetX() + g_physicsOrigin.x, hitPos.GetY() + g_physicsOrigin.y, hitPos.GetZ() + g_physicsOrigin.z };
			}
			else {
				XMMATRIX rot = XMMatrixRotationRollPitchYaw(m_camera.pitch, m_camera.yaw, 0);
				XMVECTOR forward = XMVector3TransformCoord(XMVectorSet(0, 0, 1, 0), rot);
				XMFLOAT3 fw; XMStoreFloat3(&fw, forward);
				go->transform.position = { m_camera.pos.x + fw.x * 10.0f, m_camera.pos.y + fw.y * 10.0f, m_camera.pos.z + fw.z * 10.0f };
			}
		}
		else {
			go->transform.position = { m_camera.pos.x, m_camera.pos.y, m_camera.pos.z + 5.0f };
		}

		go->transform.scale = { 0.05f, 0.05f, 0.05f };

		go->AddComponent<SkinnedMeshRenderer>(newMesh, meshName, path);
		auto animator = go->AddComponent<Animator>(newMesh);

		if (newMesh->scene && newMesh->scene->mNumAnimations > 0) {
			animator->Play(0, 0.0f, -1.0f, true);
		}

		JPH::BodyInterface& bi = m_physics->GetBodyInterface();
		go->AddComponent<BoxCollider>(SM::Vector3(15.0f, 15.0f, 15.0f));
		go->AddComponent<Rigidbody>(&bi, true);

		m_sceneManager.AddObject(std::move(go));
	}
	catch (...) {
		std::cerr << "[AssetBrowser] Failed to load model: " << path << "\n";
	}
}


void Engine::RenderAssetBrowser() {
	ImGui::Begin("Asset Browser");

	if (m_currentAssetDirectory.string() != "assets") {
		if (ImGui::Button("<- Back")) {
			m_currentAssetDirectory = m_currentAssetDirectory.parent_path();
		}
		ImGui::SameLine();
	}
	ImGui::Text("Current: %s", m_currentAssetDirectory.string().c_str());
	ImGui::Separator();

	float cellSize = 100.0f;
	float panelWidth = ImGui::GetContentRegionAvail().x;
	int columnCount = static_cast<int>(panelWidth / cellSize);
	if (columnCount < 1) columnCount = 1;

	ImGui::Columns(columnCount, 0, false);

	if (fs::exists(m_currentAssetDirectory)) {
		for (auto& directoryEntry : fs::directory_iterator(m_currentAssetDirectory)) {
			const auto& path = directoryEntry.path();
			std::string filenameString = path.filename().string();
			std::string extension = path.extension().string();

			ImGui::PushID(filenameString.c_str());

			if (directoryEntry.is_directory()) {
				// OPRAVENO: P�vodn� modr� ImVec4(0.2f, 0.3f, 0.4f, 1.0f) nahrazena modern� �istou tmav� �edou
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.18f, 0.18f, 1.0f));
				if (ImGui::Button(filenameString.c_str(), ImVec2(cellSize - 10, cellSize - 10))) {
					m_currentAssetDirectory /= path.filename();
				}
				ImGui::PopStyleColor();
			}
			else {
				ImGui::Button(filenameString.c_str(), ImVec2(cellSize - 10, cellSize - 10));

				if (extension == ".fbx" || extension == ".gltf") {
					if (ImGui::BeginDragDropSource()) {
						std::string itemPath = path.string();
						ImGui::SetDragDropPayload("CONTENT_BROWSER_ITEM", itemPath.c_str(), itemPath.size() + 1);
						ImGui::Text("Spawning: %s", filenameString.c_str());
						ImGui::EndDragDropSource();
					}
				}
			}

			ImGui::TextWrapped("%s", filenameString.c_str());
			ImGui::NextColumn();
			ImGui::PopID();
		}
	}
	else {
		ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Directory not found!");
	}

	ImGui::Columns(1);
	ImGui::End();
}


void Engine::RenderEditorUI(const ImGuiViewport* vp_imgui, float screenW, float screenH, XMMATRIX proj, XMMATRIX view) {
	ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoBackground;
	ImGui::SetNextWindowPos(vp_imgui->WorkPos);
	ImGui::SetNextWindowSize(vp_imgui->WorkSize);
	ImGui::SetNextWindowViewport(vp_imgui->ID);

	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));

	ImGui::Begin("MainDockSpace", nullptr, window_flags);
	ImGui::PopStyleColor();
	ImGui::PopStyleVar(2);

	ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");

	static bool first_time = true;
	if (first_time) {
		first_time = false;
		if (ImGui::DockBuilderGetNode(dockspace_id) == nullptr) {
			ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
			ImGui::DockBuilderSetNodeSize(dockspace_id, vp_imgui->WorkSize);

			ImGuiID dock_main_id = dockspace_id;
			ImGuiID dock_right_id = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Right, 0.25f, nullptr, &dock_main_id);
			ImGuiID dock_left_id = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Left, 0.20f, nullptr, &dock_main_id);
			ImGuiID dock_bottom_id = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Down, 0.30f, nullptr, &dock_main_id);
			ImGuiID dock_right_bottom_id = ImGui::DockBuilderSplitNode(dock_right_id, ImGuiDir_Down, 0.40f, nullptr, &dock_right_id);
			ImGuiID dock_left_bottom_id = ImGui::DockBuilderSplitNode(dock_left_id, ImGuiDir_Down, 0.30f, nullptr, &dock_left_id);
			ImGuiID dock_stats_id = ImGui::DockBuilderSplitNode(dock_left_bottom_id, ImGuiDir_Right, 0.5f, nullptr, &dock_left_bottom_id);

			ImGui::DockBuilderDockWindow("Hierarchy", dock_left_id);
			ImGui::DockBuilderDockWindow("Simulation", dock_left_bottom_id);
			ImGui::DockBuilderDockWindow("Stats", dock_stats_id);
			ImGui::DockBuilderDockWindow("Inspector", dock_right_id);
			ImGui::DockBuilderDockWindow("Post-Processing", dock_right_bottom_id);
			ImGui::DockBuilderDockWindow("Asset Browser", dock_bottom_id);

			ImGui::DockBuilderFinish(dockspace_id);
		}
	}
	ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);

	if (ImGui::IsMouseReleased(ImGuiMouseButton_Left) && !ImGui::GetIO().WantCaptureMouse) {
		if (const ImGuiPayload* payload = ImGui::GetDragDropPayload()) {
			if (payload->IsDataType("CONTENT_BROWSER_ITEM")) {
				const char* path = (const char*)payload->Data;
				ImVec2 mPos = ImGui::GetIO().MousePos;
				float localMouseX = mPos.x - vp_imgui->WorkPos.x;
				float localMouseY = mPos.y - vp_imgui->WorkPos.y;
				SpawnModelFromAsset(path, localMouseX, localMouseY, screenW, screenH);
			}
		}
	}

	RenderAssetBrowser();

	if (ImGui::BeginMenuBar()) {
		if (ImGui::BeginMenu("File")) {
			if (ImGui::MenuItem("New Scene")) m_sceneManager.ClearScene();
			ImGui::Separator();
			if (ImGui::MenuItem("Open Scene...")) m_loadDialog.Open();
			ImGui::Separator();
			if (ImGui::MenuItem("Save Scene")) {
				if (m_sceneManager.m_currentScenePath.empty()) m_saveDialog.Open();
				else m_sceneManager.SaveScene(m_sceneManager.m_currentScenePath);
			}
			if (ImGui::MenuItem("Save Scene As...")) m_saveDialog.Open();
			ImGui::Separator();
			if (ImGui::MenuItem("Exit")) PostQuitMessage(0);
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("GameObject")) {
			if (ImGui::MenuItem("Create Empty")) {
				static int newObjCounter = 1;
				auto go = std::make_unique<GameObject>("New GameObject_" + std::to_string(newObjCounter++));
				go->Start();
				m_sceneManager.m_selectedObject = go.get();
				m_sceneManager.AddObject(std::move(go));
			}

			ImGui::Separator();

			XMMATRIX camRot = XMMatrixRotationRollPitchYaw(m_camera.pitch, m_camera.yaw, 0);
			XMVECTOR forwardVec = XMVector3TransformCoord(XMVectorSet(0, 0, 1, 0), camRot);
			XMFLOAT3 fw; XMStoreFloat3(&fw, forwardVec);
			double spawnX = m_camera.pos.x + fw.x * 5.0f;
			double spawnY = m_camera.pos.y + fw.y * 5.0f;
			double spawnZ = m_camera.pos.z + fw.z * 5.0f;

			auto defaultMat = m_assets.m_allMaterials.count("Mat_Wall") ? m_assets.m_allMaterials["Mat_Wall"] : m_assets.m_allMaterials.begin()->second;

			if (ImGui::MenuItem("Create Cube")) {
				static int cubeCounter = 1;
				auto go = std::make_unique<GameObject>("Cube_" + std::to_string(cubeCounter++));
				go->transform.position = { spawnX, spawnY, spawnZ };
				go->AddComponent<MeshRenderer>(m_assets.m_meshes["Cube"], defaultMat, true, "Cube");
				go->AddComponent<BoxCollider>(SM::Vector3(1.0f, 1.0f, 1.0f));
				go->AddComponent<Rigidbody>(&m_physics->GetBodyInterface(), false);
				go->Start();
				m_sceneManager.m_selectedObject = go.get();
				m_sceneManager.AddObject(std::move(go));
			}
			if (ImGui::MenuItem("Create Sphere")) {
				static int sphereCounter = 1;
				auto go = std::make_unique<GameObject>("Sphere_" + std::to_string(sphereCounter++));
				go->transform.position = { spawnX, spawnY, spawnZ };
				go->AddComponent<MeshRenderer>(m_assets.m_meshes["Sphere"], defaultMat, true, "Sphere");
				go->AddComponent<SphereCollider>(0.5f);
				go->AddComponent<Rigidbody>(&m_physics->GetBodyInterface(), false);
				go->Start();
				m_sceneManager.m_selectedObject = go.get();
				m_sceneManager.AddObject(std::move(go));
			}
			if (ImGui::MenuItem("Create Capsule")) {
				static int capCounter = 1;
				auto go = std::make_unique<GameObject>("Capsule_" + std::to_string(capCounter++));
				go->transform.position = { spawnX, spawnY, spawnZ };
				go->AddComponent<MeshRenderer>(m_assets.m_meshes["Capsule"], defaultMat, true, "Capsule");
				go->AddComponent<CapsuleCollider>(0.5f, 0.5f);
				go->AddComponent<Rigidbody>(&m_physics->GetBodyInterface(), false);
				go->Start();
				m_sceneManager.m_selectedObject = go.get();
				m_sceneManager.AddObject(std::move(go));
			}
			if (ImGui::MenuItem("Create Plane")) {
				static int planeCounter = 1;
				auto go = std::make_unique<GameObject>("Plane_" + std::to_string(planeCounter++));
				go->transform.position = { spawnX, spawnY, spawnZ };
				go->AddComponent<MeshRenderer>(m_assets.m_meshes["Plane"], defaultMat, true, "Plane");
				go->AddComponent<BoxCollider>(SM::Vector3(5.0f, 0.05f, 5.0f));
				go->AddComponent<Rigidbody>(&m_physics->GetBodyInterface(), false);
				go->Start();
				m_sceneManager.m_selectedObject = go.get();
				m_sceneManager.AddObject(std::move(go));
			}
			ImGui::EndMenu();
		}
		ImGui::EndMenuBar();
	}
	ImGui::End();

	static ImGuizmo::OPERATION mCurrentGizmoOperation(ImGuizmo::TRANSLATE);
	static ImGuizmo::MODE mCurrentGizmoMode(ImGuizmo::WORLD);

	if (m_sceneManager.m_selectedObject && !m_cameraActive && m_sceneManager.m_selectedObject->name != "Floor") {
		ImGuizmo::SetOrthographic(false);
		ImGuizmo::SetDrawlist(ImGui::GetBackgroundDrawList());
		ImGuizmo::SetRect(vp_imgui->WorkPos.x, vp_imgui->WorkPos.y, vp_imgui->WorkSize.x, vp_imgui->WorkSize.y);

		if (ImGui::IsKeyPressed(ImGuiKey_T)) mCurrentGizmoOperation = ImGuizmo::TRANSLATE;
		if (ImGui::IsKeyPressed(ImGuiKey_R)) mCurrentGizmoOperation = ImGuizmo::ROTATE;
		if (ImGui::IsKeyPressed(ImGuiKey_S)) mCurrentGizmoOperation = ImGuizmo::SCALE;

		ImGui::SetNextWindowPos(ImVec2(vp_imgui->WorkPos.x + 20, vp_imgui->WorkPos.y + 40), ImGuiCond_FirstUseEver);
		ImGui::Begin("Gizmo Tools", nullptr, ImGuiWindowFlags_AlwaysAutoResize);

		if (ImGui::RadioButton("Translate (T)", mCurrentGizmoOperation == ImGuizmo::TRANSLATE)) mCurrentGizmoOperation = ImGuizmo::TRANSLATE;
		ImGui::SameLine();
		if (ImGui::RadioButton("Rotate (R)", mCurrentGizmoOperation == ImGuizmo::ROTATE)) mCurrentGizmoOperation = ImGuizmo::ROTATE;
		ImGui::SameLine();
		if (ImGui::RadioButton("Scale (S)", mCurrentGizmoOperation == ImGuizmo::SCALE)) mCurrentGizmoOperation = ImGuizmo::SCALE;

		ImGui::Separator();

		if (ImGui::RadioButton("Local", mCurrentGizmoMode == ImGuizmo::LOCAL)) mCurrentGizmoMode = ImGuizmo::LOCAL;
		ImGui::SameLine();
		if (ImGui::RadioButton("World", mCurrentGizmoMode == ImGuizmo::WORLD)) mCurrentGizmoMode = ImGuizmo::WORLD;

		ImGui::End();

		XMFLOAT4X4 viewF, projF;
		XMStoreFloat4x4(&viewF, view);
		XMStoreFloat4x4(&projF, proj);

		SM::Vector3 p((float)m_sceneManager.m_selectedObject->transform.position.x, (float)m_sceneManager.m_selectedObject->transform.position.y, (float)m_sceneManager.m_selectedObject->transform.position.z);
		SM::Matrix modelMatrix = SM::Matrix::CreateScale(m_sceneManager.m_selectedObject->transform.scale) * SM::Matrix::CreateFromQuaternion(m_sceneManager.m_selectedObject->transform.rotation) * SM::Matrix::CreateTranslation(p);

		XMFLOAT4X4 modelF;
		XMStoreFloat4x4(&modelF, modelMatrix);

		ImGuizmo::Manipulate(&viewF.m[0][0], &projF.m[0][0], mCurrentGizmoOperation, mCurrentGizmoMode, &modelF.m[0][0]);

		if (ImGuizmo::IsUsing()) {
			float matrixTranslation[3], matrixRotation[3], matrixScale[3];
			ImGuizmo::DecomposeMatrixToComponents(&modelF.m[0][0], matrixTranslation, matrixRotation, matrixScale);

			m_sceneManager.m_selectedObject->transform.position = { matrixTranslation[0], matrixTranslation[1], matrixTranslation[2] };
			m_sceneManager.m_selectedObject->transform.scale = { matrixScale[0], matrixScale[1], matrixScale[2] };
			m_sceneManager.m_selectedObject->transform.eulerAngles = { matrixRotation[0], matrixRotation[1], matrixRotation[2] };

			SM::Matrix rotMat(&modelF.m[0][0]);
			rotMat.Translation(SM::Vector3::Zero);

			SM::Vector3 right = rotMat.Right();   right.Normalize(); rotMat.Right(right);
			SM::Vector3 up = rotMat.Up();      up.Normalize();    rotMat.Up(up);
			SM::Vector3 fw = rotMat.Forward(); fw.Normalize();    rotMat.Forward(fw);

			m_sceneManager.m_selectedObject->transform.rotation = SM::Quaternion::CreateFromRotationMatrix(rotMat);
			m_sceneManager.m_selectedObject->transform.rotation.Normalize();

			if (m_simState == SimState::Stopped) {
				if (auto rb = m_sceneManager.m_selectedObject->GetComponent<Rigidbody>()) {
					if (!rb->bodyID.IsInvalid()) {
						rb->initialPos = m_sceneManager.m_selectedObject->transform.position;
						rb->initialRot = m_sceneManager.m_selectedObject->transform.rotation;
						Vector3d localPhysicsPos = rb->initialPos - g_physicsOrigin;

						rb->bodyInterface->SetPositionAndRotation(
							rb->bodyID,
							JPH::RVec3(static_cast<float>(localPhysicsPos.x), static_cast<float>(localPhysicsPos.y), static_cast<float>(localPhysicsPos.z)),
							JPH::Quat((float)rb->initialRot.x, (float)rb->initialRot.y, (float)rb->initialRot.z, (float)rb->initialRot.w),
							JPH::EActivation::DontActivate
						);
					}
				}
			}
		}
	}

	ImGui::Begin("Hierarchy");
	ImGui::BeginChild("HierarchyListDropZone", ImVec2(0, 0), false, ImGuiWindowFlags_NoBackground);
	for (const auto& obj : m_sceneManager.m_objects) {
		if (ImGui::Selectable(obj->name.c_str(), m_sceneManager.m_selectedObject == obj.get())) m_sceneManager.m_selectedObject = obj.get();
	}
	ImGui::EndChild();

	if (ImGui::BeginDragDropTarget()) {
		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
			const char* path = (const char*)payload->Data;
			SpawnModelFromAsset(path);
		}
		ImGui::EndDragDropTarget();
	}
	ImGui::End();

	ImGui::Begin("Stats");
	ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
	ImGui::Text("API: %s", m_apiChoice == 1 ? "DX12" : (m_apiChoice == 2 ? "Vulkan" : "DX11"));
	ImGui::Text("Camera: %s", m_cameraActive ? "ACTIVE" : "UI (TAB)");
	ImGui::SameLine();
	ImGui::Checkbox("Enable VSync", &m_rhi->m_vsync);
	ImGui::End();

	ImGui::Begin("Camera Settings");
	ImGui::SliderFloat("Speed", &m_camera.speed, 1.0f, 100.0f, "%.1f");
	ImGui::SliderFloat("Sensitivity", &m_camera.sensitivity, 0.001f, 0.050f, "%.4f");
	ImGui::End();

	ImGui::Begin("Simulation");
	if (ImGui::Button("Play")) m_simState = SimState::Playing;
	ImGui::SameLine();
	if (ImGui::Button("Pause")) m_simState = SimState::Paused;
	ImGui::SameLine();
	if (ImGui::Button("Stop")) {
		m_simState = SimState::Stopped;
		m_sceneManager.ResetScene();
	}
	ImGui::Separator();
	ImGui::Checkbox("Show Physics Colliders", &m_debugPhysics);
	ImGui::End();

	ImGui::Begin("Post-Processing");

	ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Environment");
	ImGui::Checkbox("Physically Based Sky", &m_enablePhysicallyBasedSky);

	ImGui::Separator();
	ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "Ambient Occlusion");
	ImGui::Checkbox("Enable SSAO", &m_enableSSAO);
	if (m_enableSSAO) {
		ImGui::SliderFloat("Radius", &m_ssaoParams.radius, 0.1f, 5.0f);
		ImGui::SliderFloat("Bias", &m_ssaoParams.bias, 0.0f, 0.5f);
	}
	ImGui::Separator();
	ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "Screen Space Effects");
	ImGui::Checkbox("SSR (Reflections)", &m_enableSSR);
	ImGui::Checkbox("Bloom (Glow)", &m_enableBloom);
	ImGui::Checkbox("Vignette", &m_enableVignette);
	ImGui::Checkbox("Enable Volumetric Clouds", &m_enableClouds);
	ImGui::End();

	if (m_enablePhysicallyBasedSky && m_atmosphere) {
		m_atmosphere->DrawDebug();
	}
	if (m_enableClouds && m_Clouds) {
		m_Clouds->DrawDebug();
	}

	ImGui::Begin("Inspector");
	if (m_sceneManager.m_selectedObject) {
		char nameBuf[128];
		strcpy_s(nameBuf, m_sceneManager.m_selectedObject->name.c_str());
		if (ImGui::InputText("Name", nameBuf, IM_ARRAYSIZE(nameBuf))) {
			m_sceneManager.m_selectedObject->name = nameBuf;
		}
		ImGui::SameLine();
		if (ImGui::Button("Delete Object", ImVec2(-1, 0))) m_sceneManager.m_selectedObject->Destroy();

		ImGui::Separator();
		ImGui::Text("Transform");
		bool tChanged = false;
		if (ImGui::DragScalarN("Position", ImGuiDataType_Double, &m_sceneManager.m_selectedObject->transform.position.x, 3, 0.1f)) tChanged = true;
		if (ImGui::DragFloat3("Rotation", &m_sceneManager.m_selectedObject->transform.eulerAngles.x, 1.0f)) {
			m_sceneManager.m_selectedObject->transform.UpdateRotation();
			tChanged = true;
		}
		if (ImGui::DragFloat3("Scale", &m_sceneManager.m_selectedObject->transform.scale.x, 0.05f)) tChanged = true;

		if (tChanged && m_simState == SimState::Stopped) {
			if (auto rb = m_sceneManager.m_selectedObject->GetComponent<Rigidbody>()) {
				if (!rb->bodyID.IsInvalid()) {
					rb->initialPos = m_sceneManager.m_selectedObject->transform.position;
					rb->initialRot = m_sceneManager.m_selectedObject->transform.rotation;
					Vector3d localPhysicsPos = rb->initialPos - g_physicsOrigin;
					rb->bodyInterface->SetPositionAndRotation(rb->bodyID, JPH::RVec3(static_cast<float>(localPhysicsPos.x), static_cast<float>(localPhysicsPos.y), static_cast<float>(localPhysicsPos.z)), JPH::Quat(rb->initialRot.x, rb->initialRot.y, rb->initialRot.z, rb->initialRot.w), JPH::EActivation::DontActivate);
				}
			}
		}

		for (auto& comp : m_sceneManager.m_selectedObject->components) {
			if (!comp->isPendingDestroy) {
				ImGui::PushID(comp.get());
				comp->OnGUI();
				ImGui::PopID();
			}
		}

		if (auto mr = m_sceneManager.m_selectedObject->GetComponent<MeshRenderer>()) {
			ImGui::Separator(); ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.3f, 1.0f), "Mesh Renderer"); ImGui::SameLine();
			if (ImGui::Button("Remove##mr")) mr->Destroy();

			if (ImGui::BeginCombo("Mesh", mr->meshName.c_str())) {
				for (auto const& [name, meshAsset] : m_assets.m_meshes) {
					bool isSelected = (mr->meshName == name);
					if (ImGui::Selectable(name.c_str(), isSelected)) {
						mr->mesh = meshAsset;
						mr->meshName = name;
					}
					if (isSelected) ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}

			if (ImGui::BeginCombo("Material", mr->material ? mr->material->name.c_str() : "Select...")) {
				for (auto const& [name, matAsset] : m_assets.m_allMaterials) {
					bool isSelected = (mr->material == matAsset);
					if (ImGui::Selectable(name.c_str(), isSelected)) {
						mr->material = matAsset;
						mr->matName = name;
					}
					if (isSelected) ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}

			if (mr->material) {
				ImGui::ColorEdit4("Base Color", &mr->material->baseColor.x);
				ImGui::SliderFloat("Roughness", &mr->material->roughness, 0.0f, 1.0f);
				ImGui::SliderFloat("Metalness", &mr->material->metalness, 0.0f, 1.0f);
			}
		}

		if (auto smr = m_sceneManager.m_selectedObject->GetComponent<SkinnedMeshRenderer>()) {
			ImGui::Separator(); ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.3f, 1.0f), "Skinned Mesh Renderer"); ImGui::SameLine();
			if (ImGui::Button("Remove##smr")) smr->Destroy();

			if (ImGui::BeginCombo("Skinned Mesh", smr->meshName.c_str())) {
				for (auto const& [name, skelAsset] : m_assets.m_skelMeshes) {
					bool isSelected = (smr->meshName == name);
					if (ImGui::Selectable(name.c_str(), isSelected)) {
						smr->mesh = skelAsset;
						smr->meshName = name;
						if (auto anim = m_sceneManager.m_selectedObject->GetComponent<Animator>()) {
							anim->skelMesh = skelAsset;
							if (anim->skelMesh->scene && anim->skelMesh->scene->mNumAnimations > 0) anim->Play(0, 0.0f, -1.0f, true);
						}
					}
					if (isSelected) ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}

			if (smr->mesh) {
				ImGui::Text("SubMeshes: %d", static_cast<int>(smr->mesh->subMeshes.size()));
				if (ImGui::TreeNode("Materials (SubMeshes)")) {
					for (size_t i = 0; i < smr->mesh->subMeshes.size(); i++) {
						auto& subMesh = smr->mesh->subMeshes[i];
						if (ImGui::TreeNode(reinterpret_cast<void*>(i), "[%d] %s", static_cast<int>(i), subMesh.name.c_str())) {
							std::string previewName = "Default FBX Material";
							if (smr->materialOverrides.size() > i && smr->materialOverrides[i] != nullptr) previewName = smr->materialOverrides[i]->name;

							if (ImGui::BeginCombo("Material Override", previewName.c_str())) {
								bool isDefaultSelected = (smr->materialOverrides.size() > i && smr->materialOverrides[i] == nullptr);
								if (ImGui::Selectable("Default FBX Material", isDefaultSelected)) smr->SetMaterial(i, nullptr);
								if (isDefaultSelected) ImGui::SetItemDefaultFocus();

								for (auto const& [name, matAsset] : m_assets.m_allMaterials) {
									bool isSelected = (smr->materialOverrides.size() > i && smr->materialOverrides[i] == matAsset);
									if (ImGui::Selectable(name.c_str(), isSelected)) smr->SetMaterial(i, matAsset);
									if (isSelected) ImGui::SetItemDefaultFocus();
								}
								ImGui::EndCombo();
							}

							auto activeMat = (smr->materialOverrides.size() > i && smr->materialOverrides[i]) ? smr->materialOverrides[i] : subMesh.material;
							if (activeMat) {
								ImGui::ColorEdit4("Base Color", &activeMat->baseColor.x);
								ImGui::SliderFloat("Roughness", &activeMat->roughness, 0.0f, 1.0f);
								ImGui::SliderFloat("Metalness", &activeMat->metalness, 0.0f, 1.0f);
							}
							ImGui::TreePop();
						}
					}
					ImGui::TreePop();
				}
			}
		}

		if (auto anim = m_sceneManager.m_selectedObject->GetComponent<Animator>()) {
			ImGui::Separator(); ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.7f, 1.0f), "Animator"); ImGui::SameLine();
			if (ImGui::Button("Remove##anim")) anim->Destroy();

			int numAnims = (anim->skelMesh && anim->skelMesh->scene) ? anim->skelMesh->scene->mNumAnimations : 0;
			if (numAnims > 0) {
				ImGui::Text("Available animations: %d", numAnims);
				int editAnimIndex = anim->currentAnimIndex;
				if (ImGui::SliderInt("Anim Index", &editAnimIndex, 0, numAnims - 1)) {
					anim->Play(editAnimIndex, 0.0f, -1.0f, anim->loopAnim);
				}
				float maxDuration = static_cast<float>(anim->skelMesh->scene->mAnimations[anim->currentAnimIndex]->mDuration);
				ImGui::DragFloat("Start Tick", &anim->startTick, 1.0f, 0.0f, anim->endTick);
				ImGui::DragFloat("End Tick", &anim->endTick, 1.0f, anim->startTick, maxDuration);
			}
			else {
				ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Model has no animation.");
			}

			ImGui::Checkbox("Play Animation", &anim->isPlaying); ImGui::SameLine(); ImGui::Checkbox("Loop", &anim->loopAnim);
			ImGui::SliderFloat("Timeline", &anim->currentTime, anim->startTick, anim->endTick);
		}

		ImGui::Separator();
		if (ImGui::Button("Add Component...", ImVec2(-1, 30))) {
			ImGui::OpenPopup("AddComponentPopup");
		}

		if (ImGui::BeginPopup("AddComponentPopup")) {
			if (!m_sceneManager.m_selectedObject->GetComponent<BoxCollider>() && ImGui::MenuItem("Box Collider")) {
				m_sceneManager.m_selectedObject->AddComponent<BoxCollider>(SM::Vector3(1, 1, 1));
				if (auto rb = m_sceneManager.m_selectedObject->GetComponent<Rigidbody>()) rb->RecreateShape();
			}
			if (!m_sceneManager.m_selectedObject->GetComponent<SphereCollider>() && ImGui::MenuItem("Sphere Collider")) {
				m_sceneManager.m_selectedObject->AddComponent<SphereCollider>(0.5f);
				if (auto rb = m_sceneManager.m_selectedObject->GetComponent<Rigidbody>()) rb->RecreateShape();
			}
			if (!m_sceneManager.m_selectedObject->GetComponent<CapsuleCollider>() && ImGui::MenuItem("Capsule Collider")) {
				m_sceneManager.m_selectedObject->AddComponent<CapsuleCollider>(0.5f, 0.5f);
				if (auto rb = m_sceneManager.m_selectedObject->GetComponent<Rigidbody>()) rb->RecreateShape();
			}
			if (!m_sceneManager.m_selectedObject->GetComponent<MeshCollider>() && ImGui::MenuItem("Mesh Collider")) {
				auto mr = m_sceneManager.m_selectedObject->GetComponent<MeshRenderer>();
				auto smr = m_sceneManager.m_selectedObject->GetComponent<SkinnedMeshRenderer>();
				std::shared_ptr<Mesh> mPtr = mr ? mr->mesh : nullptr;
				std::shared_ptr<SkeletalMesh> smPtr = smr ? smr->mesh : nullptr;
				std::string mName = mr ? mr->meshName : (smr ? smr->meshName : "");
				m_sceneManager.m_selectedObject->AddComponent<MeshCollider>(mPtr, smPtr, mName, false);
				if (auto rb = m_sceneManager.m_selectedObject->GetComponent<Rigidbody>()) rb->RecreateShape();
			}

			if (!m_sceneManager.m_selectedObject->GetComponent<Rigidbody>() && ImGui::MenuItem("Rigidbody")) {
				JPH::BodyInterface& bi = m_physics->GetBodyInterface();
				m_sceneManager.m_selectedObject->AddComponent<Rigidbody>(&bi, true);
			}

			if (!m_sceneManager.m_selectedObject->GetComponent<MeshRenderer>() && ImGui::MenuItem("Mesh Renderer")) {
				if (!m_assets.m_meshes.empty() && !m_assets.m_allMaterials.empty())
					m_sceneManager.m_selectedObject->AddComponent<MeshRenderer>(m_assets.m_meshes.begin()->second, m_assets.m_allMaterials.begin()->second, true, m_assets.m_meshes.begin()->first);
			}

			if (!m_sceneManager.m_selectedObject->GetComponent<SkinnedMeshRenderer>() && ImGui::MenuItem("Skinned Mesh Renderer")) {
				if (!m_assets.m_skelMeshes.empty()) {
					m_sceneManager.m_selectedObject->AddComponent<SkinnedMeshRenderer>(m_assets.m_skelMeshes.begin()->second, m_assets.m_skelMeshes.begin()->first);
					auto anim = m_sceneManager.m_selectedObject->AddComponent<Animator>(m_assets.m_skelMeshes.begin()->second);
					if (anim->skelMesh->scene && anim->skelMesh->scene->mNumAnimations > 0) anim->Play(0, 0.0f, -1.0f, true);
				}
			}

			if (!m_sceneManager.m_selectedObject->GetComponent<Animator>() && ImGui::MenuItem("Animator")) {
				auto smr = m_sceneManager.m_selectedObject->GetComponent<SkinnedMeshRenderer>();
				auto anim = m_sceneManager.m_selectedObject->AddComponent<Animator>(smr ? smr->mesh : nullptr);
				if (anim->skelMesh && anim->skelMesh->scene && anim->skelMesh->scene->mNumAnimations > 0) anim->Play(0, 0.0f, -1.0f, true);
			}

			if (!m_sceneManager.m_selectedObject->GetComponent<DirectionalLight>() && ImGui::MenuItem("Directional Light")) m_sceneManager.m_selectedObject->AddComponent<DirectionalLight>();
			if (!m_sceneManager.m_selectedObject->GetComponent<PointLight>() && ImGui::MenuItem("Point Light")) m_sceneManager.m_selectedObject->AddComponent<PointLight>();
			if (!m_sceneManager.m_selectedObject->GetComponent<ParticleSystemComponent>() && ImGui::MenuItem("Particle System")) m_sceneManager.m_selectedObject->AddComponent<ParticleSystemComponent>(m_rhi.get(), L"");
			if (!m_sceneManager.m_selectedObject->GetComponent<PlayerController>() && ImGui::MenuItem("Player Controller")) m_sceneManager.m_selectedObject->AddComponent<PlayerController>();
			if (!m_sceneManager.m_selectedObject->GetComponent<RotatingObstacle>() && ImGui::MenuItem("Rotating Obstacle")) m_sceneManager.m_selectedObject->AddComponent<RotatingObstacle>();
			if (!m_sceneManager.m_selectedObject->GetComponent<BouncingJumper>() && ImGui::MenuItem("Bouncing Jumper")) m_sceneManager.m_selectedObject->AddComponent<BouncingJumper>();
			if (!m_sceneManager.m_selectedObject->GetComponent<PlayerJumper>() && ImGui::MenuItem("Player Jumper")) m_sceneManager.m_selectedObject->AddComponent<PlayerJumper>();
			if (!m_sceneManager.m_selectedObject->GetComponent<AudioSource>() && ImGui::MenuItem("Audio Source")) m_sceneManager.m_selectedObject->AddComponent<AudioSource>(&m_audioEngine, "");

			ImGui::EndPopup();
		}
	}
	else {
		ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "No object selected.");
	}
	ImGui::End();

	if (m_sceneManager.m_selectedObject && m_sceneManager.m_selectedObject->name != "Floor" && !m_cameraActive && !ImGuizmo::IsUsing()) {
		XMVECTOR worldPos = XMVectorSet(static_cast<float>(m_sceneManager.m_selectedObject->transform.position.x), static_cast<float>(m_sceneManager.m_selectedObject->transform.position.y) + 1.5f, static_cast<float>(m_sceneManager.m_selectedObject->transform.position.z), 1.0f);
		XMVECTOR screenPos = XMVector3Project(worldPos, 0, 0, screenW, screenH, 0.0f, 1.0f, proj, view, XMMatrixIdentity());

		if (XMVectorGetZ(screenPos) < 1.0f && XMVectorGetZ(screenPos) > 0.0f) {
			ImVec2 finalPos = ImVec2(vp_imgui->WorkPos.x + XMVectorGetX(screenPos), vp_imgui->WorkPos.y + XMVectorGetY(screenPos));

			ImGui::SetNextWindowPos(finalPos, ImGuiCond_Always, ImVec2(0.5f, 1.0f));
			ImGui::SetNextWindowBgAlpha(0.65f);
			ImGui::Begin("EntityOverlay", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoFocusOnAppearing);
			ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "%s", m_sceneManager.m_selectedObject->name.c_str());
			ImGui::End();
		}
	}

	m_loadDialog.Display();
	if (m_loadDialog.HasSelected()) {
		std::string path = m_loadDialog.GetSelected().string();
		m_sceneManager.LoadScene(path);
		m_loadDialog.ClearSelected();
	}

	m_saveDialog.Display();
	if (m_saveDialog.HasSelected()) {
		std::string path = m_saveDialog.GetSelected().string();
		if (path.find(".scene") == std::string::npos) path += ".scene";
		m_sceneManager.SaveScene(path);
		m_saveDialog.ClearSelected();
	}
}


void Engine::OnQuit() {
	m_rhi->ImGuiCleanup();
	m_sceneManager.ClearScene();
	m_assets.Clear();
	m_atmosphere.reset();
	m_debugRenderer.reset();
	m_physics.reset();
	m_bpLayerInterface.reset();
	m_objVsBpFilter.reset();
	m_objPairFilter.reset();
	m_jobSystem.reset();
	m_tempAllocator.reset();
	m_contactListener.reset();
	ma_engine_uninit(&m_audioEngine);
	JPH::UnregisterTypes();
	delete JPH::Factory::sInstance;
	JPH::Factory::sInstance = nullptr;
}
