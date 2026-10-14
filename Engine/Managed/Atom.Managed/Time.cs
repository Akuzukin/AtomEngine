using System.Text.Json;
namespace Atom;

/// <summary>当前游戏世界的只读时间。暂停时停止计数，重新 Play 时归零，热重载保留；单位为秒。</summary>
public static class Time
{
    private sealed record Snapshot(float FrameDelta, float FixedDelta, double Elapsed, double FixedElapsed,
        ulong Frames, ulong FixedFrames, bool Paused, bool FixedStep);
    private static Snapshot current = new(0, 1f / 60, 0, 0, 0, 0, false, false);
    /// <summary>当前回调的时间间隔（秒）；固定更新及碰撞回调使用固定步长，其他回调使用当前游戏帧间隔，暂停为零。</summary>
    public static float DeltaTime => current.Paused ? 0 : current.FixedStep ? current.FixedDelta : current.FrameDelta;
    /// <summary>当前游戏帧的间隔（秒），与 OnUpdate 的 dt 一致；暂停为零。</summary>
    public static float FrameDeltaTime => current.FrameDelta;
    /// <summary>物理固定更新步长（秒），默认 1/60；即使暂停也保留配置值。</summary>
    public static float FixedDeltaTime => current.FixedDelta;
    /// <summary>自本次 Play 开始累计的游戏帧时间（秒），不包含暂停时间；使用双精度避免长时间运行精度下降。</summary>
    public static double ElapsedTime => current.Elapsed;
    /// <summary>已经执行的物理固定步累计时间（秒），一帧可能包含零次或多次固定更新。</summary>
    public static double FixedTime => current.FixedElapsed;
    /// <summary>本次 Play 已执行的游戏帧数；第一帧为 1，多个脚本共享同一帧号。</summary>
    public static ulong FrameCount => current.Frames;
    /// <summary>本次 Play 已执行的固定更新次数；每个物理步只增加一次，不受脚本数量影响。</summary>
    public static ulong FixedFrameCount => current.FixedFrames;
    /// <summary>当前游戏帧的瞬时帧率（1/FrameDeltaTime），暂停或间隔为零时返回零；不代表平滑后的显示帧率。</summary>
    public static float FramesPerSecond => current.FrameDelta > 0 ? 1 / current.FrameDelta : 0;
    /// <summary>当前是否位于 OnFixedUpdate 或物理碰撞回调中。</summary>
    public static bool IsFixedUpdate => current.FixedStep;
    /// <summary>游戏是否暂停；暂停期间不执行更新回调。</summary>
    public static bool IsPaused => current.Paused;
    internal static void Apply(JsonElement value, bool fixedStep)
    {
        current = new(value.GetProperty("delta").GetSingle(), value.GetProperty("fixedDelta").GetSingle(),
            value.GetProperty("elapsed").GetDouble(), value.GetProperty("fixedElapsed").GetDouble(),
            value.GetProperty("frames").GetUInt64(), value.GetProperty("fixedFrames").GetUInt64(),
            value.GetProperty("paused").GetBoolean(), fixedStep);
    }
    internal static void EndCallback() { if (current.FixedStep) current = current with { FixedStep = false }; }
}
