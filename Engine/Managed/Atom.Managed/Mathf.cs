using System.Numerics;
namespace Atom;
/// <summary>与引擎常用数学函数保持一致；三角函数的角度参数及结果使用弧度。</summary>
public static class Mathf
{
    /// <summary>圆周率 π。</summary>
    public const float PI = 3.14159265358979323846f;
    /// <summary>完整一周的弧度值 2π。</summary>
    public const float TwoPI = 2 * PI;
    /// <summary>直角的弧度值 π/2。</summary>
    public const float HalfPI = PI / 2;
    /// <summary>角度转弧度的乘数 π/180。</summary>
    public const float Deg2Rad = PI / 180;
    /// <summary>弧度转角度的乘数 180/π。</summary>
    public const float Rad2Deg = 180 / PI;
    /// <summary>数学近似阈值 1e-6；Equal 的默认误差另行定义。</summary>
    public const float Epsilon = 1e-6f;
    /// <summary>正无穷。</summary>
    public const float Infinity = float.PositiveInfinity;
    /// <summary>负无穷。</summary>
    public const float NegInfinity = float.NegativeInfinity;
    /// <summary>返回绝对值。</summary>
    /// <param name="v">输入数值或向量。</param>
    public static float Abs(float v) => MathF.Abs(v);
    /// <summary>检查数值是否为 NaN。</summary>
    /// <param name="v">输入数值或向量。</param>
    public static bool IsNaN(float v) => float.IsNaN(v);
    /// <summary>检查数值是否有限，即不是 NaN 或无穷。</summary>
    /// <param name="v">输入数值或向量。</param>
    public static bool IsFinite(float v) => float.IsFinite(v);
    /// <summary>返回平方。</summary>
    /// <param name="v">输入数值或向量。</param>
    public static float Sqr(float v) => v * v;
    /// <summary>返回平方根。</summary>
    /// <param name="v">输入数值或向量。</param>
    public static float Sqrt(float v) => MathF.Sqrt(v);
    /// <summary>返回平方根的倒数；零返回正无穷。</summary>
    /// <param name="v">输入数值或向量。</param>
    public static float InvSqrt(float v) => 1 / Sqrt(v);
    // Native Equal's default is std::numeric_limits<float>::epsilon(), not Math::Epsilon.
    /// <summary>判断两个浮点数的绝对差是否不超过指定误差；默认使用原生 float 机器精度。</summary>
    /// <param name="a">第一个操作数、起点或起始旋转。</param>
    /// <param name="b">第二个操作数、终点或目标旋转。</param>
    /// <param name="tolerance">允许的最大绝对误差。</param>
    public static bool Equal(float a, float b, float tolerance = 1.1920929e-7f) => Abs(b - a) <= tolerance;
    /// <summary>将数值限制到闭区间 [min, max]；最小值大于最大值时抛出异常。</summary>
    /// <param name="v">输入数值或向量。</param>
    /// <param name="min">闭区间最小值。</param>
    /// <param name="max">闭区间最大值。</param>
    public static float Clamp(float v, float min, float max) => System.Math.Clamp(v, min, max);
    /// <summary>将数值限制到闭区间 [min, max]；最小值大于最大值时抛出异常。</summary>
    /// <param name="v">输入数值或向量。</param>
    /// <param name="min">闭区间最小值。</param>
    /// <param name="max">闭区间最大值。</param>
    public static int Clamp(int v, int min, int max) => System.Math.Clamp(v, min, max);
    /// <summary>将数值限制到 [0, 1]。</summary>
    /// <param name="v">输入数值或向量。</param>
    public static float Clamp01(float v) => Clamp(v, 0, 1);
    /// <summary>返回较小值。</summary>
    /// <param name="a">第一个操作数、起点或起始旋转。</param>
    /// <param name="b">第二个操作数、终点或目标旋转。</param>
    public static float Min(float a, float b) => b < a ? b : a;
    /// <summary>返回较大值。</summary>
    /// <param name="a">第一个操作数、起点或起始旋转。</param>
    /// <param name="b">第二个操作数、终点或目标旋转。</param>
    public static float Max(float a, float b) => a < b ? b : a;
    /// <summary>返回较小值。</summary>
    /// <param name="a">第一个操作数、起点或起始旋转。</param>
    /// <param name="b">第二个操作数、终点或目标旋转。</param>
    public static int Min(int a, int b) => a < b ? a : b;
    /// <summary>返回较大值。</summary>
    /// <param name="a">第一个操作数、起点或起始旋转。</param>
    /// <param name="b">第二个操作数、终点或目标旋转。</param>
    public static int Max(int a, int b) => a < b ? b : a;
    /// <summary>向负无穷取整；向量重载逐分量计算。</summary>
    /// <param name="v">输入数值或向量。</param>
    public static float Floor(float v) => MathF.Floor(v);
    /// <summary>向正无穷取整。</summary>
    /// <param name="v">输入数值或向量。</param>
    public static float Ceil(float v) => MathF.Ceiling(v);
    /// <summary>向负无穷取整；向量重载逐分量计算。</summary>
    /// <param name="v">输入数值或向量。</param>
    public static Vector3 Floor(Vector3 v) => new(Floor(v.X), Floor(v.Y), Floor(v.Z));
    /// <summary>将角度转换为弧度。</summary>
    /// <param name="v">输入数值或向量。</param>
    public static float DegreesToRadians(float v) => v * Deg2Rad;
    /// <summary>将弧度转换为角度。</summary>
    /// <param name="v">输入数值或向量。</param>
    public static float RadiansToDegrees(float v) => v * Rad2Deg;
    /// <summary>返回弧度角的正弦。</summary>
    /// <param name="v">输入数值或向量。</param>
    public static float Sin(float v) => MathF.Sin(v);
    /// <summary>返回弧度角的余弦。</summary>
    /// <param name="v">输入数值或向量。</param>
    public static float Cos(float v) => MathF.Cos(v);
    /// <summary>返回弧度角的正切。</summary>
    /// <param name="v">输入数值或向量。</param>
    public static float Tan(float v) => MathF.Tan(v);
    /// <summary>返回反余弦，单位弧度；输入越界时按引擎规则限制到端点。</summary>
    /// <param name="v">输入数值或向量。</param>
    public static float Acos(float v) => v > -1 ? (v < 1 ? MathF.Acos(v) : 0) : PI;
    /// <summary>返回反正弦，单位弧度；输入越界时按引擎规则限制到端点。</summary>
    /// <param name="v">输入数值或向量。</param>
    public static float Asin(float v) => v > -1 ? (v < 1 ? MathF.Asin(v) : HalfPI) : -HalfPI;
    /// <summary>返回反正切，单位弧度。</summary>
    /// <param name="v">输入数值或向量。</param>
    public static float Atan(float v) => MathF.Atan(v);
    /// <summary>根据 y 和 x 返回包含象限信息的反正切，单位弧度。</summary>
    /// <param name="y">纵坐标。</param>
    /// <param name="x">横坐标。</param>
    public static float Atan2(float y, float x) => MathF.Atan2(y, x);
    /// <summary>返回 a 的 b 次方。</summary>
    /// <param name="a">底数。</param>
    /// <param name="b">指数。</param>
    public static float Pow(float a, float b) => MathF.Pow(a, b);
    // Unclamped, as in native Math::Lerp.
    /// <summary>进行线性插值；t 不限制到 [0, 1]，支持区间外推。</summary>
    /// <param name="a">第一个操作数、起点或起始旋转。</param>
    /// <param name="b">第二个操作数、终点或目标旋转。</param>
    /// <param name="t">插值系数，不自动限制范围。</param>
    public static float Lerp(float a, float b, float t) => a + t * (b - a);
    /// <summary>进行线性插值；t 不限制到 [0, 1]，支持区间外推。</summary>
    /// <param name="a">第一个操作数、起点或起始旋转。</param>
    /// <param name="b">第二个操作数、终点或目标旋转。</param>
    /// <param name="t">插值系数，不自动限制范围。</param>
    public static Vector2 Lerp(Vector2 a, Vector2 b, float t) => a + t * (b - a);
    /// <summary>进行线性插值；t 不限制到 [0, 1]，支持区间外推。</summary>
    /// <param name="a">第一个操作数、起点或起始旋转。</param>
    /// <param name="b">第二个操作数、终点或目标旋转。</param>
    /// <param name="t">插值系数，不自动限制范围。</param>
    public static Vector3 Lerp(Vector3 a, Vector3 b, float t) => a + t * (b - a);
    /// <summary>进行线性插值；t 不限制到 [0, 1]，支持区间外推。</summary>
    /// <param name="a">第一个操作数、起点或起始旋转。</param>
    /// <param name="b">第二个操作数、终点或目标旋转。</param>
    /// <param name="t">插值系数，不自动限制范围。</param>
    public static Vector4 Lerp(Vector4 a, Vector4 b, float t) => a + t * (b - a);
    /// <summary>将指定区间内的值平滑映射到 [0, 1]；两个边界应不同。</summary>
    /// <param name="edge0">映射区间起点。</param>
    /// <param name="edge1">映射区间终点，须与起点不同。</param>
    /// <param name="value">待映射的数值。</param>
    public static float SmoothStep(float edge0, float edge1, float value)
    { float t = Clamp01((value - edge0) / (edge1 - edge0)); return t * t * (3 - 2 * t); }
    // Native Math::Normalize returns UNIT_SCALE for an exactly zero vector.
    /// <summary>返回归一化向量；精确零向量按原生规则返回 Vector3.One。</summary>
    /// <param name="v">输入数值或向量。</param>
    public static Vector3 Normalize(Vector3 v) => v == Vector3.Zero ? Vector3.One : v * (1 / Length(v));
    /// <summary>返回归一化向量；零向量返回 Vector3.Zero。</summary>
    /// <param name="v">输入数值或向量。</param>
    public static Vector3 NormalizeOrZero(Vector3 v) => v == Vector3.Zero ? Vector3.Zero : Normalize(v);
    /// <summary>返回向量长度，任一分量无穷时返回正无穷。</summary>
    /// <param name="v">输入数值或向量。</param>
    public static float Length(Vector3 v)
    {
        if (float.IsInfinity(v.X) || float.IsInfinity(v.Y) || float.IsInfinity(v.Z)) return Infinity;
        return (float)System.Math.Sqrt((double)v.X * v.X + (double)v.Y * v.Y + (double)v.Z * v.Z);
    }
    /// <summary>返回向量点积。</summary>
    /// <param name="a">第一个操作数、起点或起始旋转。</param>
    /// <param name="b">第二个操作数、终点或目标旋转。</param>
    public static float Dot(Vector3 a, Vector3 b) => Vector3.Dot(a, b);
    /// <summary>返回向量叉积。</summary>
    /// <param name="a">第一个操作数、起点或起始旋转。</param>
    /// <param name="b">第二个操作数、终点或目标旋转。</param>
    public static Vector3 Cross(Vector3 a, Vector3 b) => a == Vector3.Zero || b == Vector3.Zero ? Vector3.Zero : Vector3.Cross(a, b);
    /// <summary>返回两点间距离。</summary>
    /// <param name="a">第一个操作数、起点或起始旋转。</param>
    /// <param name="b">第二个操作数、终点或目标旋转。</param>
    public static float Distance(Vector3 a, Vector3 b) => Length(a - b);
    /// <summary>返回两点间距离的平方。</summary>
    /// <param name="a">第一个操作数、起点或起始旋转。</param>
    /// <param name="b">第二个操作数、终点或目标旋转。</param>
    public static float DistanceSquared(Vector3 a, Vector3 b) => Vector3.DistanceSquared(a, b);
    /// <summary>使用四元数旋转向量。</summary>
    /// <param name="rotation">旋转四元数。</param>
    /// <param name="v">输入数值或向量。</param>
    public static Vector3 Rotate(Quaternion rotation, Vector3 v) => Vector3.Transform(v, rotation);
    /// <summary>从旋转轴和弧度角构造四元数；轴自动归一化，零轴返回单位旋转。</summary>
    /// <param name="axis">旋转轴，内部归一化；零轴表示单位旋转。</param>
    /// <param name="radians">旋转角，单位弧度。</param>
    public static Quaternion AxisAngle(Vector3 axis, float radians) => axis == Vector3.Zero ? Quaternion.Identity : Quaternion.CreateFromAxisAngle(NormalizeOrZero(axis), radians);
    // Same qz * qx * qy order used by CameraBase::SetRotation.
    /// <summary>从弧度欧拉角构造归一化四元数，顺序与引擎一致，为 qz * qx * qy。</summary>
    /// <param name="angles">X/Y/Z 欧拉角，单位弧度。</param>
    public static Quaternion EulerRadians(Vector3 angles) => Quaternion.Normalize(AxisAngle(Vector3.UnitZ, angles.Z) * AxisAngle(Vector3.UnitX, angles.X) * AxisAngle(Vector3.UnitY, angles.Y));
    /// <summary>从角度欧拉角构造旋转四元数，顺序为 qz * qx * qy。</summary>
    /// <param name="angles">X/Y/Z 欧拉角，单位度。</param>
    public static Quaternion EulerDegrees(Vector3 angles) => EulerRadians(angles * Deg2Rad);
    // Matches the native free Slerp(kp, kq, t, shortestPath) helper.
    /// <summary>球面插值四元数，与原生函数一致；默认选择最短旋转路径，t 不作区间限制。</summary>
    /// <param name="a">第一个操作数、起点或起始旋转。</param>
    /// <param name="b">第二个操作数、终点或目标旋转。</param>
    /// <param name="t">插值系数，不自动限制范围。</param>
    /// <param name="shortestPath">是否选择最短旋转路径，默认是。</param>
    public static Quaternion Slerp(Quaternion a, Quaternion b, float t, bool shortestPath = true)
    {
        float cosine = Quaternion.Dot(a, b);
        if (cosine < 0 && shortestPath) { cosine = -cosine; b = -b; }
        if (Abs(cosine) < 1 - Epsilon) {
            float sine = Sqrt(1 - Sqr(cosine)), angle = Atan2(sine, cosine);
            return a * (Sin((1 - t) * angle) / sine) + b * (Sin(t * angle) / sine);
        }
        return Quaternion.Normalize(a * (1 - t) + b * t);
    }
}
