#include "Quaternion.h"

#include <math.h>
#include <iostream>
#include <algorithm>

#include "../Utility/Utility.h"

Quaternion::Quaternion(void)
{
    w = 1;
    x = y = z = 0;
}

Quaternion::Quaternion(const VECTOR& rad)
{
    Quaternion q = Euler(rad.x, rad.y, rad.z);
    w = q.w;
    x = q.x;
    y = q.y;
    z = q.z;
}

Quaternion::Quaternion(double ww, double wx, double wy, double wz)
{
    w = ww;
    x = wx;
    y = wy;
    z = wz;
}

Quaternion::~Quaternion(void)
{
}

Quaternion Quaternion::Euler(const VECTOR& rad)
{
    return Euler(rad.x, rad.y, rad.z);
}

Quaternion Quaternion::Euler(double radX, double radY, double radZ)
{
    Quaternion ret = Quaternion();

    radX = Utility::RadIn2PI(radX);
    radY = Utility::RadIn2PI(radY);
    radZ = Utility::RadIn2PI(radZ);

    double cosZ = cos(radZ / 2.0f);
    double sinZ = sin(radZ / 2.0f);
    double cosX = cos(radX / 2.0f);
    double sinX = sin(radX / 2.0f);
    double cosY = cos(radY / 2.0f);
    double sinY = sin(radY / 2.0f);

    //ret.w = cosZ * cosX * cosY + sinZ * sinX * sinY;
    //ret.x = sinZ * cosX * cosY - cosZ * sinX * sinY;
    //ret.y = cosZ * sinX * cosY + sinZ * cosX * sinY;
    //ret.z = cosZ * cosX * sinY - sinZ * sinX * cosY;

    ret.w = cosX * cosY * cosZ + sinX * sinY * sinZ;
    ret.x = sinX * cosY * cosZ + cosX * sinY * sinZ;
    ret.y = cosX * sinY * cosZ - sinX * cosY * sinZ;
    ret.z = cosX * cosY * sinZ - sinX * sinY * cosZ;

    return ret;
}

Quaternion Quaternion::Mult(const Quaternion& q1, const Quaternion& q2)
{
    Quaternion ret = Quaternion();
    double d1, d2, d3, d4;

    // wÇÃåvéZ 
    d1 = q1.w * q2.w;
    d2 = -q1.x * q2.x;
    d3 = -q1.y * q2.y;
    d4 = -q1.z * q2.z;
    ret.w = d1 + d2 + d3 + d4;

    // xÇÃåvéZ 
    d1 = q1.w * q2.x;
    d2 = q2.w * q1.x;
    d3 = q1.y * q2.z;
    d4 = -q1.z * q2.y;
    ret.x = d1 + d2 + d3 + d4;

    // yÇÃåvéZ
    d1 = q1.w * q2.y;
    d2 = q2.w * q1.y;
    d3 = q1.z * q2.x;
    d4 = -q1.x * q2.z;
    ret.y = d1 + d2 + d3 + d4;

    // zÇÃåvéZ
    d1 = q1.w * q2.z;
    d2 = q2.w * q1.z;
    d3 = q1.x * q2.y;
    d4 = -q1.y * q2.x;
    ret.z = d1 + d2 + d3 + d4;

    return ret;
}

Quaternion Quaternion::Mult(const Quaternion& q) const
{
    return Mult(Quaternion(w, x, y, z), q);
}

Quaternion Quaternion::AngleAxis(double rad, VECTOR axis)
{
    Quaternion ret = Quaternion();

    double norm;
    double c, s;

    // UnityÇ…çáÇÌÇπÇÈ
    //ret.w = ret.x = ret.y = ret.z = 0.0;
    ret.w = 1.0;
    ret.x = ret.y = ret.z = 0.0;

    norm = (double)axis.x * (double)axis.x + (double)axis.y * (double)axis.y + (double)axis.z * (double)axis.z;
    if (norm <= 0.0f)
    {
        return ret;
    }

    norm = 1.0 / sqrt(norm);
    axis.x = (float)(axis.x * norm);
    axis.y = (float)(axis.y * norm);
    axis.z = (float)(axis.z * norm);

    c = cos(0.5f * rad);
    s = sin(0.5f * rad);

    ret.w = c;
    ret.x = s * axis.x;
    ret.y = s * axis.y;
    ret.z = s * axis.z;

    return ret;
}

VECTOR Quaternion::PosAxis(const Quaternion& q, VECTOR pos)
{
    // à íuèÓïÒÇ…âÒì]èÓïÒÇîΩâfÇ≥ÇπÇÈ
    // pos' = qÅEposÅEq(-1)
    Quaternion tmp = Quaternion();
    tmp = tmp.Mult(q);
    tmp = tmp.Mult(Quaternion(0.0f, pos.x, pos.y, pos.z));
    tmp = tmp.Mult(q.Inverse());
    return { (float)tmp.x, (float)tmp.y, (float)tmp.z };
}

VECTOR Quaternion::PosAxis(VECTOR pos) const
{
    return PosAxis(Quaternion(w, x, y, z), pos);
}

VECTOR Quaternion::ToEuler(const Quaternion& q)
{
    VECTOR ret;

    double r11 = 2 * (q.x * q.z + q.w * q.y);
    double r12 = q.w * q.w - q.x * q.x - q.y * q.y + q.z * q.z;
    double r21 = -2 * (q.y * q.z - q.w * q.x);
    double r31 = 2 * (q.x * q.y + q.w * q.z);
    double r32 = q.w * q.w - q.x * q.x + q.y * q.y - q.z * q.z;

    ret.x = static_cast<float>(asin(r21));
    ret.y = static_cast<float>(atan2(r11, r12));
    ret.z = static_cast<float>(atan2(r31, r32));

    return ret;
}

VECTOR Quaternion::ToEuler(void) const
{
    return ToEuler(Quaternion(w, x, y, z));
}

MATRIX Quaternion::ToMatrix(const Quaternion& q)
{
    MATRIX mat;

    FLOAT4 fq = { (float)q.x, (float)q.y, (float)q.z, (float)q.w };

    float sx = fq.x * fq.x * 2.0f;
    float sy = fq.y * fq.y * 2.0f;
    float sz = fq.z * fq.z * 2.0f;
    float cx = fq.y * fq.z * 2.0f;
    float cy = fq.x * fq.z * 2.0f;
    float cz = fq.x * fq.y * 2.0f;
    float wx = fq.w * fq.x * 2.0f;
    float wy = fq.w * fq.y * 2.0f;
    float wz = fq.w * fq.z * 2.0f;

    mat.m[0][0] = 1.0f - (sy + sz);	mat.m[0][1] = cz + wz;			mat.m[0][2] = cy - wy;			mat.m[0][3] = 0.0f;
    mat.m[1][0] = cz - wz;			mat.m[1][1] = 1.0f - (sx + sz);	mat.m[1][2] = cx + wx;			mat.m[1][3] = 0.0f;
    mat.m[2][0] = cy + wy;			mat.m[2][1] = cx - wx;			mat.m[2][2] = 1.0f - (sx + sy);	mat.m[2][3] = 0.0f;
    mat.m[3][0] = 0.0f;				mat.m[3][1] = 0.0f;				mat.m[3][2] = 0.0f;				mat.m[3][3] = 1.0f;
    //mat.m[3][0] = trans.x;				mat.m[3][1] = trans.y;				mat.m[3][2] = trans.z;				mat.m[3][3] = 1.0f;

    return mat;

    //double sqw = q.w * q.w;
    //double sqx = q.x * q.x;
    //double sqy = q.y * q.y;
    //double sqz = q.z * q.z;
    //double invs = 1.0 / (sqx + sqy + sqz + sqw);

    //MATRIX matrix = MGetIdent();

    //matrix.m[0][0] = static_cast<float>((sqx - sqy - sqz + sqw) * invs);
    //matrix.m[1][1] = static_cast<float>((-sqx + sqy - sqz + sqw) * invs);
    //matrix.m[2][2] = static_cast<float>((-sqx - sqy + sqz + sqw) * invs);

    //double tmp1 = q.x * q.y;
    //double tmp2 = q.z * q.w;
    ////matrix.m[0][1] = static_cast<float>(2.0 * (tmp1 + tmp2) * invs);
    ////matrix.m[1][0] = static_cast<float>(2.0 * (tmp1 - tmp2) * invs);
    //matrix.m[0][1] = static_cast<float>(2.0 * (tmp1 - tmp2) * invs);
    //matrix.m[1][0] = static_cast<float>(2.0 * (tmp1 + tmp2) * invs);

    //tmp1 = q.x * q.z;
    //tmp2 = q.y * q.w;
    //matrix.m[0][2] = static_cast<float>(2.0 * (tmp1 - tmp2) * invs);
    //matrix.m[2][0] = static_cast<float>(2.0 * (tmp1 + tmp2) * invs);

    //tmp1 = q.y * q.z;
    //tmp2 = q.x * q.w;
    ////matrix.m[1][2] = static_cast<float>(2.0 * (tmp1 + tmp2) * invs);
    ////matrix.m[2][1] = static_cast<float>(2.0 * (tmp1 - tmp2) * invs);
    //matrix.m[1][2] = static_cast<float>(2.0 * (tmp1 - tmp2) * invs);
    //matrix.m[2][1] = static_cast<float>(2.0 * (tmp1 + tmp2) * invs);

    //return matrix;
}

MATRIX Quaternion::ToMatrix(void) const
{
    return ToMatrix(Quaternion(w, x, y, z));
}

Quaternion Quaternion::LookRotation(VECTOR dir)
{
    VECTOR up = { 0.0f, 1.0f, 0.0f };
    return LookRotation(dir, up);
}

Quaternion Quaternion::LookRotation(VECTOR dir, VECTOR up)
{
    dir = Utility::VNormalize(dir);
    VECTOR right = Utility::VNormalize(VCross(up, dir));
    up = VCross(dir, right);
    auto m00 = right.x;
    auto m01 = right.y;
    auto m02 = right.z;
    auto m10 = up.x;
    auto m11 = up.y;
    auto m12 = up.z;
    auto m20 = dir.x;
    auto m21 = dir.y;
    auto m22 = dir.z;

    float num8 = (m00 + m11) + m22;
    auto quaternion = Quaternion();
    if (num8 > 0.0f)
    {
        double num = sqrt(num8 + 1.0);
        quaternion.w = num * 0.5;
        num = 0.5 / num;
        quaternion.x = ((double)m12 - m21) * num;
        quaternion.y = ((double)m20 - m02) * num;
        quaternion.z = ((double)m01 - m10) * num;
        return quaternion.Normalized();
    }
    if ((m00 >= m11) && (m00 >= m22))
    {
        // xÇ∆wÇ™ãtÅH
        //auto num7 = sqrt(((1.0f + m00) - m11) - m22);
        //auto num4 = 0.5f / num7;
        //quaternion.x = 0.5f * num7;
        //quaternion.y = (m01 + m10) * num4;
        //quaternion.z = (m02 + m20) * num4;
        //quaternion.w = (m12 - m21) * num4;
        //return quaternion.Normalized();
        auto num7 = sqrt(((1.0f + m00) - m11) - m22);
        auto num4 = 0.5f / num7;
        quaternion.x = ((double)m12 - m21) * num4;
        quaternion.y = ((double)m01 + m10) * num4;
        quaternion.z = ((double)m02 + m20) * num4;
        quaternion.w = 0.5 * num7;
        return quaternion.Normalized();
    }
    if (m11 > m22)
    {
        auto num6 = sqrt(((1.0f + m11) - m00) - m22);
        auto num3 = 0.5f / num6;
        quaternion.x = ((double)m10 + m01) * num3;
        quaternion.y = 0.5 * num6;
        quaternion.z = ((double)m21 + m12) * num3;
        quaternion.w = ((double)m20 - m02) * num3;
        return quaternion.Normalized();
    }
    auto num5 = sqrt(((1.0f + m22) - m00) - m11);
    auto num2 = 0.5f / num5;
    quaternion.x = ((double)m20 + m02) * num2;
    quaternion.y = ((double)m21 + m12) * num2;
    quaternion.z = 0.5 * num5;
    quaternion.w = ((double)m01 - m10) * num2;
    return quaternion.Normalized();
}

Quaternion Quaternion::GetRotation(MATRIX mat)
{
    Quaternion ret;

    float s;
    float tr = mat.m[0][0] + mat.m[1][1] + mat.m[2][2] + 1.0f;
    if (tr >= 1.0f)
    {
        s = 0.5f / sqrtf(tr);
        ret.w = 0.25f / s;
        ret.x = (mat.m[1][2] - mat.m[2][1]) * s;
        ret.y = (mat.m[2][0] - mat.m[0][2]) * s;
        ret.z = (mat.m[0][1] - mat.m[1][0]) * s;
    }
    else
    {
        float max;
        max = mat.m[1][1] > mat.m[2][2] ? mat.m[1][1] : mat.m[2][2];

        if (max < mat.m[0][0])
        {
            s = sqrtf(mat.m[0][0] - (mat.m[1][1] + mat.m[2][2]) + 1.0f);

            float x = s * 0.5f;
            s = 0.5f / s;
            ret.x = x;
            ret.y = (mat.m[0][1] + mat.m[1][0]) * s;
            ret.z = (mat.m[2][0] + mat.m[0][2]) * s;
            ret.w = (mat.m[1][2] - mat.m[2][1]) * s;
        }
        else
            if (max == mat.m[1][1])
            {
                s = sqrtf(mat.m[1][1] - (mat.m[2][2] + mat.m[0][0]) + 1.0f);

                float y = s * 0.5f;
                s = 0.5f / s;
                ret.x = (mat.m[0][1] + mat.m[1][0]) * s;
                ret.y = y;
                ret.z = (mat.m[1][2] + mat.m[2][1]) * s;
                ret.w = (mat.m[2][0] - mat.m[0][2]) * s;
            }
            else
            {
                s = sqrtf(mat.m[2][2] - (mat.m[0][0] + mat.m[1][1]) + 1.0f);

                float z = s * 0.5f;
                s = 0.5f / s;
                ret.x = (mat.m[2][0] + mat.m[0][2]) * s;
                ret.y = (mat.m[1][2] + mat.m[2][1]) * s;
                ret.z = z;
                ret.w = (mat.m[0][1] - mat.m[1][0]) * s;
            }
    }

    return ret;

    //float elem[4];
    //elem[0] = mat.m[0][0] - mat.m[1][1] - mat.m[2][2] + 1.0f;
    //elem[1] = -mat.m[0][0] + mat.m[1][1] - mat.m[2][2] + 1.0f;
    //elem[2] = -mat.m[0][0] - mat.m[1][1] + mat.m[2][2] + 1.0f;
    //elem[3] = mat.m[0][0] + mat.m[1][1] + mat.m[2][2] + 1.0f;

    //int biggestIdx = 0;
    //for (int i = 0; i < 4; i++)
    //{
    //    if (elem[i] > elem[biggestIdx])
    //    {
    //        biggestIdx = i;
    //    }
    //}

    //if (elem[biggestIdx] < 0)
    //{
    //    return Quaternion();
    //}

    //float q[4];
    //float v = sqrt(elem[biggestIdx]) * 0.5f;
    //q[biggestIdx] = v;
    //float mult = 0.25f / v;

    //switch (biggestIdx)
    //{
    //case 0:
    //    q[1] = (mat.m[1][0] + mat.m[0][1]) * mult;
    //    q[2] = (mat.m[0][2] + mat.m[2][0]) * mult;
    //    q[3] = (mat.m[2][1] - mat.m[1][2]) * mult;
    //    break;
    //case 1:
    //    q[0] = (mat.m[1][0] + mat.m[0][1]) * mult;
    //    q[2] = (mat.m[2][1] + mat.m[1][2]) * mult;
    //    q[3] = (mat.m[0][2] - mat.m[2][0]) * mult;
    //    break;
    //case 2:
    //    q[0] = (mat.m[0][2] + mat.m[2][0]) * mult;
    //    q[1] = (mat.m[2][1] + mat.m[1][2]) * mult;
    //    q[3] = (mat.m[1][0] - mat.m[0][1]) * mult;
    //    break;
    //case 3:
    //    q[0] = (mat.m[2][1] - mat.m[1][2]) * mult;
    //    q[1] = (mat.m[0][2] - mat.m[2][0]) * mult;
    //    q[2] = (mat.m[1][0] - mat.m[0][1]) * mult;
    //    break;
    //}

    //return Quaternion(q[3], q[0], q[1], q[2]);
}

VECTOR Quaternion::GetDir(VECTOR dir) const
{
    return PosAxis(dir);
}

VECTOR Quaternion::GetForward(void) const
{
    return GetDir(Utility::DIR_F);
}

VECTOR Quaternion::GetBack(void) const
{
    return GetDir(Utility::DIR_B);
}

VECTOR Quaternion::GetRight(void) const
{
    return GetDir(Utility::DIR_R);
}

VECTOR Quaternion::GetLeft(void) const
{
    return GetDir(Utility::DIR_L);
}

VECTOR Quaternion::GetUp(void) const
{
    return GetDir(Utility::DIR_U);
}

VECTOR Quaternion::GetDown(void) const
{
    return GetDir(Utility::DIR_D);
}

double Quaternion::Dot(const Quaternion& q1, const Quaternion& q2)
{
    return (q1.w * q2.w + q1.x * q2.x + q1.y * q2.y + q1.z * q2.z);
}

double Quaternion::Dot(const Quaternion& q) const
{
    return (w * q.w + x * q.x + y * q.y + z * q.z);
}

Quaternion Quaternion::Normalize(const Quaternion& q)
{
    float scale = 1.0f / static_cast<float>(q.Length());
    VECTOR v = VScale(q.xyz(), scale);
    Quaternion ret = Quaternion(q.w * scale, v.x, v.y, v.z);
    return ret;
}

Quaternion Quaternion::Normalized(void) const
{
    double mag = sqrt(w * w + x * x + y * y + z * z);
    return Quaternion(w / mag, x / mag, y / mag, z / mag);
}

void Quaternion::Normalize(void)
{
    double mag = sqrt(w * w + x * x + y * y + z * z);

    w /= mag;
    x /= mag;
    y /= mag;
    z /= mag;
}

Quaternion Quaternion::Inverse(void) const
{
    double n = 1.0f / (w * w + x * x + y * y + z * z);
    Quaternion tmp = Quaternion(w, -x, -y, -z);
    return Quaternion(tmp.w * n, tmp.x * n, tmp.y * n, tmp.z * n);;
}

Quaternion Quaternion::Slerp(Quaternion from, Quaternion to, double t)
{
    if (t > 1) t = 1;
    if (t < 0) t = 0;
    return SlerpUnclamped(from, to, (float)t);
}

inline float SIGN(float x) {
    return (x >= 0.0f) ? +1.0f : -1.0f;
}

inline float NORM(float a, float b, float c, float d) {
    return sqrt(a * a + b * b + c * c + d * d);
}

Quaternion Quaternion::FromToRotation(VECTOR fromDir, VECTOR toDir)
{
    VECTOR axis = VCross(fromDir, toDir);
    double angle = Utility::AngleDeg(fromDir, toDir);
    if (angle >= 179.9196)
    {
        auto r = VCross(fromDir, Utility::DIR_R);
        axis = VCross(r, fromDir);
        //if (axis.sqrMagnitude < 0.000001f)
        float len = axis.x * axis.x + axis.y * axis.y + axis.z * axis.z;
        if (len < 0.000001f)
        {
            axis = Utility::DIR_U;
        }
    }

    axis = Utility::VNormalize(axis);
    return Quaternion::AngleAxis(Utility::Deg2RadD(angle), axis);
}

Quaternion Quaternion::RotateTowards(const Quaternion& from, const Quaternion& to, float maxDegreesDelta)
{
    double num = Quaternion::Angle(from, to);
    if (num == 0.0)
    {
        return to;
    }
    float t = min(1.0f, maxDegreesDelta / (float)num);
    return Quaternion::SlerpUnclamped(from, to, t);
}

double Quaternion::Angle(const Quaternion& q1, const Quaternion& q2)
{
    double cos = Quaternion::Dot(q1, q2);
    double ac = acos(cos);
    return ac * (180.0 / DX_PI);
}

Quaternion Quaternion::SlerpUnclamped(Quaternion a, Quaternion b, float t)
{
    // if either input is zero, return the other.
    if (a.LengthSquared() == 0.0f)
    {
        if (b.LengthSquared() == 0.0f)
        {
            return Identity();
        }
        return b;
    }
    else if (b.LengthSquared() == 0.0f)
    {
        return a;
    }

    float cosHalfAngle = (float)(a.w * b.w) + VDot(a.xyz(), b.xyz());

    if (cosHalfAngle >= 1.0f || cosHalfAngle <= -1.0f)
    {
        // angle = 0.0f, so just return one input.
        return a;
    }
    else if (cosHalfAngle < 0.0f)
    {
        //b.xyz() = -b.xyz();
        b.x = b.x * -1.0f;
        b.y = b.y * -1.0f;
        b.z = b.z * -1.0f;
        b.w = -b.w;
        cosHalfAngle = -cosHalfAngle;
    }

    float blendA;
    float blendB;
    if (cosHalfAngle < 0.99f)
    {
        // do proper slerp for big angles
        float halfAngle = acosf(cosHalfAngle);
        float sinHalfAngle = sinf(halfAngle);
        float oneOverSinHalfAngle = 1.0f / sinHalfAngle;
        blendA = sinf(halfAngle * (1.0f - t)) * oneOverSinHalfAngle;
        blendB = sinf(halfAngle * t) * oneOverSinHalfAngle;
    }
    else
    {
        // do lerp if angle is really small.
        blendA = 1.0f - t;
        blendB = t;
    }

    //Quaternion result = Quaternion(blendA * a.xyz() + blendB * b.xyz(), blendA * a.w + blendB * b.w);
    VECTOR v = VAdd(VScale(a.xyz(), blendA), VScale(b.xyz(), blendB));
    //Quaternion result = Quaternion(v.x, v.y, v.z, blendA * a.w + blendB * b.w);
    Quaternion result = Quaternion(blendA * a.w + blendB * b.w, v.x, v.y, v.z);
    if (result.LengthSquared() > 0.0f)
    {
        return Normalize(result);
    }
    else
    {
        return Identity();
    }
}

Quaternion Quaternion::Identity(void)
{
    return Quaternion(1.0f, 0.0f, 0.0f, 0.0f);
}

double Quaternion::Length(void) const
{
    return sqrt(x * x + y * y + z * z + w * w);
}

double Quaternion::LengthSquared(void) const
{
    return x * x + y * y + z * z + w * w;
}

VECTOR Quaternion::xyz(void) const
{
    return { (float)x, (float)y, (float)z };
}

void Quaternion::ToAngleAxis(float* angle, VECTOR* axis)
{
    if (abs(this->w) > 1.0f)
    {
        this->Normalize();
    }
    *angle = 2.0f * acosf((float)this->w); // angle

    if (x == 0 && y == 0 && z == 0)
    {
        *angle = 0.0f;
    }

    float den = sqrtf(1.0f - (float)(this->w * this->w));
    if (den > 0.0001f)
    {
        //axis = q->xyz / den;
        auto v = this->xyz();
        axis->x = v.x / den;
        axis->y = v.y / den;
        axis->z = v.z / den;
    }
    else
    {
        // This occurs when the angle is zero. 
        // Not a problem: just set an arbitrary normalized axis.
        *axis = { 1.0f, 0.0f, 0.0f };
    }
}

Quaternion Quaternion::operator*(float& f) {
    return Quaternion(w * f, x * f, y * f, z * f);
}

const Quaternion Quaternion::operator*(const float& f) {
    return Quaternion(w * f, x * f, y * f, z * f);
}

Quaternion Quaternion::operator+(Quaternion& rhs) {
    return Quaternion(w + rhs.w, x + rhs.x, y + rhs.y, z + rhs.z);
}

const Quaternion Quaternion::operator+(const Quaternion& rhs) {
    return Quaternion(w + rhs.w, x + rhs.x, y + rhs.y, z + rhs.z);
}
