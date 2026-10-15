using Atom;
namespace StarOutpost;
public sealed class AudioController:Behaviour
{
    public static AudioController? Current;
    public bool Muted {get;private set;}
    private Simulation? game;private float timer;
    public override void OnCreate()=>Current=this;
    public override void OnUpdate(float dt){if(game==null){if(OutpostRuntime.Current==null)return;game=OutpostRuntime.Current.Game;game.Sound=Play;}timer=Math.Max(0,timer-dt);}
    public void Toggle(){Muted=!Muted;Object.AudioMixer.MasterVolume=Muted?0:1;}
    private void Play(string name){if(Muted||timer>0&&name=="shot")return;Object.AudioMixer.Play(new AudioClip("Asset/Sounds/"+name+".wav"),name=="upgrade"?AudioBus.UI:AudioBus.Effects,name=="shot"?.12f:.35f);if(name=="shot")timer=.12f;}
    public override void OnDestroy(){if(game!=null)game.Sound=null;if(Current==this)Current=null;}
}
