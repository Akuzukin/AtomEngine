namespace Atom;
/// <summary>与 Win32 虚拟键一致的具名键盘按键；推荐替代数字编号。</summary>
public enum KeyCode
{
    /// <summary>不绑定按键。</summary>
    None = 0x00,
    /// <summary>退格键。</summary>
    Backspace = 0x08,
    /// <summary>制表键。</summary>
    Tab = 0x09,
    /// <summary>回车键。</summary>
    Enter = 0x0D,
    /// <summary>任一 Shift 键。</summary>
    Shift = 0x10,
    /// <summary>任一 Ctrl 键。</summary>
    Control = 0x11,
    /// <summary>任一 Alt 键。</summary>
    Alt = 0x12,
    /// <summary>暂停键。</summary>
    Pause = 0x13,
    /// <summary>大写锁定键。</summary>
    CapsLock = 0x14,
    /// <summary>退出键。</summary>
    Escape = 0x1B,
    /// <summary>空格键。</summary>
    Space = 0x20,
    /// <summary>上一页键。</summary>
    PageUp = 0x21,
    /// <summary>下一页键。</summary>
    PageDown = 0x22,
    /// <summary>末尾键。</summary>
    End = 0x23,
    /// <summary>首页键。</summary>
    Home = 0x24,
    /// <summary>左方向键。</summary>
    Left = 0x25,
    /// <summary>上方向键。</summary>
    Up = 0x26,
    /// <summary>右方向键。</summary>
    Right = 0x27,
    /// <summary>下方向键。</summary>
    Down = 0x28,
    /// <summary>截屏键。</summary>
    PrintScreen = 0x2C,
    /// <summary>插入键。</summary>
    Insert = 0x2D,
    /// <summary>删除键。</summary>
    Delete = 0x2E,
    /// <summary>主键盘数字 0 键。</summary>
    D0 = 0x30,
    /// <summary>主键盘数字 1 键。</summary>
    D1 = 0x31,
    /// <summary>主键盘数字 2 键。</summary>
    D2 = 0x32,
    /// <summary>主键盘数字 3 键。</summary>
    D3 = 0x33,
    /// <summary>主键盘数字 4 键。</summary>
    D4 = 0x34,
    /// <summary>主键盘数字 5 键。</summary>
    D5 = 0x35,
    /// <summary>主键盘数字 6 键。</summary>
    D6 = 0x36,
    /// <summary>主键盘数字 7 键。</summary>
    D7 = 0x37,
    /// <summary>主键盘数字 8 键。</summary>
    D8 = 0x38,
    /// <summary>主键盘数字 9 键。</summary>
    D9 = 0x39,
    /// <summary>字母 A 键。</summary>
    A = 0x41,
    /// <summary>字母 B 键。</summary>
    B = 0x42,
    /// <summary>字母 C 键。</summary>
    C = 0x43,
    /// <summary>字母 D 键。</summary>
    D = 0x44,
    /// <summary>字母 E 键。</summary>
    E = 0x45,
    /// <summary>字母 F 键。</summary>
    F = 0x46,
    /// <summary>字母 G 键。</summary>
    G = 0x47,
    /// <summary>字母 H 键。</summary>
    H = 0x48,
    /// <summary>字母 I 键。</summary>
    I = 0x49,
    /// <summary>字母 J 键。</summary>
    J = 0x4A,
    /// <summary>字母 K 键。</summary>
    K = 0x4B,
    /// <summary>字母 L 键。</summary>
    L = 0x4C,
    /// <summary>字母 M 键。</summary>
    M = 0x4D,
    /// <summary>字母 N 键。</summary>
    N = 0x4E,
    /// <summary>字母 O 键。</summary>
    O = 0x4F,
    /// <summary>字母 P 键。</summary>
    P = 0x50,
    /// <summary>字母 Q 键。</summary>
    Q = 0x51,
    /// <summary>字母 R 键。</summary>
    R = 0x52,
    /// <summary>字母 S 键。</summary>
    S = 0x53,
    /// <summary>字母 T 键。</summary>
    T = 0x54,
    /// <summary>字母 U 键。</summary>
    U = 0x55,
    /// <summary>字母 V 键。</summary>
    V = 0x56,
    /// <summary>字母 W 键。</summary>
    W = 0x57,
    /// <summary>字母 X 键。</summary>
    X = 0x58,
    /// <summary>字母 Y 键。</summary>
    Y = 0x59,
    /// <summary>字母 Z 键。</summary>
    Z = 0x5A,
    /// <summary>左 Windows 键。</summary>
    LeftWindows = 0x5B,
    /// <summary>右 Windows 键。</summary>
    RightWindows = 0x5C,
    /// <summary>菜单键。</summary>
    Menu = 0x5D,
    /// <summary>小键盘数字 0 键。</summary>
    NumPad0 = 0x60,
    /// <summary>小键盘数字 1 键。</summary>
    NumPad1 = 0x61,
    /// <summary>小键盘数字 2 键。</summary>
    NumPad2 = 0x62,
    /// <summary>小键盘数字 3 键。</summary>
    NumPad3 = 0x63,
    /// <summary>小键盘数字 4 键。</summary>
    NumPad4 = 0x64,
    /// <summary>小键盘数字 5 键。</summary>
    NumPad5 = 0x65,
    /// <summary>小键盘数字 6 键。</summary>
    NumPad6 = 0x66,
    /// <summary>小键盘数字 7 键。</summary>
    NumPad7 = 0x67,
    /// <summary>小键盘数字 8 键。</summary>
    NumPad8 = 0x68,
    /// <summary>小键盘数字 9 键。</summary>
    NumPad9 = 0x69,
    /// <summary>小键盘乘号键。</summary>
    Multiply = 0x6A,
    /// <summary>小键盘加号键。</summary>
    Add = 0x6B,
    /// <summary>小键盘减号键。</summary>
    Subtract = 0x6D,
    /// <summary>小键盘小数点键。</summary>
    Decimal = 0x6E,
    /// <summary>小键盘除号键。</summary>
    Divide = 0x6F,
    /// <summary>F1 功能键。</summary>
    F1 = 0x70,
    /// <summary>F2 功能键。</summary>
    F2 = 0x71,
    /// <summary>F3 功能键。</summary>
    F3 = 0x72,
    /// <summary>F4 功能键。</summary>
    F4 = 0x73,
    /// <summary>F5 功能键。</summary>
    F5 = 0x74,
    /// <summary>F6 功能键。</summary>
    F6 = 0x75,
    /// <summary>F7 功能键。</summary>
    F7 = 0x76,
    /// <summary>F8 功能键。</summary>
    F8 = 0x77,
    /// <summary>F9 功能键。</summary>
    F9 = 0x78,
    /// <summary>F10 功能键。</summary>
    F10 = 0x79,
    /// <summary>F11 功能键。</summary>
    F11 = 0x7A,
    /// <summary>F12 功能键。</summary>
    F12 = 0x7B,
    /// <summary>F13 功能键。</summary>
    F13 = 0x7C,
    /// <summary>F14 功能键。</summary>
    F14 = 0x7D,
    /// <summary>F15 功能键。</summary>
    F15 = 0x7E,
    /// <summary>F16 功能键。</summary>
    F16 = 0x7F,
    /// <summary>F17 功能键。</summary>
    F17 = 0x80,
    /// <summary>F18 功能键。</summary>
    F18 = 0x81,
    /// <summary>F19 功能键。</summary>
    F19 = 0x82,
    /// <summary>F20 功能键。</summary>
    F20 = 0x83,
    /// <summary>F21 功能键。</summary>
    F21 = 0x84,
    /// <summary>F22 功能键。</summary>
    F22 = 0x85,
    /// <summary>F23 功能键。</summary>
    F23 = 0x86,
    /// <summary>F24 功能键。</summary>
    F24 = 0x87,
    /// <summary>小键盘锁定键。</summary>
    NumLock = 0x90,
    /// <summary>滚动锁定键。</summary>
    ScrollLock = 0x91,
    /// <summary>左Shift 键。</summary>
    LeftShift = 0xA0,
    /// <summary>右Shift 键。</summary>
    RightShift = 0xA1,
    /// <summary>左Ctrl 键。</summary>
    LeftControl = 0xA2,
    /// <summary>右Ctrl 键。</summary>
    RightControl = 0xA3,
    /// <summary>左Alt 键。</summary>
    LeftAlt = 0xA4,
    /// <summary>右Alt 键。</summary>
    RightAlt = 0xA5,
    /// <summary>分号键。</summary>
    Semicolon = 0xBA,
    /// <summary>等号键。</summary>
    Equals = 0xBB,
    /// <summary>逗号键。</summary>
    Comma = 0xBC,
    /// <summary>减号键。</summary>
    Minus = 0xBD,
    /// <summary>句号键。</summary>
    Period = 0xBE,
    /// <summary>斜杠键。</summary>
    Slash = 0xBF,
    /// <summary>反引号键。</summary>
    Backquote = 0xC0,
    /// <summary>左方括号键。</summary>
    LeftBracket = 0xDB,
    /// <summary>反斜杠键。</summary>
    Backslash = 0xDC,
    /// <summary>右方括号键。</summary>
    RightBracket = 0xDD,
    /// <summary>引号键。</summary>
    Quote = 0xDE,
 }
/// <summary>引擎支持的鼠标按键。</summary>
public enum MouseButton
{
    /// <summary>鼠标左键。</summary>
    Left,
    /// <summary>鼠标右键。</summary>
    Right,
    /// <summary>鼠标中键。</summary>
    Middle,
    /// <summary>鼠标后退侧键。</summary>
    Back,
    /// <summary>鼠标前进侧键。</summary>
    Forward,
}
/// <summary>与原生引擎编号一致的手柄按键。</summary>
public enum GamepadButton
{
    /// <summary>手柄 A 键。</summary>
    A,
    /// <summary>手柄 B 键。</summary>
    B,
    /// <summary>手柄 X 键。</summary>
    X,
    /// <summary>手柄 Y 键。</summary>
    Y,
    /// <summary>左肩键。</summary>
    LeftBumper,
    /// <summary>右肩键。</summary>
    RightBumper,
    /// <summary>左摇杆按下。</summary>
    LeftStick,
    /// <summary>右摇杆按下。</summary>
    RightStick,
    /// <summary>方向垫上键。</summary>
    DPadUp,
    /// <summary>方向垫下键。</summary>
    DPadDown,
    /// <summary>方向垫左键。</summary>
    DPadLeft,
    /// <summary>方向垫右键。</summary>
    DPadRight,
    /// <summary>手柄菜单键。</summary>
    Menu,
    /// <summary>手柄视图键。</summary>
    View,
}
/// <summary>手柄摇杆和扳机轴。</summary>
public enum GamepadAxis
{
    /// <summary>左摇杆横轴，范围 -1～1。</summary>
    LeftX,
    /// <summary>左摇杆纵轴，范围 -1～1。</summary>
    LeftY,
    /// <summary>右摇杆横轴，范围 -1～1。</summary>
    RightX,
    /// <summary>右摇杆纵轴，范围 -1～1。</summary>
    RightY,
    /// <summary>左扳机，范围 0～1。</summary>
    LeftTrigger,
    /// <summary>右扳机，范围 0～1。</summary>
    RightTrigger,
}
/// <summary>组合键盘方向与左摇杆的移动预设。</summary>
public enum InputPreset
{
    /// <summary>W/S/A/D 移动，并合成左摇杆输入。</summary>
    Wasd,
    /// <summary>方向键移动，并合成左摇杆输入。</summary>
    ArrowKeys,
}
