# 版权与分发范围

根目录 MIT License 适用于本项目新增的补丁实现、工具和文档中由项目贡献者拥有权利的部分。它不改变原厂固件、设备商标、原厂字体、第三方库和第三方素材的许可。

| 内容 | 处理方式 |
| --- | --- |
| 本项目新增 C / ARM 汇编 / Python 实现 | MIT，保留文件中已有的第三方声明 |
| BearSSL 0.6 | 上游 MIT；见 `third_party/bearssl-0.6/LICENSE.txt` |
| Opus 1.5.2 | 保留完整 COPYING 及其中的专利许可指引；见 `licenses/Opus-COPYING.txt` |
| Noto Emoji 笑脸及派生帧 | 保留 `licenses/LICENSE-Noto` 和 `licenses/emoji-provenance.json` |
| 素材生成器的第三方声明 | 见 `licenses/LICENSE-generator`，不要用本项目作者替换其版权主体 |
| 清风开机动画、播放模式图标 | 项目使用的界面素材及生成代码；来源、源文件散列记录在 `assets/`，不表示拥有原厂其他图像的权利 |
| 原厂固件、完整 OTA、原厂字库提取数据 | 不在公开仓库中分发；由使用者本地提供，根目录 MIT 不覆盖它们 |
| TLS 根证书 | 公共信任锚，不包含私钥；来源及 SHA-256 见 `build-support/tls/verified-root-provenance.json` |

本项目为非官方实验性补丁工程，不代表原设备厂商或小智服务运营方。完整系统由原厂闭源基础与本项目补丁组合，不能称为“全部固件源码开源”。

软件界面预览仅用于说明布局，包含原系统字体和界面的渲染结果；它们不是实机照片，也不授予原厂素材的独立再分发许可。
