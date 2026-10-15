#include "Editor/EditorLayer.h"
#include "ParticleComponentEditor.h"
#include "ParticleSystem.h"
#include "ParticleSettingsCodec.h"
#include "ParticleAsset.h"
#include "Runtime/Core/Utility/Utility.h"
#include "Runtime/Function/Global/GlobalContext.h"
#include "Runtime/Function/Render/WindowManager.h"
#include "Runtime/Function/Framework/ECS/GameObject.h"
#include "Runtime/Function/Framework/Component/TransformComponent.h"
#include <commdlg.h>
#include "Runtime/Function/Render/RenderSystem.h"
#include "Runtime/Platform/DirectX12/Core/DirectX12Core.h"
#include "Runtime/Resource/AssetArchive.h"
#include "Runtime/Function/Camera/CameraBase.h"
#include "Editor/ImGuiCommon.h"
#include "imgui.h"
#include <cmath>
#include <algorithm>
namespace AtomEngine
{
	namespace { World* world = nullptr;EditorLayer *editor=nullptr; Entity entity = entt::null; std::weak_ptr<void> lifetime; bool open = false, dirty = true, playing = true; ColorBuffer image; DepthBuffer depth; DescriptorHandle descriptor; uint32_t preview = 0xffffffff; Camera camera; std::string signature,error; float yaw=2.55f,pitch=.4f,distance=7;int selectedPreset=0; }
	namespace { ParticleSettings presetDraft; }
	namespace
	{
		std::unique_ptr<World> assetWorld;
		std::filesystem::path assetPath;
		std::string savedSignature;
		bool closePrompt=false;
		std::filesystem::path SaveDialog()
		{
			if(assetPath.empty())std::filesystem::create_directories("Asset/Particles");
			wchar_t file[32768]{};auto initial=std::filesystem::absolute(assetPath.empty()?std::filesystem::path("Asset/Particles/New Particle.particle"):assetPath);
			wcsncpy_s(file,initial.c_str(),_TRUNCATE);
			OPENFILENAMEW dialog{};dialog.lStructSize=sizeof(dialog);dialog.hwndOwner=gContext.windowManager->GetWindowHandle();
			dialog.lpstrFile=file;dialog.nMaxFile=DWORD(std::size(file));dialog.lpstrFilter=L"Particle asset (*.particle)\0*.particle\0\0";dialog.lpstrDefExt=L"particle";
			dialog.Flags=OFN_EXPLORER|OFN_NOCHANGEDIR|OFN_PATHMUSTEXIST|OFN_OVERWRITEPROMPT;
			if(GetSaveFileNameW(&dialog))return file;
			if(CommDlgExtendedError())throw std::runtime_error("Particle save dialog failed");return {};
		}
	}
	bool ParticleComponentEditor::HasUnsavedChanges()
	{
		return assetWorld && !lifetime.expired() && world->IsValid(entity) &&
			ParticleJson(world->GetComponent<ParticleComponent>(entity).settings).dump()!=savedSignature;
	}
	void ParticleComponentEditor::Open(World& w, Entity e,EditorLayer *owner)
	{
		if(HasUnsavedChanges()){error="Save or revert the current particle asset before opening another document.";throw std::runtime_error(error);}
		Shutdown();world=&w;editor=owner;entity=e;lifetime=w.GetLifetime();error.clear();
		try {if(auto c=w.TryGet<ParticleComponent>(e))presetDraft=c->preset.empty()?c->settings:ReadParticleSettings(ReadAssetText(UTF8ToWString(c->preset)));}
		catch(const std::exception &ex){error=ex.what();}
		open=true;dirty=true;playing=true;selectedPreset=0;
	}
	void ParticleComponentEditor::OpenAsset(const std::filesystem::path &path,EditorLayer *owner)
	{
		if(path.extension()!=L".particle")throw std::runtime_error("Expected .particle asset");
		auto settings=LoadParticleAsset(path);auto isolated=std::make_unique<World>();auto object=isolated->CreateGameObject(WStringToUTF8(path.stem().wstring()));
		object->AddComponent<TransformComponent>();object->AddComponent<ParticleComponent>();object->GetComponent<ParticleComponent>().settings=settings;
		Open(*isolated,object->GetHandle(),owner);assetWorld=std::move(isolated);assetPath=std::filesystem::absolute(path);savedSignature=ParticleJson(settings).dump();
	}
	std::filesystem::path ParticleComponentEditor::CreateAsset(const std::filesystem::path &directory,EditorLayer *owner)
	{
		if(HasUnsavedChanges())throw std::runtime_error("Save or revert the current particle asset first");
		auto path=directory/L"New Particle.particle";int suffix=1;
		while(std::filesystem::exists(path))path=directory/(L"New Particle "+std::to_wstring(suffix++)+L".particle");
		SaveParticleAsset(path,ParticleSettings{});OpenAsset(path,owner);if(owner)owner->mRefreshAssets=true;return path;
	}
	void ParticleComponentEditor::SaveAs(const std::filesystem::path &path)
	{
		if(!world||lifetime.expired()||!world->IsValid(entity))throw std::runtime_error("No particle document");
		auto &component=world->GetComponent<ParticleComponent>(entity);
		auto settings=component.preset.empty()?component.settings:presetDraft;SaveParticleAsset(path,settings);
		if(assetWorld){assetPath=std::filesystem::absolute(path);savedSignature=ParticleJson(settings).dump();world->SetName(entity,WStringToUTF8(path.stem().wstring()));}
		else {auto relative=std::filesystem::absolute(path).lexically_relative(std::filesystem::current_path());bool portable=!relative.empty()&&*relative.begin()!=L"..";component.preset=WStringToUTF8((portable?relative:std::filesystem::absolute(path)).generic_wstring());component.settings=settings;presetDraft=settings;if(editor)editor->mHistoryPending=true;}
		if(editor)editor->mRefreshAssets=true;error.clear();
	}
	void ParticleComponentEditor::Draw()
	{
		if (!open || lifetime.expired()){if(world)Shutdown();return;} if (!world->IsValid(entity) || !world->HasComponent<ParticleComponent>(entity)) { Shutdown(); return; }ImGui::SetNextWindowSize(ImVec2(1000, 680), ImGuiCond_FirstUseEver); if (ImGui::Begin("Particle component editor", &open))
		{
			auto& c = world->GetComponent<ParticleComponent>(entity); auto settings=c.preset.empty()?c.settings:presetDraft;auto &s=settings;auto before=c.settings;auto oldPreset=c.preset;auto oldAwake=c.playOnAwake;bool changed=false;
			ImGui::Text("%s%s",world->GetComponent<NameComponent>(entity).value.c_str(),HasUnsavedChanges()?" *":"");
			if(assetWorld)ImGui::TextWrapped("%s",WStringToUTF8(assetPath.wstring()).c_str());
			try {
				if(assetWorld&&ImGui::Button("Save .particle"))SaveAs(assetPath);
				if(assetWorld)ImGui::SameLine();
				if(ImGui::Button("Save as .particle...")){auto path=SaveDialog();if(!path.empty())SaveAs(path);}
				if(assetWorld){ImGui::SameLine();if(ImGui::Button("Revert")){auto loaded=LoadParticleAsset(assetPath);c.settings=loaded;savedSignature=ParticleJson(loaded).dump();dirty=true;error.clear();}}
			}catch(const std::exception &ex){error=ex.what();}
			ImGui::BeginChild("Settings",ImVec2(380,0),true);
			if (ImGui::Combo("Emitter preset", &selectedPreset, "Custom\0Burst\0Smoke\0Fountain\0Rain\0Ring\0"))
			{
				int preset=selectedPreset;
				s = ParticleSettings();
				if (preset == 1)
				{
					s.rate = 0; s.burst = 64; s.duration = .2f; s.gravity = { 0,0,0 };
				}
				if (preset == 2)
				{
					s.shape = EmitterShape::Sphere;
					s.gravity = { 0,1,0 };
					s.speed = .3f;
					s.sizeStart = .2f;
					s.sizeEnd = 1;
					s.colorStart = { .4f,.4f,.4f,.5f };
					s.colorEnd = { .2f,.2f,.2f,0 };
					s.loop = true;
					s.blend = ParticleBlend::Alpha;
				}
				if (preset == 3)
				{
					s.shape = EmitterShape::Cone;
					s.speed = 4;
					s.gravity = { 0,-3,0 };
					s.loop = true;
				}
				if (preset == 4)
				{
					s.shape = EmitterShape::Box;
					s.speed = 0;
					s.burst = 0;
					s.rate = 200;
					s.sizeStart = s.sizeEnd = .04f;
					s.extent = { 3,0,3 };
					s.gravity = { 0,-6,0 };
					s.loop = true;
				}
				if (preset == 5)
				{
					s.shape = EmitterShape::Ring;
					s.gravity = { 0,0,0 };
					s.burst = 64;
					s.rate = 0;
				}
				changed = true;
			}
			int shape = int(s.shape), blend = int(s.blend), capacity = int(s.capacity), burst = int(s.burst);
			changed |= ImGui::Combo("Shape", &shape, "Point\0Box\0Sphere\0Cone\0Ring\0");
			changed |= ImGui::Combo("Blend", &blend, "Alpha\0Additive\0Multiply\0Opaque\0");
			s.shape = EmitterShape(shape);
			s.blend = ParticleBlend(blend);
			changed |= ImGui::Checkbox("Billboard", &s.billboard);
			changed |= ImGui::Checkbox("Loop", &s.loop);
			if(!assetWorld)changed |= ImGui::Checkbox("Play on awake", &c.playOnAwake);
			char texture[1024]{}; strncpy_s(texture, s.texture.c_str(), _TRUNCATE);
			if (ImGui::InputText("Billboard texture (empty = color only)", texture, sizeof(texture)))
			{
				s.texture = texture; changed = true;
			}
			if (ImGui::BeginDragDropTarget())
			{
				if (auto p = ImGui::AcceptDragDropPayload("ATOM_ASSET"))
				{
					s.texture = static_cast<const char*>(p->Data); changed = true;
				}
				ImGui::EndDragDropTarget();
			}
			changed |= ImGui::SliderInt("Capacity", &capacity, 1, 65536);
			s.capacity = capacity;
			changed |= ImGui::SliderInt("Burst", &burst, 0, capacity); s.burst = std::min(burst,capacity);
			changed |= ImGui::DragFloat("Rate", &s.rate, 1, 0, 10000);
			changed |= ImGui::DragFloat("Duration", &s.duration, .05f, .01f, 60);
			changed |= ImGui::DragFloat("Lifetime", &s.lifetime, .05f, .01f, 60);
			changed |= ImGui::DragFloat("Speed", &s.speed, .05f, 0, 100);
			changed |= ImGui::DragFloat("Drag", &s.drag, .05f, 0, 20);
			changed |= ImGui::DragFloat("Radius", &s.radius, .05f, 0, 50);
			changed |= ImGui::DragFloat("Cone angle", &s.coneAngle, .5f, 0, 89);
			changed |= ImGui::DragFloat3("Box extent", &s.extent.x, .05f, 0, 50);
			changed |= ImGui::DragFloat3("Gravity", &s.gravity.x, .05f);
			changed |= ImGui::DragFloat("Start size", &s.sizeStart, .01f, 0, 20);
			changed |= ImGui::DragFloat("End size", &s.sizeEnd, .01f, 0, 20);
			changed |= ImGui::ColorEdit4("Start color", &s.colorStart.x);
			changed |= ImGui::ColorEdit4("End color", &s.colorEnd.x);
            bool toggle=ImGui::Checkbox("Randomize per particle",&s.randomize);changed|=toggle;
            auto defaultRanges=[&]{auto current=ParticleJson(s),defaults=ParticleJson(ParticleSettings{});for(auto key:{"lifetimeRange","speedRange","sizeStartRange","sizeEndRange","colorStartMin","colorStartMax","colorEndMin","colorEndMax"})if(current[key]!=defaults[key])return false;return true;};
            if(toggle&&s.randomize&&defaultRanges()){
                s.lifetimeRange={s.lifetime*.8f,s.lifetime*1.2f};s.speedRange={s.speed*.8f,s.speed*1.2f};
                s.sizeStartRange={s.sizeStart*.8f,s.sizeStart*1.2f};s.sizeEndRange={s.sizeEnd*.8f,s.sizeEnd*1.2f};
                for(int channel=0;channel<4;channel++){
                    float delta=channel==3?0.f:.05f;
                    (&s.colorStartMin.x)[channel]=std::clamp((&s.colorStart.x)[channel]-delta,0.f,1.f);
                    (&s.colorStartMax.x)[channel]=std::clamp((&s.colorStart.x)[channel]+delta,0.f,1.f);
                    (&s.colorEndMin.x)[channel]=std::clamp((&s.colorEnd.x)[channel]-delta,0.f,1.f);
                    (&s.colorEndMax.x)[channel]=std::clamp((&s.colorEnd.x)[channel]+delta,0.f,1.f);
                }
            }
            if(s.randomize){
                ImGui::TextWrapped("Random ranges replace the fixed values above. Left = minimum, right = maximum. Values are sampled once at birth.");
                changed|=ImGui::DragFloat2("Lifetime min/max",&s.lifetimeRange.x,.01f,.001f,60);
                changed|=ImGui::DragFloat2("Speed min/max",&s.speedRange.x,.01f,0,100);
                changed|=ImGui::DragFloat2("Start size min/max",&s.sizeStartRange.x,.01f,0,20);
                changed|=ImGui::DragFloat2("End size min/max",&s.sizeEndRange.x,.01f,0,20);
                changed|=ImGui::ColorEdit4("Start color minimum",&s.colorStartMin.x);
                changed|=ImGui::ColorEdit4("Start color maximum",&s.colorStartMax.x);
                changed|=ImGui::ColorEdit4("End color minimum",&s.colorEndMin.x);
                changed|=ImGui::ColorEdit4("End color maximum",&s.colorEndMax.x);
            }
			if(changed){auto after=s;auto afterAwake=c.playOnAwake;c.settings=before;c.preset=oldPreset;c.playOnAwake=oldAwake;try{after=ReadParticleSettings(ParticleJson(after).dump());auto apply=[&]{c.settings=after;c.preset="";c.playOnAwake=afterAwake;};if(editor&&!assetWorld){editor->Run(apply);editor->mHistoryPending=true;}else apply();dirty=true;error.clear();}catch(const std::exception &ex){error=ex.what();}}
			ImGui::EndChild();ImGui::SameLine();ImGui::BeginChild("Preview",ImVec2(0,0),true);
			if (ImGui::Button("Replay"))
			{
				dirty = true;
				playing = true;
			}
			ImGui::SameLine();
			ImGui::Checkbox("Preview running", &playing);
			if (!descriptor.IsNull()){
				float width=ImGui::GetContentRegionAvail().x;ImGuiCommon::ImageDisplayEncoded((ImTextureID)descriptor.GetGpuPtr(), ImVec2(width,width*2.f/3));
				if(ImGui::IsItemHovered()){auto &io=ImGui::GetIO();if(ImGui::IsMouseDragging(ImGuiMouseButton_Left)){yaw+=io.MouseDelta.x*.01f;pitch=std::clamp(pitch-io.MouseDelta.y*.01f,-1.4f,1.4f);}distance=std::clamp(distance-io.MouseWheel*.5f,.5f,50.f);}
			}
			ImGui::TextWrapped("Drag preview to orbit. Mouse wheel to zoom. Preview simulation is independent of the scene.");
			if(!error.empty())ImGui::TextWrapped("Preview error: %s",error.c_str());
			ImGui::EndChild();
		}ImGui::End();
		if(!open&&HasUnsavedChanges()){open=true;closePrompt=true;}
		if(closePrompt)ImGui::OpenPopup("Unsaved particle asset");
		if(ImGui::BeginPopupModal("Unsaved particle asset",nullptr,ImGuiWindowFlags_AlwaysAutoResize)){
			ImGui::TextUnformatted("Save changes to the particle asset?");
			if(ImGui::Button("Save")){try{SaveAs(assetPath);open=false;closePrompt=false;ImGui::CloseCurrentPopup();}catch(const std::exception &ex){error=ex.what();}}
			ImGui::SameLine();if(ImGui::Button("Discard")){open=false;closePrompt=false;ImGui::CloseCurrentPopup();}
			ImGui::SameLine();if(ImGui::Button("Cancel")){closePrompt=false;ImGui::CloseCurrentPopup();}
			if(!error.empty())ImGui::TextWrapped("%s",error.c_str());ImGui::EndPopup();
		}
	}
	void ParticleComponentEditor::Render(float dt)
	{
		if (!open || lifetime.expired())return;
		if (!world->IsValid(entity) || !world->HasComponent<ParticleComponent>(entity))return;
		try{
		auto& c = world->GetComponent<ParticleComponent>(entity);
		auto config = c.preset.empty() ? c.settings : ReadParticleSettings(ReadAssetText(UTF8ToWString(c.preset)));
		auto next = ParticleJson(config).dump();
		if (!image.GetResource())
		{
			image.Create(L"Particle editor preview", 480, 320, 1, DXGI_FORMAT_R8G8B8A8_UNORM);
			image.SetClearColor(Color(.06f,.08f,.1f,1));
			depth.Create(L"Particle editor depth", 480, 320, DXGI_FORMAT_D24_UNORM_S8_UINT);
			descriptor = Renderer::GetTextureHeap().Alloc();
			DX12Core::gDevice->CopyDescriptorsSimple(1, descriptor,
				image.GetSRV(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			camera.SetAspectRatio(320.f / 480); camera.Update();
		}
		camera.SetLookAt({distance*std::cos(pitch)*std::sin(yaw),distance*std::sin(pitch),distance*std::cos(pitch)*std::cos(yaw)},Vector3::ZERO,Vector3::UP);camera.Update();
		if (dirty || next != signature)
		{
			ParticleSystem::ReleaseParticle(preview);
			preview = ParticleSystem::Create(config, true);
			ParticleSystem::EmitParticle(preview, Vector3::ZERO);
			signature = next;
			dirty = false;
		}
		auto& ctx = GraphicsContext::Begin(L"Particle preview");
		if (playing)ParticleSystem::UpdatePreview(ctx.GetComputeContext(), preview, dt);
		ctx.TransitionResource(image, D3D12_RESOURCE_STATE_RENDER_TARGET);
		ctx.TransitionResource(depth, D3D12_RESOURCE_STATE_DEPTH_WRITE);
		ctx.ClearColor(image);
		ctx.ClearDepth(depth);
		ParticleSystem::RenderPreview(ctx, preview, camera, image, depth);
		ctx.TransitionResource(image, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		ctx.Finish();
		}catch(const std::exception &ex){error=ex.what();}
	}
	void ParticleComponentEditor::CapturePreview(const std::filesystem::path &path){if(image.GetResource())image.ExportToFile(path.wstring());}
	void ParticleComponentEditor::Shutdown()
	{
		if(image.GetResource())DX12Core::gCommandManager.IdleGPU();
		ParticleSystem::ReleaseParticle(preview);
		preview = 0xffffffff; image.Destroy();
		depth.Destroy();
		if (!descriptor.IsNull())Renderer::GetTextureHeap().Free(descriptor);
		descriptor = DescriptorHandle();
		open = false;
		world = nullptr;
		assetWorld.reset();assetPath.clear();savedSignature.clear();closePrompt=false;
	}
}
