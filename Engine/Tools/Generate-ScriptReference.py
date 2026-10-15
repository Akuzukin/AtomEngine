"""Generate script API reference after building Atom.Managed."""
from pathlib import Path
import xml.etree.ElementTree as E
root=Path(__file__).resolve().parents[1]
xml=root/'Managed/Atom.Managed/bin/Release/net10.0/Atom.Managed.xml'
out=['# AtomEngine 脚本包 Reference','', '来源：Atom.Managed 公共 API 的 XML 文档。先编译 SDK，再运行 `python Engine/Tools/Generate-ScriptReference.py`。新增或修改 API 必须更新 XML 注释并重新生成本文件。','', '脚本使用 `using Atom;` 并继承 Behaviour。`new SaveStore("GameId")` 支持游戏自定义存档类型；`Object.AudioMixer` 控制音量总线；`Object.GetParticleComponent()` / `Object.FindParticleComponent(idOrName)` 查询粒子组件，`public ParticleComponent Effect;` 可接收 Inspector 组件拖拽，使用 `Effect.Play(position)` / `Effect.Stop()` 控制发射；`Application.Quit()` 正常退出当前进程。生命周期说明见 SCRIPT_API.md，粒子组件编辑与迁移见 GPU_PARTICLES.md。','']
out += ['编辑器 UI 布局与点击坐标使用实际 UI 合成缓冲；普通/放大视口和窗口缩放均同步其尺寸，3D拾取另用场景像素。排查坐标问题不能用场景渲染分辨率代替 UI 分辨率。', '']
out += ['底层主 Overlay 资源始终使用交换链像素尺寸，独立于3D渲染分辨率；离屏Overlay使用本视图尺寸。尺寸字段检查之外，锚点须验证最终GPU合成结果。', '']
for member in E.parse(xml).findall('./members/member'):
    summary=member.find('summary')
    out += ['## `'+member.get('name')+'`','',' '.join(''.join(summary.itertext()).split()) if summary is not None else '', '']
    for param in member.findall('param')+member.findall('typeparam'):
        out += ['- `'+param.get('name')+'`：'+' '.join(''.join(param.itertext()).split())]
    out += ['']
(root/'SCRIPT_REFERENCE.md').write_text('\n'.join(out),encoding='utf-8')
