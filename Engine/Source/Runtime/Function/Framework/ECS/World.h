/**
 * @file World.h
 * @brief ECSワールドの管理クラス
 *
 * エンティティとコンポーネントの生成・破棄・管理、
 * およびイベントディスパッチャーを提供する。
 */

#pragma once
#include "ECSCommon.h"
#include "Runtime/Function/Framework/Component/SceneComponents.h"

namespace AtomEngine
{
class GameObject;

using ComponentCallback = std::function<void(Entity, std::type_index)>;

/**
 * @struct ComponentAddedEvent
 * @brief コンポーネント追加イベント
 */
struct ComponentAddedEvent
{
    Entity entity;        ///< 対象エンティティ
    std::type_index type; ///< コンポーネントの型情報
};

/**
 * @struct ComponentRemovedEvent
 * @brief コンポーネント削除イベント
 */
struct ComponentRemovedEvent
{
    Entity entity;        ///< 対象エンティティ
    std::type_index type; ///< コンポーネントの型情報
};

/**
 * @class World
 * @brief ECSワールドの中核クラス
 *
 * エンティティ・コンポーネント・システム（ECS）アーキテクチャの
 * 中心となるワールド管理クラス。全てのゲームオブジェクトとコンポーネントを管理する。
 *
 * 主な機能:
 * - ゲームオブジェクトの生成・破棄
 * - コンポーネントの追加・削除・取得
 * - イベントディスパッチ
 * - コンポーネントビューの提供
 */
class World
{
  public:
    World();
    SceneSettings settings = ProjectSceneDefaults;
    std::string skyTextureError; // 渲染诊断，不保存到场景或撤销历史。
    Vector2 uiViewportSize{1920,1080}, uiRenderSize{1920,1080};
    Entity uiFocus=entt::null;
    Entity FindById(const std::string &id);
    bool IsActive(Entity entity);
    bool IsEnabled(Entity entity,ComponentKind kind);
    void SetComponentEnabled(Entity entity,ComponentKind kind,bool enabled);
    void ReplaceWith(World &source);
    ~World();

    /**
     * @brief ゲームオブジェクトを生成
     * @param name オブジェクト名（オプション）
     * @return 生成されたゲームオブジェクト
     */
    std::unique_ptr<GameObject> CreateGameObject(const std::string &name = "");

    /**
     * @brief エンティティからゲームオブジェクトを取得
     * @param entity エンティティID
     * @return ゲームオブジェクトのポインタ
     */
    std::unique_ptr<GameObject> GetGameObject(Entity entity);
    std::weak_ptr<void> GetLifetime() const
    {
        return mLifetime;
    }
    bool IsValid(Entity entity) const
    {
        return mRegistry.valid(entity);
    }

    /**
     * @brief ゲームオブジェクトを破棄
     * @param entity 破棄するエンティティ
     */
    void DestroyGameObject(Entity entity);

    /**
     * @brief 名前でエンティティを検索
     * @param name 検索する名前
     * @return 見つかったエンティティ（なければnull）
     */
    Entity FindByName(const std::string &name);

    /**
     * @brief コンポーネントを追加
     * @tparam T コンポーネントの型
     * @tparam Args コンストラクタ引数の型
     * @param e エンティティ
     * @param args コンストラクタ引数
     */
    template <typename T, typename... Args> void AddComponent(Entity e, Args &&...args)
    {
        mRegistry.emplace<T>(e, std::forward<Args>(args)...);
        mDispatcher.trigger(ComponentAddedEvent{e, std::type_index(typeid(T))});
    }

    /**
     * @brief コンポーネントの有無を判定
     * @tparam T コンポーネントの型
     * @param e エンティティ
     * @return コンポーネントが存在すればtrue
     */
    template <typename T> bool HasComponent(Entity e) const
    {
        return mRegistry.any_of<T>(e);
    }

    /**
     * @brief コンポーネントを取得
     * @tparam T コンポーネントの型
     * @param e エンティティ
     * @return コンポーネントへの参照
     */
    template <typename T> T &GetComponent(Entity e)
    {
        return mRegistry.get<T>(e);
    }

    /**
     * @brief コンポーネントを削除
     * @tparam T コンポーネントの型
     * @param e エンティティ
     */
    template <typename T> void RemoveComponent(Entity e)
    {
        if (!mRegistry.valid(e) || !mRegistry.remove<T>(e))
            return;
        mDispatcher.trigger(ComponentRemovedEvent{e, std::type_index(typeid(T))});
    }

    /**
     * @brief 指定コンポーネントを持つエンティティのビューを取得
     * @tparam Components ビュー対象のコンポーネント型
     * @return エンティティビュー
     */
    template <typename... Components> decltype(auto) View()
    {
        return mRegistry.view<Components...>();
    }

    /**
     * @brief コンポーネントを安全に取得（nullableポインタ）
     * @tparam Components コンポーネント型
     * @param e エンティティ
     * @return コンポーネントへのポインタ（なければnullptr）
     */
    template <typename... Components> decltype(auto) TryGet(Entity e)
    {
        return mRegistry.try_get<Components...>(e);
    }

    /**
     * @brief コンポーネント追加時のコールバックを設定
     * @param cb コールバック関数
     */
    void SetOnComponentAdded(ComponentCallback cb)
    {
        mOnComponentAdded = std::move(cb);
    }

    /**
     * @brief コンポーネント削除時のコールバックを設定
     * @param cb コールバック関数
     */
    void SetOnComponentRemoved(ComponentCallback cb)
    {
        mOnComponentRemoved = std::move(cb);
    }

    /**
     * @brief イベントディスパッチャーを取得
     * @return ディスパッチャーへの参照
     */
    entt::dispatcher &GetDispatcher()
    {
        return mDispatcher;
    }

    /**
     * @brief レジストリを取得
     * @return レジストリへの参照
     */
    const entt::registry &GetRegistry() const
    {
        return mRegistry;
    }

    /**
     * @brief エンティティに名前を設定
     * @param entity エンティティ
     * @param name 設定する名前
     */
    void SetName(Entity entity, const std::string &name);

  private:
    std::map<std::string,Entity> mPersistentIndex;
    std::shared_ptr<void> mLifetime = std::make_shared<int>(0);
    entt::registry mRegistry;     ///< enttレジストリ
    entt::dispatcher mDispatcher; ///< イベントディスパッチャー

    ComponentCallback mOnComponentAdded = nullptr;   ///< コンポーネント追加コールバック
    ComponentCallback mOnComponentRemoved = nullptr; ///< コンポーネント削除コールバック

  private:
    /**
     * @brief コンポーネント追加イベントハンドラ
     * @param e イベントデータ
     */
    void OnComponentAdded(const ComponentAddedEvent &e);

    /**
     * @brief コンポーネント削除イベントハンドラ
     * @param e イベントデータ
     */
    void OnComponentRemoved(const ComponentRemovedEvent &e);
};
} // namespace AtomEngine
