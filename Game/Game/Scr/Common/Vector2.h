#pragma once
class Vector2F;

class Vector2
{
public:

	int x;
	int y;

	// デフォルトコンストラクタ
	Vector2(void);

	// コンストラクタ
	Vector2(int vX, int vY);

	// コンストラクタ
	Vector2(Vector2F value);

	// Vector2F へ変換
	Vector2F ToVector2F(void);

};
