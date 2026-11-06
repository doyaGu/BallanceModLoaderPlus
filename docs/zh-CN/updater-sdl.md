# SDL 更新器与 doctor

SDL 更新器是独立可执行文件。文件操作使用 SDL，HTTPS 下载使用 libcurl 7.68+，
Windows/macOS 使用系统加密接口；Linux 静态编入 RHash SHA-256 和 p256-m。
BML+ 不直接依赖 OpenSSL；libcurl 的 TLS 后端取决于它自身的构建。原版引擎的
schema 1 更新器不属于本分支。当前 Loader 仅支持动态宿主，下述静态包分流
不表示已完成静态 Player 集成。

```sh
./Bin/Updater doctor --game-root "/path/to/game"
./Bin/Updater configure https://example.org/bml --game-root "/path/to/game"
./Bin/Updater check --game-root "/path/to/game"
./Bin/Updater plan --game-root "/path/to/game"
./Bin/Updater update --game-root "/path/to/game"
```

Windows 使用 `Bin\Updater.exe`，SDL 命令相同。`--game-root` 填写游戏根目录，支持
`Bin/Player` 标准布局和与 `base.cmo` 并列的扁平 `Player`。省略时，位于 `Bin` 的
Updater 默认使用上级目录。路径与控制台文字使用 UTF-8；POSIX Player 须有执行权限。

`doctor` 检查 ABI 描述、Player 与动态库的二进制格式及架构、SDL 版本、启用脚本时
的脚本运行时、重复 Loader、Player 进程、状态目录写入权限及中断事务。它不执行
插件入口，也不能代替游戏验收。静态宿主会检查 Player 和元数据，内嵌插件的注册
仍须在真实 Player 中验收。

## 更新目标与签名

SDL 清单与通道使用 `schemaVersion: 2`，包含以下目标对象：

```json
{"runtime":"sdl","platform":"macos","architecture":"arm64","host_linkage":"dynamic"}
```

平台为 `windows`、`linux`、`macos`，架构为 `x86`、`x64`、`arm64`，x86 仅用于
Windows。配置的源地址按
`<base>/sdl/<平台>/<架构>/<宿主链接方式>/stable.json` 或 `beta.json` 查找通道。
签名通道、签名清单、已安装的 `ModLoader/BML.abi.json` 和载荷 ABI 描述必须一致。
缺失描述或目标不匹配时拒绝应用。已有原生 Mod 的编译器与 SDK ABI 兼容性仍由
Loader 检查；更新器不会自动迁移原生 Mod 二进制。

动态包使用 `packageType: "bml-runtime"`，包含 Loader、可选 CKAngelScript、字体、
ABI 元数据及所属运行时依赖，不更新 Player、CK2、VxMath 或 SDL。静态包使用
`packageType: "player-runtime"`，更新匹配的完整 Player 运行时，不能用于动态宿主。
静态包目标分流已实现；真实 BML 静态 Player 的构建和集成仍是单独的待完成事项。

每个文件记录 `path`、SHA-256、精确字节数 `size` 和权限 `mode`：`420` 表示 0644，
`493` 表示 0755。用户 Mods、配置（含 Player.ini）、Updater 状态及 Updater 自身不纳入更新。
显式删除仅处理上一次已签名安装清单中登记、且当前内容哈希未变的文件。

分离签名沿用 win32 格式：对原始 JSON 字节做 SHA-256，再用 ECDSA P-256 签名，
将 64 字节的 `r || s` 编为 base64。HTTPS 校验证书和主机名，重定向仅允许 HTTPS。
可用 `CURL_CA_BUNDLE` 或 `SSL_CERT_FILE` 指定 CA 文件，不关闭证书校验。
下载失败会清理临时文件，重试时重新下载。

## 失败恢复

应用和回滚前须关闭 Player。更新器使用排他进程锁，先复制并校验备份，再刷新事务
日志与 pending 标记，之后才替换文件。替换采用原子操作，并记录、恢复 POSIX 权限。
状态元数据、原始安装清单和签名也参与同一事务。

```sh
./Bin/Updater verify-local /path/to/BMLPlus-Update-....zip --game-root /path/to/game
./Bin/Updater apply-local /path/to/BMLPlus-Update-....zip --game-root /path/to/game
./Bin/Updater rollback --game-root /path/to/game
```

应用失败会尝试回滚；回滚失败会保留日志与 pending 标记。进程意外退出后，新更新
会被阻止，须先执行 `rollback`。回滚本身被中断后也可再次执行。不要删除备份来
消除错误，应先恢复写入权限或缺失的备份文件。成功事务会保留备份以供手动回滚。
Updater 可执行文件仍通过完整手动包更新。

## 生成签名更新包

使用全新暂存目录，或用 CMake 组件安装清单从暂存游戏中选择所属文件，不要直接
把现有用户安装目录作为更新载荷扫描。安装清单也会排除 Player 提供的宿主库。

```sh
cmake --build build --target BML Updater BMLUpdateSigner
cmake --install build --component BMLRuntime --prefix /path/to/staged-game
python3 scripts/package_sdl_update.py \
  --stage /path/to/staged-game \
  --install-manifest build/install_manifest_BMLRuntime.txt \
  --output /path/to/release --version v0.3.14 \
  --signing-key /private/path/p256-key.pem \
  --signer build/Bin/BMLUpdateSigner \
  --base-url https://example.org/bml/artifacts
```

macOS 重定位需要暂存目录中已有匹配的宿主 SDL 库。工具生成 ZIP、清单和签名；
指定 `--base-url` 时还会生成按目标分目录的签名通道，并保留已有撤销列表。
覆盖输出须使用 `--force`。版本化产物发布到 `--base-url`，目标通道目录树发布到
更新器配置的源地址下。签名私钥须匹配正式更新器内嵌的公钥。测试公钥仅用于
单独构建、不会安装的测试可执行文件。

当前已在 macOS ARM64 完成构建、临时运行时夹具、本机 HTTPS、签名、拒绝、权限
及中断恢复测试。Windows/Linux 原生运行、真实静态 Player 集成和真实游戏更新
验收仍待完成，详见带日期的[验证记录](validation.md)。

`BMLUpdateSigner` 接受未加密的 P-256 SEC1/PKCS#8 PEM 或 DER 私钥，使用系统
随机源签名，输出原有的 64 字节原始签名，由 Python 工具进行 base64 编码。
该发布工具独立于 Player 与运行时 Updater，第三方许可随包放在 `ModLoader/Licenses`。
