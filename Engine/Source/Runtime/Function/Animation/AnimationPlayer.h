#pragma once
#include "Runtime/Resource/Model.h"
namespace AtomEngine {
// Playback state and pose belong to an instance; the cached model remains immutable.
class AnimationPlayer {
public:
    void Initialize(const std::shared_ptr<Model>& model);
    void UpdateAnimation(float deltaTime);
    void PlayAnimation(uint32_t index, bool loop);
    void PauseAnimation(uint32_t index);
    void ResetAnimation(uint32_t index);
    void StopAnimation(uint32_t index);
    void LoopAllAnimations();
    uint32_t GetNumAnimations() const { return static_cast<uint32_t>(mAnimState.size()); }
    void SetDeltaScale(float scale) { mDeltaScale = scale; }
    Skeleton& GetSkeleton() { return mSkeleton; }
private:
    std::shared_ptr<const Model> mModel;
    Skeleton mSkeleton;
    std::vector<AnimationState> mAnimState;
    float mDeltaScale = 1.0f;
};
}
