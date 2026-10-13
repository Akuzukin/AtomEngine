#include "AnimationPlayer.h"
namespace AtomEngine {
void AnimationPlayer::Initialize(const std::shared_ptr<Model>& model) {
    mModel = model;
    mSkeleton = model ? model->mSkeleton : Skeleton{};
    mAnimState.clear();
    if (model) mAnimState.resize(model->mAnimationData.size());
}
	void AnimationPlayer::UpdateAnimation(float deltaTime)
	{
		for (uint32_t i = 0; i < mAnimState.size(); ++i)
		{
			AnimationState& animState = mAnimState[i];
			if (animState.state == AnimationState::kStopped)
				continue;
			animState.time += deltaTime * mDeltaScale;
			const AnimationClip& clip = mModel->mAnimationData[i];
			if (clip.duration <= 0.0f) { animState.state = AnimationState::kStopped; continue; }

			if (animState.state == AnimationState::kLooping)
				animState.time = std::fmodf(animState.time, clip.duration);
			else if (animState.time > clip.duration)
			{
				animState.time = clip.duration;
				animState.state = AnimationState::kStopped;
			}

			auto& time = animState.time;
			for (auto& curve : clip.curves)
			{
				if (curve.targetJoint >= mSkeleton.joints.size()) continue;
			auto& joint = mSkeleton.joints[curve.targetJoint];
				if (!curve.scale.empty()) joint.transform.scale = CalculateValue(curve.scale, time);
				if (!curve.rotation.empty()) joint.transform.rotation = CalculateRotation(curve.rotation, time);
				if (!curve.translation.empty()) joint.transform.transition = CalculateValue(curve.translation, time);
			}
		}
	}
	void AnimationPlayer::PlayAnimation(uint32_t animIdx, bool loop)
	{
		if (animIdx >= mAnimState.size())return;

		for(uint32_t i = 0; i < mAnimState.size() ; i++)
		{
			if (i == animIdx)continue;
			auto& animState = mAnimState[i];
			animState.state = AnimationState::kStopped;
			animState.time = 0.0f;
		}
	
		mAnimState[animIdx].state = loop ? AnimationState::kLooping : AnimationState::kPlaying;
	}

	void AnimationPlayer::PauseAnimation(uint32_t animIdx)
	{
		if (animIdx < mAnimState.size())
		 mAnimState[animIdx].state = AnimationState::kStopped;
	}

	void AnimationPlayer::ResetAnimation(uint32_t animIdx)
	{
		if (animIdx < mAnimState.size())
		 mAnimState[animIdx].time = 0.0f;
	}

	void AnimationPlayer::StopAnimation(uint32_t animIdx)
	{
		if (animIdx < mAnimState.size())
		{
			mAnimState[animIdx].state = AnimationState::kStopped;
			mAnimState[animIdx].time = 0.0f;
		}
	}

	void AnimationPlayer::LoopAllAnimations()
	{
		for (auto& anim : mAnimState)
		{
			anim.state = AnimationState::kLooping;
			anim.time = 0.0f;
		}
	}
}
