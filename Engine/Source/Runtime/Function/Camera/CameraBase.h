/**
 * @file CameraBase.h
 * @brief カメラの基底クラスと透視投影カメラ
 * 
 * ビュー行列・プロジェクション行列の管理、視錐台カリングなどを提供する。
 */

#pragma once
#include "Runtime/Core/Math/MathInclude.h"
#include "Runtime/Core/Math/Frustum.h"
#include <functional>


namespace AtomEngine
{
	/**
	 * @class CameraBase
	 * @brief カメラの基底クラス
	 * 
	 * ビュー行列・プロジェクション行列の計算、視錐台管理などの
	 * カメラの基本機能を提供する抽象基底クラス。
	 */
	class CameraBase
	{
	public:
		/**
		 * @brief カメラの更新処理
		 * 
		 * ビュー行列、ビュープロジェクション行列、視錐台を更新する。
		 * 毎フレーム呼び出す必要がある。
		 */
		void Update();
		
		/**
		 * @brief LookAt方式でカメラを設定
		 * @param eye カメラの位置
		 * @param lookAt 注視点
		 * @param up 上方向ベクトル
		 */
		void SetLookAt(const Vector3& eye, const Vector3& lookAt, const Vector3& up);
		
		/**
		 * @brief 前方向ベクトルでカメラの向きを設定
		 * @param forward 前方向ベクトル
		 * @param up 上方向ベクトル
		 */
		void SetLookDirection(const Vector3& forward, const Vector3& up = Vector3{0.0f,1.0f,0.0f});
		
		/**
		 * @brief 回転をクォータニオンで設定
		 * @param basisRotation 回転クォータニオン
		 */
		void SetRotation(const Quaternion& basisRotation);
		
		/**
		 * @brief 回転をオイラー角で設定
		 * @param rotation 回転ベクトル（ラジアン）
		 */
		void SetRotation(const Vector3& rotation);
		
		/**
		 * @brief カメラ位置を設定
		 * @param worldPos ワールド座標での位置
		 */
		void SetPosition(const Vector3& worldPos);
		
		/**
		 * @brief トランスフォームから設定
		 * @param transform トランスフォーム
		 */
		void SetTransform(const Transform& transform);

		/**
		 * @brief 右方向ベクトルを取得
		 * @return 右方向ベクトル
		 */
		const Vector3 GetRightVec() const { return mBasis.GetX(); }
		
		/**
		 * @brief 上方向ベクトルを取得
		 * @return 上方向ベクトル
		 */
		const Vector3 GetUpVec() const { return mBasis.GetY(); }
		
		/**
		 * @brief 前方向ベクトルを取得
		 * @return 前方向ベクトル
		 */
		const Vector3 GetForwardVec() const { return mBasis.GetZ(); }

		/**
		 * @brief カメラの回転を取得
		 * @return 回転クォータニオン
		 */
		const Quaternion& GetRotation() const { return mCameraToWorld.rotation; }
		
		/**
		 * @brief カメラの位置を取得
		 * @return ワールド座標での位置
		 */
		const Vector3& GetPosition() const { return mCameraToWorld.transition; }

		/**
		 * @brief ビュー行列を取得
		 * @return ビュー行列（World → View）
		 */
		const Matrix4x4& GetViewMatrix() const { return mViewMatrix; }
		
		/**
		 * @brief プロジェクション行列を取得
		 * @return プロジェクション行列（View → Projection）
		 */
		const Matrix4x4& GetProjMatrix() const { return mProjMatrix; }
		
		/**
		 * @brief ビュープロジェクション行列を取得
		 * @return ビュープロジェクション行列（World → Projection）
		 */
		const Matrix4x4& GetViewProjMatrix() const { return mViewProjMatrix; }
		
		/**
		 * @brief リプロジェクション行列を取得（モーションベクトル用）
		 * @return リプロジェクション行列
		 */
		const Matrix4x4& GetReprojectionMatrix() const { return mReprojectMatrix; }
		
		/**
		 * @brief ビュー空間の視錐台を取得
		 * @return ビュー空間視錐台
		 */
		const Frustum& GetViewSpaceFrustum() const { return mFrustumVS; }
		
		/**
		 * @brief ワールド空間の視錐台を取得
		 * @return ワールド空間視錐台
		 */
		const Frustum& GetWorldSpaceFrustum() const { return mFrustumWS; }

	protected:
		/**
		 * @brief プロジェクション行列を設定（派生クラス用）
		 * @param ProjMat プロジェクション行列
		 */
		void SetProjMatrix(const Matrix4x4& ProjMat) { mProjMatrix = ProjMat; }

		Transform mCameraToWorld;  ///< カメラのワールドトランスフォーム

		Matrix4x4 mBasis;  ///< カメラの基底ベクトル

		Matrix4x4 mViewMatrix;		///< ビュー行列（World → View）
		Matrix4x4 mProjMatrix;		///< プロジェクション行列（View → Projection）
		Matrix4x4 mViewProjMatrix;	///< ビュープロジェクション行列（World → Projection）

		Matrix4x4 mPreviousViewProjMatrix;  ///< 前フレームのビュープロジェクション行列
		Matrix4x4 mReprojectMatrix;         ///< リプロジェクション行列

		Frustum mFrustumVS;		///< ビュー空間の視錐台
		Frustum mFrustumWS;		///< ワールド空間の視錐台
	};

	/**
	 * @class Camera
	 * @brief 透視投影カメラクラス
	 * 
	 * 視野角（FOV）、アスペクト比、ニア/ファークリップ平面を持つ
	 * 標準的な3D透視投影カメラ。
	 */
	class Camera : public CameraBase
	{
	public:
		Camera();

		/**
		 * @brief 透視投影行列を設定
		 * @param verticalFovRadians 垂直視野角（ラジアン）
		 * @param aspectHeightOverWidth アスペクト比（高さ/幅）
		 * @param nearZClip ニアクリップ平面
		 * @param farZClip ファークリップ平面
		 */
		void SetPerspectiveMatrix(float verticalFovRadians, float aspectHeightOverWidth, float nearZClip, float farZClip);
		
		/**
		 * @brief 視野角を設定
		 * @param verticalFovInRadians 垂直視野角（ラジアン）
		 */
		void SetFOV(float verticalFovInRadians) { mVerticalFOV = verticalFovInRadians; UpdateProjMatrix(); }
		
		/**
		 * @brief アスペクト比を設定
		 * @param heightOverWidth アスペクト比（高さ/幅）
		 */
		void SetAspectRatio(float heightOverWidth) { mAspectRatio = heightOverWidth; UpdateProjMatrix(); }
		
		/**
		 * @brief Z範囲を設定
		 * @param nearZ ニアクリップ距離
		 * @param farZ ファークリップ距離
		 */
		void SetZRange(float nearZ, float farZ) { mNearClip = nearZ; mFarClip = farZ; UpdateProjMatrix(); }
		
		/**
		 * @brief リバースZ（深度反転）の有効化
		 * @param enable 有効にするならtrue
		 */
		void ReverseZ(bool enable) { mReverseZ = enable; UpdateProjMatrix(); }

		/**
		 * @brief 視野角を取得
		 * @return 垂直視野角（ラジアン）
		 */
		float GetFOV() const { return mVerticalFOV; }
		
		/**
		 * @brief ニアクリップ距離を取得
		 * @return ニアクリップ距離
		 */
		float GetNearClip() const { return mNearClip; }
		
		/**
		 * @brief ファークリップ距離を取得
		 * @return ファークリップ距離
		 */
		float GetFarClip() const { return mFarClip; }
		
		/**
		 * @brief デプスバッファのクリア値を取得
		 * @return クリア値（ReverseZなら0.0、通常なら1.0）
		 */
		float GetClearDepth() const { return mReverseZ ? 0.0f : 1.0f; }
		
	private:
		/**
		 * @brief プロジェクション行列を更新
		 */
		void UpdateProjMatrix(void);
		
		float mVerticalFOV;    ///< 垂直視野角
		float mAspectRatio;    ///< アスペクト比
		float mNearClip;       ///< ニアクリップ距離
		float mFarClip;      ///< ファークリップ距離
		bool mReverseZ;        ///< リバースZ有効フラグ
		bool mInfiniteZ;       ///< 無限遠Z有効フラグ
	};
}


