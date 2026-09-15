#include "Vector2F.h"
#include "Vector2.h"
#include "../Utility/Utility.h"

Vector2F::Vector2F(void)
{
	x = 0.0f;
	y = 0.0f;
}

Vector2F::Vector2F(float vX, float vY)
{
	x = vX;
	y = vY;
}

Vector2F::Vector2F(Vector2 value)
{
	x = static_cast<float>(value.x);
	y = static_cast<float>(value.y);
}

Vector2 Vector2F::ToVector2(void)
{
	Vector2 ret;
	ret.x = Utility::Round(x);
	ret.y = Utility::Round(y);
	return ret;
}
