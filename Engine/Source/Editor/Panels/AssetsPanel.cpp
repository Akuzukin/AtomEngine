#include "../EditorPanelSupport.h"
#include "../ScriptFieldEditor.h"
#include "Runtime/Function/Render/Particle/ParticleComponentEditor.h"
namespace AtomEngine
{
	void AssetsPanel::Draw(EditorLayer& editor, Scene& s)
	{
        if (ImGui::GetFrameCount() == EditorDefaultPanelFocusFrame) ImGui::SetNextWindowFocus();
		ImGui::Begin(EditorLocalization::Label("Assets"), &editor.mShowAssets);
		ImGui::BeginChild("Asset folders", ImVec2(155, 0), true);
		std::function<void(const std::filesystem::path&, int)> folders = [&](const auto& path, int depth)
			{
				if (depth > 8)return;
				auto label = PathText(path.filename()); bool open = ImGui::TreeNodeEx(PathText(path).c_str(), ImGuiTreeNodeFlags_OpenOnArrow | (depth == 0 ? ImGuiTreeNodeFlags_DefaultOpen : 0), "%s", label.c_str());
				if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) { editor.mAssetDirectory = path; editor.mRefreshAssets = true; }
				if (open) { std::error_code ec; int count = 0; for (auto& entry : std::filesystem::directory_iterator(path, ec)) { if (++count > 256)break; if (entry.is_directory(ec) && !entry.is_symlink(ec))folders(entry.path(), depth + 1); }ImGui::TreePop(); }
			};
		folders("Asset", 0); ImGui::EndChild(); ImGui::SameLine(); ImGui::BeginChild("Asset browser", ImVec2(0, 0));
		if (ImGui::Button(EditorLocalization::Label("Assets")))
		{
			editor.mAssetDirectory = "Asset";
			editor.mRefreshAssets = true;
		}
		ImGui::SameLine();
		if (ImGui::Button(EditorLocalization::Label("Materials")))
		{
			editor.mAssetDirectory = "Asset/Materials";
			editor.mRefreshAssets = true;
		}
		ImGui::SameLine();
		if (ImGui::Button(EditorLocalization::Label("Scripts")))
		{
			editor.mAssetDirectory = Preferences().parent_path() / "Scripts";
			editor.mRefreshAssets = true;
		}
		ImGui::SameLine();
		if (ImGui::Button(EditorLocalization::Label("Up")) && editor.mAssetDirectory.has_parent_path())
		{
			editor.mAssetDirectory = editor.mAssetDirectory.parent_path();
			editor.mRefreshAssets = true;
		}
		ImGui::SameLine();
		if (ImGui::Button(EditorLocalization::Label("Refresh")))
			editor.mRefreshAssets = true;
		ImGui::SameLine();
		if(ImGui::Button(EditorLocalization::Label("New particle")))
			editor.Run([&]{auto path=ParticleComponentEditor::CreateAsset(editor.mAssetDirectory,&editor);editor.mAssetSelection=PathText(path);});
		if (ImGui::GetContentRegionAvail().x > 280)
			ImGui::SameLine();
		editor.ScriptCreation(s.GetWorld());
		ImGui::Spacing();
		ImGui::TextDisabled("%s", PathText(editor.mAssetDirectory).c_str());

		const bool wideSearch = ImGui::GetContentRegionAvail().x > 400;
		ImGui::SetNextItemWidth(wideSearch ? ImGui::GetContentRegionAvail().x - 230 : -1);
		if (ImGui::InputTextWithHint("##Search assets", EditorLocalization::Text("Search assets..."), search.InputBuf,
			sizeof(search.InputBuf)))
			search.Build();
		EditorTheme::Tooltip(EditorLocalization::Text("Filter assets by name; use commas for multiple filters"));
		if (wideSearch)
			ImGui::SameLine();
		ImGui::Checkbox(EditorLocalization::Label("Thumbnails"), &editor.mThumbnailView);
		ImGui::SameLine();
		ImGui::TextDisabled(EditorLocalization::Text("%zu items"), editor.mAssetEntries.size());
		if (editor.mRefreshAssets)
		{
			editor.ClearThumbnails();
			editor.mRefreshAssets = false;
			editor.mAssetEntries.clear();
			std::error_code error;
			for (std::filesystem::directory_iterator it(editor.mAssetDirectory, error), end; !error && it != end;
				it.increment(error))
				if (it->is_directory(error) || (!error && it->is_regular_file(error)))
					editor.mAssetEntries.push_back(it->path());
			std::sort(editor.mAssetEntries.begin(), editor.mAssetEntries.end(), [](auto& a, auto& b)
				{
					std::error_code ec;
					bool af = std::filesystem::is_directory(a, ec), bf = std::filesystem::is_directory(b, ec);
					if (af != bf)
						return af;
					return a.filename() < b.filename();
				});
			if (error)
				WriteLog(LogLevel::Warning, "Editor", "Cannot browse assets: " + error.message());
		}
		ImGui::Separator();
		int thumbnailBudget = 2;
		size_t pendingThumbnails = 0;
		for (auto& [key, thumbnail] : editor.mThumbnails)
			if (thumbnail.pending)
			{
				++pendingThumbnails;
				if (thumbnailBudget <= 0)
					continue;
				auto before = thumbnail.pending->Report().completed;
				const auto& report = thumbnail.pending->Step(1);
				if (report.completed != before)
					--thumbnailBudget;
				if (!report.Ready())
					continue;
				try
				{
					if (!report.Success())
						throw std::runtime_error("Thumbnail preload failed");
					thumbnail.texture = AssetManager::LoadCovertTexture(thumbnail.path, kWhiteOpaque2D, true);
					if (!thumbnail.texture.IsValid() ||
						const_cast<ID3D12Resource*>(thumbnail.texture->GetResource())->GetDesc().DepthOrArraySize != 1)
						thumbnail.failed = true;
					else
					{
						thumbnail.handle = Renderer::GetTextureHeap().Alloc();
						DX12Core::gDevice->CopyDescriptorsSimple(1, thumbnail.handle, thumbnail.texture.GetSRV(),
							D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
					}
				}
				catch (const std::exception&)
				{
					thumbnail.failed = true;
				}
				thumbnail.pending.reset();
				--pendingThumbnails;
			}
		ImGui::BeginChild("Asset list", ImVec2(editor.mAssetSelection.empty() ? 0 : ImGui::GetContentRegionAvail().x * .65f, 0));
		bool grid =
			editor.mThumbnailView && ImGui::BeginTable("Asset grid", std::max(1, int(ImGui::GetContentRegionAvail().x / 116.f)),
				ImGuiTableFlags_SizingFixedFit);
		for (auto& entry : editor.mAssetEntries)
		{
			auto name = PathText(entry.filename());
			if (!search.PassFilter(name.c_str()))
				continue;
			std::error_code statusError;
			bool folder = std::filesystem::is_directory(entry, statusError);
			auto path = PathText(entry);
			if (std::filesystem::absolute(entry).parent_path() == Preferences().parent_path() / "Scripts")
				path = "Scripts/" + name;
			ImGui::PushID(path.c_str());
			bool clicked = false;
			if (grid)
			{
				ImGui::TableNextColumn();
				auto pos = ImGui::GetCursorScreenPos();
				clicked = ImGui::InvisibleButton("Asset tile", ImVec2(108, 100));
				auto draw = ImGui::GetWindowDrawList();
				auto color = editor.mAssetSelection == path ? EditorTheme::U32(40, 70, 94)
					: ImGui::IsItemHovered() ? EditorTheme::U32(52, 52, 52)
					: EditorTheme::U32(29, 29, 29);
				draw->AddRectFilled(pos, ImVec2(pos.x + 108, pos.y + 100), color, 5);
				if (folder)
				{
					draw->AddRectFilled(ImVec2(pos.x + 18, pos.y + 20), ImVec2(pos.x + 50, pos.y + 31),
						EditorTheme::U32(226, 175, 66), 3);
					draw->AddRectFilled(ImVec2(pos.x + 18, pos.y + 28), ImVec2(pos.x + 90, pos.y + 64),
						EditorTheme::U32(234, 187, 82), 4);
				}
				else
				{
					auto key = PathText(std::filesystem::absolute(entry));
					auto found = editor.mThumbnails.find(key);
					if (TextureFile(entry) && found == editor.mThumbnails.end() && thumbnailBudget > 0 &&
						editor.mThumbnails.size() < 128 && pendingThumbnails < 4 && ImGui::IsItemVisible())
					{
						--thumbnailBudget;
						EditorLayer::Thumbnail thumbnail;
						try
						{
							thumbnail.path = path;
							thumbnail.pending = AssetManager::PreloadAsync(
								{ {AssetKind::Texture, path, BuiltinMesh::None, kWhiteOpaque2D, true} });
							thumbnail.pending->Step(1);
							++pendingThumbnails;
						}
						catch (const std::exception&)
						{
							thumbnail.failed = true;
						}
						found = editor.mThumbnails.emplace(key, std::move(thumbnail)).first;
					}
					if (found != editor.mThumbnails.end() && !found->second.failed && !found->second.pending &&
						found->second.texture.IsValid())
					{
						auto& thumbnail = found->second;
						float aspect = float(thumbnail.texture.Width()) / std::max(1u, thumbnail.texture.Height());
						ImVec2 image(aspect >= 1 ? 72.f : 72.f * aspect, aspect >= 1 ? 72.f / aspect : 72.f);
						auto top = ImVec2(pos.x + (108 - image.x) * .5f, pos.y + 4 + (72 - image.y) * .5f);
						draw->AddImage((ImTextureID)thumbnail.handle.GetGpuPtr(), top,
							ImVec2(top.x + image.x, top.y + image.y));
					}
					else
					{
						draw->AddRectFilled(ImVec2(pos.x + 27, pos.y + 12), ImVec2(pos.x + 81, pos.y + 77),
							EditorTheme::U32(63, 63, 63), 3);
						auto extension = Extension(entry);
						EditorTheme::ResourceIcon(draw, ImVec2(pos.x + 52, pos.y + 27), 14, extension);
						draw->AddText(ImVec2(pos.x + 32, pos.y + 53), EditorTheme::U32(218, 229, 240), extension.c_str());
					}
				}
				std::string label = name;
				while (label.size() > 3 && ImGui::CalcTextSize(label.c_str()).x > 98)
				{
					size_t last = label.size() - 1;
					while (last > 0 && (uint8_t(label[last]) & 0xc0) == 0x80)
						--last;
					label.erase(last);
				}
				draw->AddText(ImVec2(pos.x + 5, pos.y + 80), EditorTheme::U32(226, 232, 240), label.c_str());
				if (ImGui::IsItemHovered())
					ImGui::SetTooltip("%s", name.c_str());
			}
			else
				clicked = ImGui::Selectable((folder ? "[Folder] " + name : name).c_str(), editor.mAssetSelection == path);
			if (clicked)
			{
				editor.mAssetSelection = path;
				if (!folder && Extension(entry) == ".mat")
					editor.Run([&] { editor.SelectMaterial(entry); });
				editor.mAssetPreview = nullptr;
				if (!folder && TextureFile(entry))
					editor.Run([&]
						{
							editor.mAssetPreview = AssetManager::LoadCovertTexture(PathText(entry), kWhiteOpaque2D, true);
							if (!editor.mAssetPreview.IsValid())
								throw std::runtime_error("Cannot preview texture");
							auto handle = Renderer::GetTextureHeap().Alloc();
							DX12Core::gDevice->CopyDescriptorsSimple(1, handle, editor.mAssetPreview.GetSRV(),
								D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
							if (!editor.mAssetPreviewHandle.IsNull())
								Renderer::GetTextureHeap().Free(editor.mAssetPreviewHandle);
							editor.mAssetPreviewHandle = handle;
						});
			}
			if (!folder && ImGui::BeginDragDropSource())
			{
				ImGui::SetDragDropPayload("ATOM_ASSET", path.c_str(), path.size() + 1);
				ImGui::TextUnformatted(name.c_str());
				ImGui::EndDragDropSource();
			}
			if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
			{
				if (folder)
				{
					editor.mAssetDirectory = entry;
					editor.mRefreshAssets = true;
				}
				else if(Extension(entry)==".particle")editor.Run([&]{ParticleComponentEditor::OpenAsset(entry,&editor);});
				else if ((Extension(entry) == ".mp3" || Extension(entry) == ".wav") && !editor.mPlaying)
					editor.Run([&]
						{
							Audio::GetInstance()->StopOwner(UINT64_MAX);
							Audio::GetInstance()->PlayClip(path, 1, false, UINT64_MAX);
						});
				else if (Extension(entry) == ".scene" && !editor.mPlaying)
					editor.OpenScene(UTF8ToWString(path));
				else if (Extension(entry) == ".prefab" && !editor.mPlaying)
					editor.Run([&]
						{
							editor.Select(s.GetWorld(), SceneSerializer::InstantiatePrefab(s.GetWorld(), UTF8ToWString(path)));
						});
			}
			ImGui::PopID();
		}
		if (grid)
			ImGui::EndTable();
		if(ImGui::BeginPopupContextWindow("Create particle asset",ImGuiPopupFlags_MouseButtonRight|ImGuiPopupFlags_NoOpenOverItems)) {
			if(ImGui::MenuItem(EditorLocalization::Label("New particle")))editor.Run([&]{auto path=ParticleComponentEditor::CreateAsset(editor.mAssetDirectory,&editor);editor.mAssetSelection=PathText(path);});
			ImGui::EndPopup();
		}
		ImGui::EndChild();
		if (!editor.mAssetSelection.empty())
		{
			ImGui::SameLine();
			ImGui::BeginChild("Asset details");
			ImGui::TextWrapped("%s", editor.mAssetSelection.c_str());
			if (ImGui::Button(EditorLocalization::Label("Copy path")))
				ImGui::SetClipboardText(editor.mAssetSelection.c_str());
			if(std::filesystem::path(UTF8ToWString(editor.mAssetSelection)).extension()==L".particle") {
				if(ImGui::Button("Edit particle asset"))editor.Run([&]{ParticleComponentEditor::OpenAsset(UTF8ToWString(editor.mAssetSelection),&editor);});
				if(ImGui::Button("Create particle object"))editor.Run([&]{editor.Select(s.GetWorld(),SceneSerializer::InstantiateParticlePrefab(s.GetWorld(),UTF8ToWString(editor.mAssetSelection)));editor.mHistoryPending=true;});
			}
			if (editor.mAssetPreview.IsValid())
			{
				float width = std::min(180.f, ImGui::GetContentRegionAvail().x);
				ImGui::Image((ImTextureID)editor.mAssetPreviewHandle.GetGpuPtr(),
					ImVec2(width, width * float(editor.mAssetPreview.Height()) / std::max(1u, editor.mAssetPreview.Width())));
				auto materials = s.GetWorld().TryGet<MaterialComponent>(editor.mSelected);
				ImGui::BeginDisabled(editor.mPlaying || !materials || materials->mMaterials.empty());
				if (ImGui::Button(EditorLocalization::Label("Use as BaseColor")))
					materials->mMaterials[0].texturePaths[0] = editor.mAssetSelection;
				ImGui::EndDisabled();
			}
			if (ImGui::Button(EditorLocalization::Label("Open externally")))
			{
				auto path = std::filesystem::path(UTF8ToWString(editor.mAssetSelection));
				if (editor.mAssetSelection.starts_with("Scripts/"))
					path = Preferences().parent_path() / path;
				ShellExecuteW(gContext.windowManager->GetWindowHandle(), L"open", path.c_str(), nullptr, nullptr,
					SW_SHOWNORMAL);
			}
			ImGui::EndChild();
		}
		ImGui::EndChild();
		ImGui::End();
	}
}
