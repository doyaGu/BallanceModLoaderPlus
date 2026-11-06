# BML+ SDK

[English](README.md)

这个压缩包用于开发 Mod，不是 BML+ 运行时包，不能直接解压到 Ballance 游戏目录。

## Windows、Linux 和 macOS 上的 SDL SDK

SDK 必须匹配游戏的运行时、平台、架构、宿主链接方式、编译器、C++ 标准库和运行库。
SDL CLI 目前支持动态宿主。

Linux 和 macOS 使用 Python 3.10+ 及有执行权限的 POSIX 入口：

```sh
"<BML-SDK>/scripts/bml" new native yourname.my-mod
cd MyMod
./bml build
./bml run --ballance-root "/path/to/game"
./bml pack
```

脚本项目改用 `new script`，其余命令一致。Windows 继续使用 `bml.cmd`。
SDL 原生构建默认采用 `share/BML/BML.abi.json` 中的配置，需要匹配的 CK2、VxMath
和 SDL3 开发包，不使用原版 Virtools SDK。额外依赖的安装前缀可用 `--package-root`
传入。POSIX 默认选择 Ninja 或 Unix Makefiles，Windows 默认选择 Visual Studio；
可用 `--generator` 指定其他生成器。在首次构建前用 `CXX` 选择匹配的编译器。
Windows 使用非 Visual Studio 生成器时，需要先初始化编译器环境。

游戏路径填写根目录。支持 `Bin/Player`（Windows 为 `Bin/Player.exe`），以及与
`base.cmo` 并列的扁平 `Player`。运行时须有 `ModLoader/BML.abi.json`；原生部署会
在替换文件前核对其与 SDK 的兼容性。脚本运行要求 `Managers` 中的 BML 和
`BuildingBlocks` 中的 CKAngelScript，文件名采用对应平台的动态库格式。

`build` 同时准备 `.bmodp` 和 `.bmodp.abi.json`。SDL 的 `pack` 生成包含两者的 ZIP，
采用原子替换，并用 `--force` 防止意外覆盖。将 ZIP 放入 `ModLoader/Mods`，也可
同时复制两个构建产物；缺少 ABI 描述的 SDL `.bmodp` 会被拒绝。SDK 可移动目录；不要将 SDK 的引擎库复制进
现有游戏安装。

SDL Mod 复用 Player 随包的 SDL，macOS SDL 构建需要 CMake 3.19 或更新版本。
macOS 上，即使开发环境 SDL 使用绝对安装路径，
`bml_add_mod` 也会在链接后将 Mod 的 SDL 依赖改为 `@rpath` 并更新临时签名。
构建出的 `.bmodp` 可直接复制到 `ModLoader/Mods`；部署时无需修改二进制，也不要
另外携带 SDL。需要发行签名时，请在构建完成后使用自己的签名身份签名。

## CMake

```cmake
find_package(BML CONFIG REQUIRED)
bml_add_mod(MyMod src/MyMod.cpp)
bml_install_mod(MyMod)
```

Use `basic`, `interface-provider`, `interface-consumer`, or `imc-provider` with
`bml new native --profile`. Script projects use `bml new script`.
The full guides are installed under `share/BML/docs`.
