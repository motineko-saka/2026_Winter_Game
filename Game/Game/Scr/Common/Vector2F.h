#pragma once
// ポインタ渡しの前に、Vector2とVector2Fの循環参照を見せること
class Vector2;

class Vector2F
{
public:

	float x;
	float y;

	// デフォルトコンストラクタ
	Vector2F(void);

	// コンストラクタ
	Vector2F(float vX, float vY);

	// コンストラクタ
	Vector2F(Vector2 value);

	// Vector2 へ変換
	Vector2 ToVector2(void);

};

