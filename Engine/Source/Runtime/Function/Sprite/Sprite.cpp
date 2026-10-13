#include "Sprite.h"
#include "../Render/SpriteRenderer.h"

namespace AtomEngine
{
	Sprite::Sprite(TextureRef texture)
	{
		mTexture = texture;
	}

	void Sprite::Render(const Vector2& position, const Vector2& scale, float angle, const Vector2& pivot, const Color& color,uint32_t layer)
	{
		Render(position, scale, angle, mTexture, pivot, color,layer);
	}

	void Sprite::Render(const Vector2& position, const Vector2& scale, float angle, TextureRef texture, const Vector2& pivot, const Color& color,uint32_t layer)
	{
		auto size = Vector2((float)mTexture->GetWidth(), (float)mTexture->GetHeight());
		mDesc.color = color;
		mDesc.position = position;
		mDesc.size = size;
        mDesc.texBase = Vector2(0.0f, 0.0f);
        mDesc.texSize = size;
	}

	void Sprite::Render()
	{
		mDesc.size = Vector2((float)mTexture->GetWidth(), (float)mTexture->GetHeight());
		mDesc.texSize = Vector2((float)mTexture->GetWidth(), (float)mTexture->GetHeight());
		SpriteRenderer::AddSprite(this, mTexture, mBlendMode);
	}
}
