#pragma once

#include <DxLib.h>
#include "Vector2.h"
#include "Vector2F.h"

struct Image
{
    int handle = -1;        // 画像ハンドル

    Vector2F pos;           // 座標
    float scale = 1.0f;     // 拡大率
    float angle = 0.0f;     // 角度

    int width = 0;          // 横幅
    int height = 0;         // 縦幅
    
    float alpha = 1.0f;     // 透過率
};

struct Model
{
    int handle = -1;                    // モデルハンドル

    VECTOR scale = { 0.0f,0.0f,0.0f };  // 拡大率
    VECTOR rot = { 0.0f,0.0f,0.0f };    // 回転値
    VECTOR pos = { 0.0f,0.0f,0.0f };    // 座標

    float alpha = 1.0f; // 透過率
};
