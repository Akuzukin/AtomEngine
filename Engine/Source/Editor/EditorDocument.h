#pragma once
#include "Runtime/Function/Camera/CameraBase.h"
#include "Runtime/Function/Scene/SceneSerializer.h"
namespace AtomEngine
{
	class EditorDocument
	{
	public:
		void Reset(World& world, const std::string& name);
        void MarkUnsaved(){mSavedId=~uint64_t(0);}
		bool Capture(World& world, const std::string& name, const std::string& selection,
			const std::string& label = "Edit properties");
		void MarkSaved(World& world, const std::string& name);
		void SyncRestoredState(World& world);
		bool Dirty() const
		{
			return !mStates.empty() && mStates[mCursor].id != mSavedId;
		}
		bool CanUndo() const
		{
			return mCursor > 0;
		}
		bool CanRedo() const
		{
			return mCursor + 1 < mStates.size();
		}
		std::string Undo(World& world);
		std::string Redo(World& world);
		const std::string& UndoLabel() const;
		const std::string& SceneName() const
		{
			return mName;
		}
		size_t Size() const
		{
			return mStates.size();
		}

	private:
		struct State
		{
			std::string xml, selection, label;
			uint64_t id;
		};
		std::vector<State> mStates;
		size_t mCursor = 0;
		std::string mName;
		uint64_t mSavedId = 0, mNextId = 0;
	};
	class EditorActions
	{
	public:
		static Entity Find(World& world, const std::string& id);
		static std::string Id(World& world, Entity entity);
		static void DeleteTree(World& world, Entity root);
		static void SetWorldMatrix(World& world, Entity entity, const Matrix4x4& matrix);
		static void Reparent(World& world, Entity child, Entity parent);
		static Vector3 FrameSelection(World& world, Entity entity, Camera& camera);
	};
} // namespace AtomEngine
