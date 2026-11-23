# GitHub 发布准备

公开仓库仅包含源码、构建与检查工具、素材、许可证、V0141 描述和脱敏验证记录。完整镜像与所有 `_local/` 文件不在公开 Git 文件集中。

发布前在仓库根目录运行：

```sh
python3 scripts/run_checks.py
python3 scripts/check_public_tree.py --tracked
git status --short
```

确认只跟踪预期公开文件后，再提交并推送到你自己的 GitHub 仓库。首次上传可在 GitHub 创建空仓库，再使用页面给出的远程地址：

```sh
git add .
python3 scripts/check_public_tree.py --tracked
git commit -m "Prepare C208 V0141 source release"
git branch -M main
git remote add origin <你的仓库远程地址>
git push -u origin main
```

占位符需替换为自己的地址；作者身份按自己的 Git 配置设置。当前本地整理不创建远程仓库、不发送文件或消息，也不自动上传 GitHub。

不要把整个桌面目录打包上传。准备好的公开上传压缩包只取实际 Git 文件集，不包含 `.git`、私有输入或构建目录。

如果以后决定公开分发完整 OTA，应单独审核原厂代码与素材的分发权利、vendor／身份数据、实际设备适配及验证状态。现在的 `.gitignore` 和公开检查会阻止它进入源码仓库，不能通过强制加入来代替审核。该原则不表示现有完整 OTA 已经完成公开分发审核。

GitHub Release 说明应继续标明“实验版本”和已验证范围，不应使用“永久防砖”“刷黑必救”“原厂源码全部开源”等表述。
