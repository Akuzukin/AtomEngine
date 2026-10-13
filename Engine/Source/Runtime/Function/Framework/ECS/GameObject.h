/**
 * @file GameObject.h
 * @brief ゲームオブジェクトのラッパークラス
 *
 * EnTTエンティティをラップし、より直感的なAPIを提供する。
 */

#pragma once
#include "ECSCommon.h"
namespace AtomEngine
{
	class World;

	/**
	 * @class GameObject
	 * @brief ゲームオブジェクトクラス
	 *
	 * ECSのエンティティをラップし、コンポーネントの追加・削除・取得などの
	 * 操作を簡潔に行えるようにする。
	 *
	 * 使用例:
	 * @code
	 * auto obj = world.CreateGameObject("Player");
	 * obj->AddComponent<TransformComponent>(Vector3::ZERO);
	 * obj->AddComponent<VelocityComponent>();
	 * @endcode
	 */
	class GameObject
	{
	public:
		/**
		 * @brief コンストラクタ
		 * @param handle エンティティハンドル
		 * @param world ワールドへのポインタ
		 */
		GameObject(Entity handle, World* world);
		~GameObject()= default;

		operator Entity() const { return mHandle; }
		operator bool() const { return IsValid(); }

		/**
		 * @brief ゲームオブジェクトが有効か判定
		 * @return 有効ならtrue
		 */
		bool IsValid() const;

		/**
		 * @brief エンティティハンドルを取得
		 * @return エンティティハンドル
		 */
		Entity GetHandle() const { return mHandle; }

		/**
		 * @brief 新しいコンポーネントを追加する
		 * @tparam T コンポーネント型
		 * @tparam Args コンストラクタ引数の型
		 * @param args コンストラクタ引数
		 */
		template<typename T, typename... Args>
		void AddComponent(Args&&... args);

		/**
		 * @brief コンポーネントの有無を確認
		 * @tparam T コンポーネント型
		 * @return コンポーネントが存在すればtrue
		 */
		template<typename T>
		bool HasComponent() const;

		/**
		 * @brief 指定したコンポーネントを取得
		 * @tparam T コンポーネント型
		 * @return コンポーネントへの参照
		 */
		template<typename T>
		T& GetComponent();

		/**
		 * @brief 指定したコンポーネントを削除する
		 * @tparam T コンポーネント型
		 */
		template<typename T>
		void RemoveComponent();

		/**
		 * @brief このゲームオブジェクトを破棄
		 */
		void Destroy();

		/**
		 * @brief オブジェクトの名前を取得
		 * @return オブジェクト名
		 */
		std::string GetName();

		/**
		 * @brief オブジェクトに名前を設定
		 * @param name 設定する名前
		 */
		void SetName(const std::string& name);

		bool operator==(const GameObject& other) const { return mWorld == other.mWorld && mHandle == other.mHandle; }
		bool operator!=(const GameObject& other) const { return !(*this == other); }

	private:
		Entity mHandle{ entt::null };  ///< エンティティハンドル
		std::weak_ptr<void> mWorldLifetime;
		World* mWorld = nullptr;     ///< ワールドへのポインタ
	};
}

#include "GameObject.inl"
