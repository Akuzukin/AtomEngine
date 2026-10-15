using Atom;
using System.Numerics;
using System.Text.Json;
namespace StarOutpost;
// Included only in ServicesCheck.scene. Tests native resize, particles, audio and graceful shutdown.
public sealed class ServiceDiagnostics:Behaviour
{
    private int frames;
    private bool cleanup;
    public override void OnUpdate(float dt)
    {
        frames++;
        if(frames==3){var game=OutpostRuntime.Current!.Game;game.Persist=null;game.Start();}
        if(frames==5)Object.Application.SetWindow(800,600);
        if(frames==30){CheckSize(800,600);Object.Application.SetWindow(1600,900);}
        if(frames==60){CheckSize(1600,900);Object.Application.SetWindow(1280,720);}
        if(frames==90){CheckSize(1280,720);var mixer=Object.AudioMixer;mixer.SetVolume(AudioBus.Effects,.4f);mixer.MasterVolume=.5f;if(mixer.GetVolume(AudioBus.Effects)!=.4f||mixer.MasterVolume!=.5f)throw new Exception("Mixer bridge mismatch");var emitter=Object.FindById("BurstEmitter2")!;if(!emitter.HasComponent(ComponentKind.Particle)||!Object.Query(ComponentKind.Particle).Any(o=>o.Entity==emitter.Entity))throw new Exception("Particle ECS query failed");emitter.GetParticleComponent()!.Value.Play(Vector3.Zero);emitter.GetParticleComponent()!.Value.Stop();emitter.GetParticleComponent()!.Value.Play(Vector3.UnitX);}
        if(frames==120){Write(false);Object.Application.Quit();}
    }
    private void CheckSize(int width,int height){var size=Object.Pointer.ViewportSize;if(size!=new Vector2(width,height))throw new Exception("Resize did not update UI overlay: "+size);}
    private void Write(bool destroyed){string dir=Path.Combine(Environment.CurrentDirectory,"Diagnostics");Directory.CreateDirectory(dir);File.WriteAllText(Path.Combine(dir,"services-check.json"),JsonSerializer.Serialize(new{frames,cleanup=destroyed,resizePassed=frames>=120,mixerPassed=frames>=120}));}
    public override void OnDestroy(){cleanup=true;Write(cleanup);}
}
