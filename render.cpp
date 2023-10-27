#include "stdafx.h"

namespace Render {
	BOOLEAN showMenu = FALSE;

	ID3D11Device *device = nullptr;
	ID3D11DeviceContext *immediateContext = nullptr;
	ID3D11RenderTargetView *renderTargetView = nullptr;

	WNDPROC WndProcOriginal = nullptr;
	HRESULT(*PresentOriginal)(IDXGISwapChain *swapChain, UINT syncInterval, UINT flags) = nullptr;
	HRESULT(*ResizeOriginal)(IDXGISwapChain *swapChain, UINT bufferCount, UINT width, UINT height, DXGI_FORMAT newFormat, UINT swapChainFlags) = nullptr;

	ImGuiWindow &BeginScene() {
		ImGui_ImplDX11_NewFrame();

		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
		ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
		ImGui::Begin("##scene", nullptr, ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoTitleBar);

		auto &io = ImGui::GetIO();
		ImGui::SetWindowPos(ImVec2(0, 0), ImGuiCond_Always);
		ImGui::SetWindowSize(ImVec2(io.DisplaySize.x, io.DisplaySize.y), ImGuiCond_Always);

		return *ImGui::GetCurrentWindow();
	}

	VOID EndScene(ImGuiWindow &window) {
		window.DrawList->PushClipRectFullScreen();
		ImGui::End();
		ImGui::PopStyleColor();
		ImGui::PopStyleVar(2);

		ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.17f, 0.18f, 0.2f, 1.0f));

		if (showMenu) {
			ImGui::Begin(xorstr("##menu"));
			ImGui::SetWindowSize(ImVec2(500, 600), ImGuiCond_FirstUseEver);
			ImGui::Text("Redboy- FN");


			if (ImGui::CollapsingHeader("Upgrades:"))
			{
				ImGui::Text("AIM KEY: RT MOUSE");
				ImGui::Checkbox(xorstr("memory-aimbot"), &Settings.Aimbot);
				ImGui::Checkbox(xorstr("silent-aim"), &Settings.SilentAimbot);
				if (!Settings.AutoAimbot) {
					ImGui::Checkbox(xorstr("aim FOV##checkbox"), &Settings.ESP.AimbotFOV);
				}
				if (!Settings.AutoAimbot) {
					ImGui::SliderFloat(xorstr("aim-fov##slider"), &Settings.AimbotFOV, 0.0f, 1000.0f, xorstr("%.2f"));
					ImGui::SliderFloat(xorstr("aim-smoothing"), &Settings.AimbotSlow, 0.0f, 25.0f, xorstr("%.2f"));
				}
				ImGui::Text("use at risk [!]");
				if (Settings.Aimbot) {
					ImGui::Checkbox(xorstr("rage-setting"), &Settings.AutoAimbot);
					ImGui::Checkbox(xorstr("no-spread"), &Settings.NoSpreadAimbot);	
					ImGui::Checkbox(xorstr("Instant Reload"), &Settings.InstantReload);
				}
			}		

			if (ImGui::CollapsingHeader("Visuals:"))
			{
				ImGui::Text(xorstr("ESP:"));
				ImGui::Checkbox(xorstr("bone/box"), &Settings.ESP.Players);
				if (Settings.ESP.Players) {
					ImGui::Checkbox(xorstr("snap-lines"), &Settings.ESP.PlayerLines);
					ImGui::Checkbox(xorstr("name"), &Settings.ESP.PlayerNames);

					ImGui::PushItemWidth(150.0f);
					ImGui::ColorPicker3(xorstr("if-visible"), Settings.ESP.PlayerVisibleColor);
					ImGui::ColorPicker3(xorstr("not-visible"), Settings.ESP.PlayerNotVisibleColor);
					ImGui::PopItemWidth();
				}
				ImGui::SliderFloat(xorstr("FOV slider:"), &Settings.FOV, 60.0f, 160.0f, xorstr("%.2f"));
				ImGui::Text("Loot ESP:");
				ImGui::Checkbox(xorstr("ammo"), &Settings.ESP.Ammo);
				ImGui::Checkbox(xorstr("containers"), &Settings.ESP.Containers);
				ImGui::Checkbox(xorstr("weapons"), &Settings.ESP.Weapons);
				if (Settings.ESP.Weapons) {
					ImGui::SliderInt(xorstr("min weapon rariety"), &Settings.ESP.MinWeaponTier, 0, 8);
				}						
			}
			ImGui::End();
		}

		ImGui::PopStyleColor();

		ImGui::Render();
	}

	VOID AddLine(ImGuiWindow &window, float width, float height, float a[3], float b[3], ImU32 color, float &minX, float &maxX, float &minY, float &maxY) {
		float ac[3] = { a[0], a[1], a[2] };
		float bc[3] = { b[0], b[1], b[2] };
		if (Util::WorldToScreen(width, height, ac) && Util::WorldToScreen(width, height, bc)) {
			window.DrawList->AddLine(ImVec2(ac[0], ac[1]), ImVec2(bc[0], bc[1]), color, 2.0f);

			minX = min(ac[0], minX);
			minX = min(bc[0], minX);

			maxX = max(ac[0], maxX);
			maxX = max(bc[0], maxX);

			minY = min(ac[1], minY);
			minY = min(bc[1], minY);

			maxY = max(ac[1], maxY);
			maxY = max(bc[1], maxY);
		}
	}

	VOID AddMarker(ImGuiWindow &window, float width, float height, float *start, PVOID pawn, LPCSTR text, ImU32 color) {
		auto root = Util::GetPawnRootLocation(pawn);
		if (root) {
			auto pos = *root;
			float dx = start[0] - pos.X;
			float dy = start[1] - pos.Y;
			float dz = start[2] - pos.Z;

			if (Util::WorldToScreen(width, height, &pos.X)) {
				float dist = Util::SpoofCall(sqrtf, dx * dx + dy * dy + dz * dz) / 1000.0f;

				CHAR modified[0xFF] = { 0 };
				snprintf(modified, sizeof(modified), xorstr("%s\n[%dm]"), text, static_cast<INT>(dist));

				auto size = ImGui::GetFont()->CalcTextSizeA(window.DrawList->_Data->FontSize, FLT_MAX, 0, modified);
				window.DrawList->AddText(ImVec2(pos.X - size.x / 2.0f, pos.Y - size.y / 2.0f), color, modified);
			}
		}
	}

	__declspec(dllexport) LRESULT CALLBACK WndProcHook(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
		if (msg == WM_KEYUP && (wParam == VK_INSERT || (showMenu && wParam == VK_ESCAPE))) {
			showMenu = !showMenu;
			ImGui::GetIO().MouseDrawCursor = showMenu;

			if (!showMenu) {
				SettingsHelper::SaveSettings();
			}
		} else if (msg == WM_QUIT && showMenu) {
			SettingsHelper::SaveSettings();
			ExitProcess(0);
		}

		if (showMenu) {
			ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam);
			return TRUE;
		}

		return CallWindowProc(WndProcOriginal, hWnd, msg, wParam, lParam);
	}

	__declspec(dllexport) HRESULT PresentHook(IDXGISwapChain *swapChain, UINT syncInterval, UINT flags) {
		static float width = 0;
		static float height = 0;
		static HWND hWnd = 0;

		if (!device) {
			swapChain->GetDevice(__uuidof(device), reinterpret_cast<PVOID *>(&device));
			device->GetImmediateContext(&immediateContext);

			ID3D11Texture2D *renderTarget = nullptr;
			swapChain->GetBuffer(0, __uuidof(renderTarget), reinterpret_cast<PVOID *>(&renderTarget));
			device->CreateRenderTargetView(renderTarget, nullptr, &renderTargetView);
			renderTarget->Release();

			ID3D11Texture2D *backBuffer = 0;
			swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (PVOID *)&backBuffer);
			D3D11_TEXTURE2D_DESC backBufferDesc = { 0 };
			backBuffer->GetDesc(&backBufferDesc);

			hWnd = FindWindow(xorstr(L"UnrealWindow"), xorstr(L"Fortnite  "));
			if (!width) {
				WndProcOriginal = reinterpret_cast<WNDPROC>(SetWindowLongPtr(hWnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(WndProcHook)));
			}

			width = (float)backBufferDesc.Width;
			height = (float)backBufferDesc.Height;
			backBuffer->Release();

			ImGui::GetIO().Fonts->AddFontFromFileTTF(xorstr("C:\\Windows\\Fonts\\verdana.ttf"), 12.0f);

			ImGui_ImplDX11_Init(hWnd, device, immediateContext);
			ImGui_ImplDX11_CreateDeviceObjects();
		}

		immediateContext->OMSetRenderTargets(1, &renderTargetView, nullptr);

		auto &window = BeginScene();

		auto success = FALSE;
		do {
			float closestDistance = FLT_MAX;
			PVOID closestPawn = NULL;

			auto world = *Offsets::uWorld;
			if (!world) break;

			auto gameInstance = ReadPointer(world, Offsets::Engine::World::OwningGameInstance);
			if (!gameInstance) break;

			auto localPlayers = ReadPointer(gameInstance, Offsets::Engine::GameInstance::LocalPlayers);
			if (!localPlayers) break;

			auto localPlayer = ReadPointer(localPlayers, 0);
			if (!localPlayer) break;

			auto localPlayerController = ReadPointer(localPlayer, Offsets::Engine::Player::PlayerController);
			if (!localPlayerController) break;

			auto localPlayerPawn = reinterpret_cast<UObject *>(ReadPointer(localPlayerController, Offsets::Engine::PlayerController::AcknowledgedPawn));
			if (!localPlayerPawn) break;

			auto localPlayerWeapon = ReadPointer(localPlayerPawn, Offsets::FortniteGame::FortPawn::CurrentWeapon);
			if (!localPlayerWeapon) break;

			auto localPlayerRoot = ReadPointer(localPlayerPawn, Offsets::Engine::Actor::RootComponent);
			if (!localPlayerRoot) break;

			auto localPlayerState = ReadPointer(localPlayerPawn, Offsets::Engine::Pawn::PlayerState);
			if (!localPlayerState) break;

			auto localPlayerLocation = reinterpret_cast<float *>(reinterpret_cast<PBYTE>(localPlayerRoot) + Offsets::Engine::SceneComponent::RelativeLocation);
			auto localPlayerTeamIndex = ReadDWORD(localPlayerState, Offsets::FortniteGame::FortPlayerStateAthena::TeamIndex);

			auto weaponName = Util::GetObjectFirstName((UObject *)localPlayerWeapon);
			auto isProjectileWeapon = wcsstr(weaponName.c_str(), L"Rifle_Sniper");

			Core::LocalPlayerPawn = localPlayerPawn;
			Core::LocalPlayerController = localPlayerController;

			std::vector<PVOID> playerPawns;
			for (auto li = 0UL; li < ReadDWORD(world, Offsets::Engine::World::Levels + sizeof(PVOID)); ++li) {
				auto levels = ReadPointer(world, Offsets::Engine::World::Levels);
				if (!levels) break;

				auto level = ReadPointer(levels, li * sizeof(PVOID));
				if (!level) continue;
				
				for (auto ai = 0UL; ai < ReadDWORD(level, Offsets::Engine::Level::AActors + sizeof(PVOID)); ++ai) {
					auto actors = ReadPointer(level, Offsets::Engine::Level::AActors);
					if (!actors) break;
					
					auto pawn = reinterpret_cast<UObject *>(ReadPointer(actors, ai * sizeof(PVOID)));
					if (!pawn || pawn == localPlayerPawn) continue;

					auto name = Util::GetObjectFirstName(pawn);
					if (wcsstr(name.c_str(), L"PlayerPawn_Athena_C") || wcsstr(name.c_str(), L"PlayerPawn_Athena_Phoebe_C")) {
						playerPawns.push_back(pawn);
					} else if (wcsstr(name.c_str(), L"FortPickupAthena")) {
						auto item = ReadPointer(pawn, Offsets::FortniteGame::FortPickup::PrimaryPickupItemEntry + Offsets::FortniteGame::FortItemEntry::ItemDefinition);
						if (!item) continue;

						auto itemName = reinterpret_cast<FText *>(ReadPointer(item, Offsets::FortniteGame::FortItemDefinition::DisplayName));
						if (!itemName || !itemName->c_str()) continue;

						auto isAmmo = wcsstr(itemName->c_str(), L"Ammo: ");
						if ((!Settings.ESP.Ammo && isAmmo) || ((!Settings.ESP.Weapons || ReadBYTE(item, Offsets::FortniteGame::FortItemDefinition::Tier) < Settings.ESP.MinWeaponTier) && !isAmmo)) continue;

						CHAR text[0xFF] = { 0 };
						wcstombs(text, itemName->c_str() + (isAmmo ? 6 : 0), sizeof(text));

						AddMarker(window, width, height, localPlayerLocation, pawn, text, isAmmo ? ImGui::GetColorU32({ 0.75f, 0.75f, 0.75f, 1.0f }) : ImGui::GetColorU32({ 1.0f, 1.0f, 1.0f, 1.0f }));
					} else if (Settings.ESP.Containers && wcsstr(name.c_str(), L"Tiered_Chest") && !((ReadBYTE(pawn, Offsets::FortniteGame::BuildingContainer::bAlreadySearched) >> 7) & 1)) {
						AddMarker(window, width, height, localPlayerLocation, pawn, "Chest", ImGui::GetColorU32({ 1.0f, 0.84f, 0.0f, 1.0f }));
					} else if (Settings.ESP.Containers && wcsstr(name.c_str(), L"AthenaSupplyDrop_Llama")) {
						AddMarker(window, width, height, localPlayerLocation, pawn, "Llama", ImGui::GetColorU32({ 1.0f, 0.0f, 0.0f, 1.0f }));
					} else if (Settings.ESP.Ammo && wcsstr(name.c_str(), L"Tiered_Ammo") && !((ReadBYTE(pawn, Offsets::FortniteGame::BuildingContainer::bAlreadySearched) >> 7) & 1)) {
						AddMarker(window, width, height, localPlayerLocation, pawn, "Ammo Box", ImGui::GetColorU32({ 0.75f, 0.75f, 0.75f, 1.0f }));
					}
				}
			}

			for (auto pawn : playerPawns) {
				auto state = ReadPointer(pawn, Offsets::Engine::Pawn::PlayerState);
				if (!state) continue;

				auto mesh = ReadPointer(pawn, Offsets::Engine::Character::Mesh);
				if (!mesh) continue;

				auto bones = ReadPointer(mesh, Offsets::Engine::StaticMeshComponent::StaticMesh);
				if (!bones) bones = ReadPointer(mesh, Offsets::Engine::StaticMeshComponent::StaticMesh + 0x10);
				if (!bones) continue;

				float compMatrix[4][4] = { 0 };
				Util::ToMatrixWithScale(reinterpret_cast<float *>(reinterpret_cast<PBYTE>(mesh) + Offsets::Engine::StaticMeshComponent::ComponentToWorld), compMatrix);

				// Top
				float head[3] = { 0 };
				Util::GetBoneLocation(compMatrix, bones, BONE_HEAD_ID, head);

				float neck[3] = { 0 };
				Util::GetBoneLocation(compMatrix, bones, 65, neck);

				float chest[3] = { 0 };
				Util::GetBoneLocation(compMatrix, bones, 36, chest);

				float pelvis[3] = { 0 };
				Util::GetBoneLocation(compMatrix, bones, 2, pelvis);

				// Arms
				float leftShoulder[3] = { 0 };
				Util::GetBoneLocation(compMatrix, bones, 9, leftShoulder);

				float rightShoulder[3] = { 0 };
				Util::GetBoneLocation(compMatrix, bones, 62, rightShoulder);

				float leftElbow[3] = { 0 };
				Util::GetBoneLocation(compMatrix, bones, 10, leftElbow);

				float rightElbow[3] = { 0 };
				Util::GetBoneLocation(compMatrix, bones, 38, rightElbow);

				float leftHand[3] = { 0 };
				Util::GetBoneLocation(compMatrix, bones, 11, leftHand);

				float rightHand[3] = { 0 };
				Util::GetBoneLocation(compMatrix, bones, 39, rightHand);

				// Legs
				float leftLeg[3] = { 0 };
				Util::GetBoneLocation(compMatrix, bones, 67, leftLeg);

				float rightLeg[3] = { 0 };
				Util::GetBoneLocation(compMatrix, bones, 74, rightLeg);

				float leftThigh[3] = { 0 };
				Util::GetBoneLocation(compMatrix, bones, 73, leftThigh);

				float rightThigh[3] = { 0 };
				Util::GetBoneLocation(compMatrix, bones, 80, rightThigh);

				float leftFoot[3] = { 0 };
				Util::GetBoneLocation(compMatrix, bones, 68, leftFoot);

				float rightFoot[3] = { 0 };
				Util::GetBoneLocation(compMatrix, bones, 75, rightFoot);

				float leftFeet[3] = { 0 };
				Util::GetBoneLocation(compMatrix, bones, 71, leftFeet);

				float rightFeet[3] = { 0 };
				Util::GetBoneLocation(compMatrix, bones, 78, rightFeet);

				float leftFeetFinger[3] = { 0 };
				Util::GetBoneLocation(compMatrix, bones, 72, leftFeetFinger);

				float rightFeetFinger[3] = { 0 };
				Util::GetBoneLocation(compMatrix, bones, 79, rightFeetFinger);

				auto color = ImGui::GetColorU32({ Settings.ESP.PlayerNotVisibleColor[0], Settings.ESP.PlayerNotVisibleColor[1], Settings.ESP.PlayerNotVisibleColor[2], 1.0f });
				FVector viewPoint = { 0 };
				if (ReadDWORD(state, Offsets::FortniteGame::FortPlayerStateAthena::TeamIndex) == localPlayerTeamIndex) {
					color = ImGui::GetColorU32({ 0.0f, 1.0f, 0.0f, 1.0f });
				} else if (!(ReadBYTE(pawn, Offsets::FortniteGame::FortPawn::bIsDBNO) & 1) && (isProjectileWeapon || Util::LineOfSightTo(localPlayerController, pawn, &viewPoint))) {
					color = ImGui::GetColorU32({ Settings.ESP.PlayerVisibleColor[0], Settings.ESP.PlayerVisibleColor[1], Settings.ESP.PlayerVisibleColor[2], 1.0f });

					if (Settings.AutoAimbot) {
						auto dx = head[0] - localPlayerLocation[0];
						auto dy = head[1] - localPlayerLocation[1];
						auto dz = head[2] - localPlayerLocation[2];
						auto dist = dx * dx + dy * dy + dz * dz;
						if (dist < closestDistance) {
							closestDistance = dist;
							closestPawn = pawn;
						}
					} else {
						auto w2s = *reinterpret_cast<FVector *>(head);
						if (Util::WorldToScreen(width, height, &w2s.X)) {
							auto dx = w2s.X - (width / 2);
							auto dy = w2s.Y - (height / 2);
							auto dist = Util::SpoofCall(sqrtf, dx * dx + dy * dy);
							if (dist < Settings.AimbotFOV && dist < closestDistance) {
								closestDistance = dist;
								closestPawn = pawn;
							}
						}
					}
				}

				if (!Settings.ESP.Players) continue;

				if (Settings.ESP.PlayerLines) {
					auto end = *reinterpret_cast<FVector *>(head);
					if (Util::WorldToScreen(width, height, &end.X)) {
						window.DrawList->AddLine(ImVec2(width / 2, height), ImVec2(end.X, end.Y), color);
					}
				}

				float minX = FLT_MAX;
				float maxX = -FLT_MAX;
				float minY = FLT_MAX;
				float maxY = -FLT_MAX;

				AddLine(window, width, height, head, neck, color, minX, maxX, minY, maxY);
				AddLine(window, width, height, neck, pelvis, color, minX, maxX, minY, maxY);
				AddLine(window, width, height, chest, leftShoulder, color, minX, maxX, minY, maxY);
				AddLine(window, width, height, chest, rightShoulder, color, minX, maxX, minY, maxY);
				AddLine(window, width, height, leftShoulder, leftElbow, color, minX, maxX, minY, maxY);
				AddLine(window, width, height, rightShoulder, rightElbow, color, minX, maxX, minY, maxY);
				AddLine(window, width, height, leftElbow, leftHand, color, minX, maxX, minY, maxY);
				AddLine(window, width, height, rightElbow, rightHand, color, minX, maxX, minY, maxY);
				AddLine(window, width, height, pelvis, leftLeg, color, minX, maxX, minY, maxY);
				AddLine(window, width, height, pelvis, rightLeg, color, minX, maxX, minY, maxY);
				AddLine(window, width, height, leftLeg, leftThigh, color, minX, maxX, minY, maxY);
				AddLine(window, width, height, rightLeg, rightThigh, color, minX, maxX, minY, maxY);
				AddLine(window, width, height, leftThigh, leftFoot, color, minX, maxX, minY, maxY);
				AddLine(window, width, height, rightThigh, rightFoot, color, minX, maxX, minY, maxY);
				AddLine(window, width, height, leftFoot, leftFeet, color, minX, maxX, minY, maxY);
				AddLine(window, width, height, rightFoot, rightFeet, color, minX, maxX, minY, maxY);
				AddLine(window, width, height, leftFeet, leftFeetFinger, color, minX, maxX, minY, maxY);
				AddLine(window, width, height, rightFeet, rightFeetFinger, color, minX, maxX, minY, maxY);

				if (minX < width && maxX > 0 && minY < height && maxY > 0) {
					auto topLeft = ImVec2(minX - 3.0f, minY - 3.0f);
					auto bottomRight = ImVec2(maxX + 3.0f, maxY + 3.0f);

					window.DrawList->AddRectFilled(topLeft, bottomRight, ImGui::GetColorU32({ 0.0f, 0.0f, 0.0f, 0.20f }));
					window.DrawList->AddRect(topLeft, bottomRight, ImGui::GetColorU32({ 0.0f, 0.50f, 0.90f, 1.0f }), 0.5, 15, 1.5f);

					if (Settings.ESP.PlayerNames) {
						FString playerName;
						Core::ProcessEvent(state, Offsets::Engine::PlayerState::GetPlayerName, &playerName, 0);
						if (playerName.c_str()) {
							CHAR copy[0xFF] = { 0 };
							wcstombs(copy, playerName.c_str(), sizeof(copy));
							Util::FreeInternal(playerName.c_str());

							auto centerTop = ImVec2((topLeft.x + bottomRight.x) / 2.0f, topLeft.y);
							auto size = ImGui::GetFont()->CalcTextSizeA(window.DrawList->_Data->FontSize, FLT_MAX, 0, copy);
							window.DrawList->AddRectFilled(ImVec2(centerTop.x - size.x / 2.0f, centerTop.y - size.y + 3.0f), ImVec2(centerTop.x + size.x / 2.0f, centerTop.y), ImGui::GetColorU32({ 0.0f, 0.0f, 0.0f, 0.4f }));
							window.DrawList->AddText(ImVec2(centerTop.x - size.x / 2.0f, centerTop.y - size.y), color, copy);
						}
					}
				}
			}

			if (Settings.Aimbot && closestPawn && Util::SpoofCall(GetAsyncKeyState, VK_RBUTTON) < 0 && Util::SpoofCall(GetForegroundWindow) == hWnd) {
				Core::TargetPawn = closestPawn;
				Core::NoSpread = TRUE;
			} else {
				Core::TargetPawn = nullptr;
				Core::NoSpread = FALSE;
			}

			if (!Settings.AutoAimbot && Settings.ESP.AimbotFOV) {
				window.DrawList->AddCircle(ImVec2(width / 2, height / 2), Settings.AimbotFOV, ImGui::GetColorU32({ 0.0f, 0.0f, 0.0f, 1.0f }), 128);
			}

			success = TRUE;
		} while (FALSE);

		if (!success) {
			Core::LocalPlayerController = Core::LocalPlayerPawn = Core::TargetPawn = nullptr;
		}

		EndScene(window);

		return PresentOriginal(swapChain, syncInterval, flags);
	}

	__declspec(dllexport) HRESULT ResizeHook(IDXGISwapChain *swapChain, UINT bufferCount, UINT width, UINT height, DXGI_FORMAT newFormat, UINT swapChainFlags) {
		ImGui_ImplDX11_Shutdown();
		renderTargetView->Release();
		immediateContext->Release();
		device->Release();
		device = nullptr;

		return ResizeOriginal(swapChain, bufferCount, width, height, newFormat, swapChainFlags);
	}

	BOOLEAN Initialize() {
		IDXGISwapChain      *swapChain    = nullptr;
		ID3D11Device        *device       = nullptr;
		ID3D11DeviceContext *context      = nullptr;
		auto                 featureLevel = D3D_FEATURE_LEVEL_11_0;

		DXGI_SWAP_CHAIN_DESC sd           = { 0 };
		sd.BufferCount                    = 1;
		sd.BufferDesc.Format              = DXGI_FORMAT_R8G8B8A8_UNORM;
		sd.BufferUsage                    = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		sd.Flags                          = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
		sd.OutputWindow                   = FindWindow(xorstr(L"UnrealWindow"), xorstr(L"Fortnite  "));
		sd.SampleDesc.Count               = 1;
		sd.Windowed                       = TRUE;

		if (FAILED(D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, 0, 0, &featureLevel, 1, D3D11_SDK_VERSION, &sd, &swapChain, &device, nullptr, &context))) {
			MessageBox(0, L"Failed to create D3D11 device and swap chain", L"Failure", MB_ICONERROR);
			return FALSE;
		}

		auto table = *reinterpret_cast<PVOID **>(swapChain);
		auto present = table[8];
		auto resize = table[13];

		context->Release();
		device->Release();
		swapChain->Release();

		MH_CreateHook(present, PresentHook, reinterpret_cast<PVOID *>(&PresentOriginal));
		MH_EnableHook(present);

		MH_CreateHook(resize, ResizeHook, reinterpret_cast<PVOID *>(&ResizeOriginal));
		MH_EnableHook(resize);

		return TRUE;
	}
}