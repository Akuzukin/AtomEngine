# 引擎 API 维护

添加或修改 Atom.Managed 公开 API 时，提供中文 XML summary 与参数文档，编译 SDK，运行 `python Engine/Tools/Generate-ScriptReference.py` 更新 `Engine/SCRIPT_REFERENCE.md`。组件修改需检查序列化、Inspector、生命周期、发行依赖和对应回归。
