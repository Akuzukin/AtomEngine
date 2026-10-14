using Atom;
using System.Text.Json;
namespace Game;

// 由原生回归检查附加，用于验证多个脚本共享同一帧时间。
public sealed class TimeProbe : Behaviour
{
    public override void OnUpdate(float dt)
    {
        if (Time.DeltaTime != dt || Time.FrameDeltaTime != dt || Time.IsFixedUpdate || Time.IsPaused)
            throw new InvalidOperationException("Game frame time does not match engine callback");
        Report();
    }
    public override void OnFixedUpdate(float dt)
    {
        if (Time.DeltaTime != dt || Time.FixedDeltaTime != dt || !Time.IsFixedUpdate || Time.IsPaused)
            throw new InvalidOperationException("Fixed step time does not match engine callback");
        Report();
    }
    public override void OnUiClick() => Report();
    public override string CaptureReloadState() => "time-probe";
    public override void RestoreReloadState(string state) { }
    public override void OnReload() => Report();
    private void Report() => Object.Text = JsonSerializer.Serialize(new {
        delta=Time.DeltaTime, frameDelta=Time.FrameDeltaTime, fixedDelta=Time.FixedDeltaTime,
        elapsed=Time.ElapsedTime, fixedElapsed=Time.FixedTime, frames=Time.FrameCount,
        fixedFrames=Time.FixedFrameCount, fps=Time.FramesPerSecond,
        paused=Time.IsPaused, fixedStep=Time.IsFixedUpdate
    });
}
