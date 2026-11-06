# Native authoring example / 原生开发示例

`Mod.cpp` uses only `<BML/Mod.hpp>` and `BML::BML`. Context binds the common
Config/Input/Timer/Command services. Declarations keep defaults and comments
in one place; callbacks are ordinary lambdas. `BML_DEFINE_MOD` declares named metadata fields directly. Registration members express
ownership; the host cleans them before destroying the Instance.

只需 Mod.hpp 和核心 BML::BML。通过 ctx.Config()/Input()/Timers()/Commands() 取得服务，
BML_DEFINE_MOD 直接填写元数据，无需额外 Define 函数；一次写完配置默认值和说明。普通 lambda 可作为定时器回调，注册成员明确表达所有权。
失败的 Take() 会携带错误码进入当前回调诊断，异常不跨 C ABI。

```sh
cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/installed/sdk -DCMAKE_BUILD_TYPE=Release
cmake --build build
cmake --install build --prefix /path/to/game/ModLoader
```

Keep the `.bmodp` and `.bmodp.abi.json` together. No engine or ImGui extension
is needed for this example. / 同时携带模块及 ABI 伴随文件，本示例不依赖引擎或 ImGui 扩展。
