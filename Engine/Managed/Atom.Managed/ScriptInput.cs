using System.Numerics;
namespace Atom;
/// <summary>具名键盘、鼠标和手柄输入接口；所有查询遵守引擎游戏输入焦点限制。</summary>
public sealed class ScriptInput
{
    private readonly GameObject owner;
    internal ScriptInput(GameObject owner) { this.owner = owner; }
    private bool Read(int device,int code,int state) => owner.Request(new {op="input",device,code,state}).GetBoolean();
    private static int Code<T>(T value) where T:struct,Enum
    {
        if (!Enum.IsDefined(value)) throw new ArgumentOutOfRangeException(nameof(value),value,"Unknown input code");
        return Convert.ToInt32(value);
    }
    /// <summary>查询指定按键是否持续按下。</summary>
    /// <param name="key">要查询的键盘按键；None 返回假。</param>
    public bool IsDown(KeyCode key) => key!=KeyCode.None && Read(0,Code(key),0);
    /// <summary>查询指定按键是否在当前帧首次按下。</summary>
    /// <param name="key">要查询的键盘按键；None 返回假。</param>
    public bool WasPressed(KeyCode key) => key!=KeyCode.None && Read(0,Code(key),1);
    /// <summary>查询指定按键是否在当前帧松开。</summary>
    /// <param name="key">要查询的键盘按键；None 返回假。</param>
    public bool WasReleased(KeyCode key) => key!=KeyCode.None && Read(0,Code(key),2);
    /// <summary>查询指定按键是否持续按下。</summary>
    /// <param name="button">要查询的鼠标或手柄按键。</param>
    public bool IsDown(MouseButton button) => Read(1,Code(button),0);
    /// <summary>查询指定按键是否在当前帧首次按下。</summary>
    /// <param name="button">要查询的鼠标或手柄按键。</param>
    public bool WasPressed(MouseButton button) => Read(1,Code(button),1);
    /// <summary>查询指定按键是否在当前帧松开。</summary>
    /// <param name="button">要查询的鼠标或手柄按键。</param>
    public bool WasReleased(MouseButton button) => Read(1,Code(button),2);
    /// <summary>查询指定按键是否持续按下。</summary>
    /// <param name="button">要查询的鼠标或手柄按键。</param>
    public bool IsDown(GamepadButton button) => Read(2,Code(button),0);
    /// <summary>查询指定按键是否在当前帧首次按下。</summary>
    /// <param name="button">要查询的鼠标或手柄按键。</param>
    public bool WasPressed(GamepadButton button) => Read(2,Code(button),1);
    /// <summary>查询指定按键是否在当前帧松开。</summary>
    /// <param name="button">要查询的鼠标或手柄按键。</param>
    public bool WasReleased(GamepadButton button) => Read(2,Code(button),2);
    /// <summary>读取具名手柄轴，摇杆范围 -1～1，扳机范围 0～1；无游戏输入焦点时为零。</summary>
    /// <param name="axis">要读取的手柄轴。</param>
    public float Axis(GamepadAxis axis) { owner.EnsureValid();return owner.GamepadAxis(Code(axis)); }
    /// <summary>组合键盘方向与手柄摇杆得到移动向量；前进为正 Y，合成向量长度限制为 1。</summary>
    /// <param name="preset">键盘移动预设，WASD 或方向键，均合成左摇杆输入。</param>
    public Vector2 Movement(InputPreset preset=InputPreset.Wasd) => preset switch
    {
        InputPreset.Wasd=>Movement(KeyCode.W,KeyCode.S,KeyCode.A,KeyCode.D),
        InputPreset.ArrowKeys=>Movement(KeyCode.Up,KeyCode.Down,KeyCode.Left,KeyCode.Right),
        _=>throw new ArgumentOutOfRangeException(nameof(preset))
    };
    /// <summary>组合键盘方向与手柄摇杆得到移动向量；前进为正 Y，合成向量长度限制为 1。</summary>
    /// <param name="forward">前进键，对应正 Y。</param>
    /// <param name="backward">后退键，对应负 Y。</param>
    /// <param name="left">左移键，对应负 X。</param>
    /// <param name="right">右移键，对应正 X。</param>
    /// <param name="horizontal">横向手柄轴，默认左摇杆 X。</param>
    /// <param name="vertical">纵向手柄轴，默认左摇杆 Y。</param>
    public Vector2 Movement(KeyCode forward,KeyCode backward,KeyCode left,KeyCode right,
        GamepadAxis horizontal=GamepadAxis.LeftX,GamepadAxis vertical=GamepadAxis.LeftY)
    {
        var v=new Vector2((IsDown(right)?1:0)-(IsDown(left)?1:0)+Axis(horizontal),
            (IsDown(forward)?1:0)-(IsDown(backward)?1:0)+Axis(vertical));
        return v.LengthSquared()>1?Vector2.Normalize(v):v;
    }
}
public sealed partial class GameObject
{
    private ScriptInput? input;
    /// <summary>此对象的具名输入接口，支持按键状态与移动预设。</summary>
    public ScriptInput Input => input??=new(this);
    /// <summary>查询具名键盘按键是否持续按下；None 表示不绑定。</summary>
    /// <param name="key">要查询的键盘按键；None 返回假。</param>
    public bool IsKeyDown(KeyCode key)=>Input.IsDown(key);
    /// <summary>查询具名键盘按键是否在当前帧首次按下；None 表示不绑定。</summary>
    /// <param name="key">要查询的键盘按键；None 返回假。</param>
    public bool IsKeyPressed(KeyCode key)=>Input.WasPressed(key);
    /// <summary>读取具名手柄轴，摇杆范围 -1～1，扳机范围 0～1。</summary>
    /// <param name="axis">要读取的手柄轴。</param>
    public float GamepadAxis(GamepadAxis axis)=>Input.Axis(axis);
    /// <summary>查询具名手柄按键是否持续按下。</summary>
    /// <param name="button">要查询的鼠标或手柄按键。</param>
    public bool IsGamepadDown(GamepadButton button)=>Input.IsDown(button);
    /// <summary>查询具名鼠标按键是否持续按下。</summary>
    /// <param name="button">要查询的鼠标或手柄按键。</param>
    public bool IsMouseDown(MouseButton button)=>Input.IsDown(button);
}
