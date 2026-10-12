#include "GameScene.h"
#include "imgui.h"
#include "Runtime/Function/Global/GlobalContext.h"
#include "Runtime/Resource/AssetManager.h"
#include "Runtime/Function/Light/LightManager.h"

#include "Runtime/Function/Framework/Component/TransformComponent.h"
#include "Runtime/Function/Framework/Component/MaterialComponent.h"
#include "Runtime/Function/Framework/Component/MeshComponent.h"
#include "Runtime/Function/Render/Primitive.h"
#include "Runtime/Core/Color/Color.h"

namespace
{
	Entity helmatEntity = entt::null;
	Radian helmatRotation = Radian(0.0f);

	Entity bunnyEntity = entt::null;
	Radian bunnyRotation = Radian(0.0f);

	bool lightMotionEnabled = true;
}

GameScene::GameScene(std::string_view name, SceneManager& manager)
	: Scene(name, manager)
{
}

bool GameScene::Initialize()
{
	mCamera = &mGameCamera;
	mGameCamera.SetPosition({ 0.0f, 0.0f, -20.0f });

	// デバッグ表示用の独立したカメラを用意し、それを DebugCamera に渡す
	mDebugViewCamera.SetPosition({ 0.0f, 50.0f, -100.0f });
	mDebugViewCamera.SetZRange(0.1f, 10000.0f);
	mDebugCamera.reset(new DebugCamera(mDebugViewCamera));

	auto sponza = AssetManager::LoadModel("Asset/Models/Sponza/Sponza.gltf");

	auto obj = mWorld.CreateGameObject("sponza");
	obj->AddComponent<MeshComponent>(sponza);
	obj->AddComponent<MaterialComponent>(sponza);
	obj->AddComponent<TransformComponent>();
	obj->GetComponent<TransformComponent>().SetScale(100.0f);

	auto helmat = AssetManager::LoadModel("Asset/Models/DamagedHelmet/DamagedHelmet.gltf");

	auto obj1 = mWorld.CreateGameObject("DamagedHelmet");
	obj1->AddComponent<MeshComponent>(helmat);
	obj1->AddComponent<MaterialComponent>(helmat);
	obj1->AddComponent<TransformComponent>();
	auto& transform1 = obj1->GetComponent<TransformComponent>();
	transform1.SetScale(100.0f);
	transform1.transition.y += 100.0f;
	transform1.transition.z += 30.0f;

	helmatEntity = obj1->GetHandle();

	auto suzanne = AssetManager::LoadModel("Asset/Models/suzanne/suzanne.gltf");

	auto obj2 = mWorld.CreateGameObject("suzanne");
	obj2->AddComponent<MeshComponent>(suzanne);
	obj2->AddComponent<MaterialComponent>(suzanne);
	obj2->AddComponent<TransformComponent>();
	auto& transform2 = obj2->GetComponent<TransformComponent>();
	transform2.SetScale(100.0f);
	transform2.transition.x += 300.0f;
	transform2.transition.y += 100.0f;
	bunnyEntity = obj2->GetHandle();

	auto Chimera = AssetManager::LoadModel("Asset/Models/Chimera/Chimera.gltf");
	auto obj3 = mWorld.CreateGameObject("Chimera");
	obj3->AddComponent<MeshComponent>(Chimera);
	obj3->AddComponent<MaterialComponent>(Chimera);
	obj3->AddComponent<TransformComponent>();
	auto& transform3 = obj3->GetComponent<TransformComponent>();
	transform3.SetScale(200.0f);
	transform3.transition.x -= 200.0f;
	transform3.transition.z += 30.0f;
	transform3.rotation.FromAngleAxis(Radian(Math::HalfPI), Vector3::UP);
	LightManager::CreateRandomLights(sponza->mBoundingBox.GetMin() * 100, sponza->mBoundingBox.GetMax() * 100);

	mGameCamera.SetZRange(0.1f, 10000.0f);
	mGameCamera.Update();
	return Scene::Initialize();
}

void GameScene::Update(float deltaTime)
{
#ifndef RELEASE
	gContext.imgui->ShowPerformanceWindow(deltaTime);
#endif
	LightManager::UpdateRandomMotion(deltaTime);
	if (mIsModelRotating)
	{
		helmatRotation += Radian(Math::PIDiv4 * deltaTime);
		mWorld.GetComponent<TransformComponent>(helmatEntity).rotation.FromAngleAxis(helmatRotation, Vector3::UP);
		bunnyRotation += Radian(Math::HalfPI * deltaTime);
		mWorld.GetComponent<TransformComponent>(bunnyEntity).rotation.FromAngleAxis(bunnyRotation, Vector3::UP);
	}

	ImGui::Begin("Camera");
	ImGui::Checkbox("Use Debug Camera", &mUseDebugCamera);
	ImGui::End();
	ImGui::Begin("Light");
	ImGui::Checkbox("Enable Light Motion", &lightMotionEnabled);
	ImGui::End();
	ImGui::Begin("Model Rotation");
	ImGui::Checkbox("Enable Model Rotation", &mIsModelRotating);
	ImGui::End();

	LightManager::EnableRandomMotion(lightMotionEnabled);

	if (mUseDebugCamera)
	{
		// デバッグカメラが独立カメラを操作する -> それを現在の描画カメラにする
		mDebugCamera->Update(deltaTime);
		mCamera = &mDebugViewCamera;
	}
	else
	{
		mGameCamera.Update();
		mCamera = &mGameCamera;
	}

//#ifndef RELEASE
	ImGuiHandleObjects();
//#endif
	DestroyGameObject();
}

void GameScene::Render()
{
	// デバッグモードなら、デバッグカメラの視点で主カメラのワールド視錐体を描画
	//if (mUseDebugCamera)
	//{
	//	const Frustum& mainFrustumWS = mGameCamera.GetWorldSpaceFrustum();
	//	mainFrustumWS.DebugDraw(mCamera->GetViewProjMatrix(), Color::Cyan);
	//}
}

void GameScene::DestroyGameObject()
{
	auto view = mWorld.View<DeathFlag>();
	view.each([&](auto entity, DeathFlag& flag)
		{
			if (flag.isDead)
			{
				mWorld.DestroyGameObject(entity);
			}
		});
}

void GameScene::Shutdown()
{

}

bool GameScene::Exit()
{
	return false;
}

void GameScene::ImGuiHandleObjects()
{
	ImGui::Begin("Objects");
	int index = 0;
	auto view = mWorld.View<TransformComponent, MaterialComponent, NameComponent>();

	view.each([&](auto entity, TransformComponent& transform, MaterialComponent& material, NameComponent& name)
		{
			std::string entityID = " EntityID:" + std::to_string((uint32_t)entity);

			if (ImGui::CollapsingHeader((name.value + entityID).c_str(), ImGuiTreeNodeFlags_None))
			{
				ImGui::PushID(index);

				if (ImGui::TreeNodeEx("Transform", ImGuiTreeNodeFlags_None))
				{
					ImGui::DragFloat3("Position##A", transform.transition.ptr(), 0.01f);
					ImGui::DragFloat4("Rotation##A", transform.rotation.ptr(), 0.01f);
					ImGui::DragFloat3("Scale##A", transform.scale.ptr(), 0.01f);
					transform.rotation.Normalize();

					ImGui::TreePop();
				}

				if (ImGui::TreeNode("Materials"))
				{
					for (int matIndex = 0; matIndex < material.mMaterials.size(); matIndex++)
					{
						auto& mat = material.mMaterials[matIndex];
						std::string matLabel = "Material " + std::to_string(matIndex);
						if (ImGui::TreeNode(matLabel.c_str()))
						{
							ImGui::PushID(matIndex);
							ImGui::ColorEdit3("Base Color##A", mat.mBaseColor.ptr());
							ImGui::ColorEdit3("Emissive##A", &mat.mEmissive.x);
							ImGui::DragFloat("Metallic##A", &mat.mMetallic, 0.01f, 0.0f, 1.0f);
							ImGui::DragFloat("Roughness##A", &mat.mRoughness, 0.01f, 0.0f, 1.0f);
							ImGui::DragFloat("Normal Scale##A", &mat.mNormalScale, 0.01f, 0.0f, 10.0f);

							ImGui::PopID();
							ImGui::TreePop();
						}
					}
					ImGui::TreePop();
				}
				index++;
				ImGui::PopID();
			}
		});
	ImGui::End();

}