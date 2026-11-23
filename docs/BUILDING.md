# 构建与本地输入

这是一套固定基线的二进制补丁工程。公开源码涵盖新增功能；原厂 SDK 完整工程、闭源驱动源码、原厂镜像和 V0121 基线不随仓库分发。仅克隆仓库不能凭空重建闭源基础。构建前必须自行取得有权使用且校验匹配的输入。

## 工具

- Python 3.10+。常规构建与校验仅使用标准库。
- Clang：支持 `--target=armv7m-none-eabi -mcpu=cortex-m4 -mthumb -mfloat-abi=soft`。
- LLVM `ld.lld`、`llvm-ar`。使用 PATH、`LLVM_BIN` 或单独的 `C208_CLANG`、`C208_LD_LLD`、`C208_LLVM_AR` 环境变量选择工具，不依赖作者电脑目录。
- 字库子集、图标和动画再生成用 Pillow；固件直接构建可使用已有素材和私有字体头文件。

不同 Clang／链接器版本可能生成不同机器码。当前校验器钉住已交付 V0141 的散列；源码修改或工具链差异后的构建应作为新候选重新验证，不能冒用原发行散列。

## 私有输入

在仓库根目录建立以下路径，全部已被 Git 忽略：

```text
_local/inputs/sg-bak.bin                     8MiB 原始镜像
_local/inputs/v0121-base/SOGOUOTA.BIN        指定的 V0121 构建基线
_local/inputs/record-pages.bin              基线原生录音页的两条20字节表项
_local/inputs/mode_font.h                   从本地原厂字库生成的子集
_local/inputs/mode_font.json                字库生成记录
_local/build/tls/                           本机构建的 BearSSL／适配层静态库及大小报告
_local/build/opus/                          本机构建的 Opus 静态库及报告
```

五项私有输入的精确大小和 SHA-256 记录在 `build-support/private-inputs.json`。原始镜像 SHA-256：

```text
20d63efd687fdfbf76ae979bc04ca472162d07d5f6addc1c53fc9e21ed7cb204
```

V0121 OTA 基线 SHA-256：

```text
c484e9468bae8de8ef379965290749da7cffe2eebc5a5881accffb67a7da7741
```

`record-pages.bin` 为历史 V0098 基线页面表中第 8、9 项共 40 字节；它也是本地二进制输入，不能随意用空文件代替。本地整理副本已保留这些必要输入。公开仓库的布局元数据并不包含相应完整原厂固件。

可用以下命令检查本地输入，工具只读，不访问设备：

```sh
python3 scripts/check_private_inputs.py
```

重新生成私有字体子集：

```sh
python3 scripts/build_xiaozhi_mode_font.py
```

输出进入 `_local/inputs/`，不会把原厂字库复制进公开源码目录。

## 构建

第一次从源码构建时先生成两个库：

```sh
python3 scripts/build_xiaozhi_tls.py
python3 scripts/build_xiaozhi_opus.py
python3 scripts/build_firmware.py --output-directory build/V0141
```

若本地已保留经过核对的静态库，可直接运行最后一条。构建器拒绝覆盖已存在的候选目录；需要再次构建时使用新的输出目录。

输出：`SOGOUOTA.BIN`、`manifest.json` 和 `main.elf`。构建不接触设备，不把候选包自动复制到设备，也不自动发布。

本次整理使用保留的已核对静态库，在当前 macOS 工具链下重新构建，结果与原 V0141 OTA 逐字节相同。该结果不代表所有操作系统／编译器组合都可得到相同散列；本轮没有重新编译全部第三方库。

## 验证与升级工具

```sh
python3 scripts/run_checks.py
python3 scripts/verify_release.py \
  --package build/V0141/SOGOUOTA.BIN \
  --original _local/inputs/sg-bak.bin \
  --elf build/V0141/main.elf
```

`verify_release.py` 会检查已发行 V0141 的精确散列、容器、版本字段、两份 RESC、受保护分区和静态区布局。它不证明实际 Flash 时序、设备适配或主板能够恢复。

只读检查设备目录时，可使用 `scripts/check_current_ota.py --summary <设备目录>`。它读取指定 OTA／诊断文件，不读取私有身份或录音；不能从磁盘文件推断当前 SPI 运行版本。

`scripts/stage_xiaozhi_official.py` 会实际写入明确选择的设备目录，请先读 `FLASHING.md`、`PRIVACY.md` 并查看 `--help`。备份旧 OTA 文件不等于读取旧系统镜像。
