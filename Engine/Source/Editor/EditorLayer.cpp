#include "Runtime/Function/Audio/Audio.h"
#include "EditorPanelSupport.h"
#include "EditorTheme.h"
#include "Runtime/Function/Global/GlobalContext.h"
#include "EditorLocalization.h"
#include "Runtime/Function/Scene/SceneView.h"
#include "imgui.h"
#include "ImGuizmo.h"
#include "Runtime/Core/LogSystem/LogSystem.h"
#include "Runtime/Function/Framework/Component/MeshComponent.h"
#include "Runtime/Function/Framework/ECS/GameObject.h"
#include "Runtime/Function/Input/Input.h"
#include "Runtime/Function/Render/SelectionRenderer.h"
#include "Runtime/Function/Scene/SceneSerializer.h"
#include "Runtime/Platform/DirectX12/Buffer/BufferManager.h"
#include "Runtime/Platform/DirectX12/Core/DirectX12Core.h"
#include "Runtime/Resource/Shader.h"
#include <algorithm>
#include <cstring>

namespace AtomEngine
{
	void EditorLayer::Run(const std::function<void()>& action)
	{
		try
		{
			action();
		}
		catch (const std::exception& e)
		{
			mError = e.what();
			WriteLog(LogLevel::Error, "Editor", mError);
		}
	}
	void EditorLayer::Select(World& w, Entity e)
	{
		mWorld = w.GetLifetime();
		mSelected = e;
		mModelDraftId.clear();
		SelectionRenderer::Select(&w, e);
	}
	void EditorLayer::Create(World& w, BuiltinMesh type)
	{
		auto o = w.CreateGameObject(type == BuiltinMesh::None ? "Object" : "Primitive");
		o->AddComponent<TransformComponent>();
		if (type != BuiltinMesh::None)
		{
			o->AddComponent<MeshSourceComponent>(type);
			SceneSerializer::RebuildMesh(w, o->GetHandle());
		}
		Select(w, o->GetHandle());
	}
	void EditorLayer::TogglePlay(Scene& s)
	{
		if (!mPlaying)
		{
			Capture(s);
			mPlayXml = SceneSerializer::ToXml(s.GetWorld(), s.GetName());
			mPlaySelection = EditorActions::Id(s.GetWorld(), mSelected);
			mEditCamera = s.GetCamera();
			mPlaying = true;
			mPaused = false;
			ActiveProject.pendingScene.reset();
			if (ActiveProject.project) ActiveProject.currentScene = WStringToUTF8(std::filesystem::absolute(UTF8ToWString(mScenePath)).lexically_relative(ActiveProject.project->Root()).generic_wstring());
		}
		else
		{
			DX12Core::gCommandManager.IdleGPU();
			s.SetName(SceneSerializer::FromXml(s.GetWorld(), mPlayXml));
			mDocument.SyncRestoredState(s.GetWorld());
			mPlaying = false;
			mPaused = false;
			mStep = false;
			if (mEditCamera)
			{
				s.GetCamera() = *mEditCamera;
				mEditCamera.reset();
			}
			Select(s.GetWorld(), EditorActions::Find(s.GetWorld(), mPlaySelection));
			mPlayXml.clear();
			ActiveProject.pendingScene.reset();
			if (ActiveProject.project) ActiveProject.currentScene = WStringToUTF8(std::filesystem::absolute(UTF8ToWString(mScenePath)).lexically_relative(ActiveProject.project->Root()).generic_wstring());
		}
	}
	void EditorLayer::Update(Scene& s)
	{
		PollBackgroundWork(s);
		mStep = false;
		if (!mDocumentReady)
		{
			mDocument.Reset(s.GetWorld(), s.GetName());
			mDocumentReady = true;
		}
		auto& w = s.GetWorld();
		if (mWorld.lock() != w.GetLifetime().lock() || !w.IsValid(mSelected))
			Select(w, entt::null);
		Workspace(s);
        mProjectSettingsPanel.Draw(*this, s);
        mProjectPanel.Draw(*this, s);
		if (mPlaying && mMaximizeOnPlay)
		{
			PlayViewport(s);
			ConfirmPending(s);
			mTextInput = ImGui::GetIO().WantTextInput;
			return;
		}
		mConsolePanel.Draw(*this, s);
		Hierarchy(w);
		Inspector(s);
		if (mShowAssets)
			Assets(s);
		if (mShowMaterials)
			Materials(s);
		Viewport(s);
		mShadersPanel.Draw(*this, s);
		mHelpPanel.Draw(*this, s);
		mSceneSettingsPanel.Draw(*this, s);
		if (ImGui::IsAnyItemActive() || ImGuizmo::IsUsing() || ImGui::IsMouseClicked(0) || ImGui::IsMouseClicked(1) ||
			ImGui::GetIO().KeyCtrl || ImGui::IsKeyPressed(ImGuiKey_Delete) || ImGui::IsKeyPressed(ImGuiKey_Enter) ||
			ImGui::IsKeyPressed(ImGuiKey_Space))
			mHistoryPending = true;
		if (!mPlaying && mHistoryPending && !ImGui::IsAnyItemActive() && !ImGuizmo::IsUsing())
		{
			Capture(s);
			mHistoryPending = false;
		}
		ConfirmPending(s);
		mTextInput = ImGui::GetIO().WantTextInput;
	}



void EditorLayer::Hierarchy(World& w){ mHierarchyPanel.Draw(*this, w); }
void EditorLayer::Inspector(Scene& s){ mInspectorPanel.Draw(*this, s); }
void EditorLayer::SceneObjectInspector(Scene &scene){ mInspectorPanel.DrawSceneComponents(*this, scene); }
void EditorLayer::Viewport(Scene &s){ mViewportPanel.Draw(*this, s); }
void EditorLayer::PlayViewport(Scene &scene){ mViewportPanel.DrawPlay(*this, scene); }
void EditorLayer::Assets(Scene &s){ mAssetsPanel.Draw(*this, s); }
void EditorLayer::Materials(Scene& scene){ mMaterialsPanel.Draw(*this, scene); }
void EditorLayer::Workspace(Scene &s){ mWorkspacePanel.Draw(*this, s); }
void EditorLayer::PlayToolbar(Scene &scene){ mToolbarPanel.Draw(*this, scene); }
void EditorLayer::ScriptCreation(World &world){ mScriptsPanel.Draw(*this, world); }
void EditorLayer::ScriptFields(World &world,ScriptAttachment &s,const std::string &live){ mScriptsPanel.DrawFields(*this,world,s,live); }
} // namespace AtomEngine

#include "ScriptFieldEditor.h"
