# B26 直齿轮原型

启动 CATIA V5-6R2016，在 PowerShell 运行：

```powershell
cscript.exe //nologo "C:\齿轮生成\CodexGear\Build-SpurPair.vbs"
```

脚本生成一个 CATPart 并在 CATIA 中打开。输出路径和步骤状态显示在控制台。生成期间关闭 CATIA 文件提示，退出时恢复原设置。输出包含 Gear_34 和 Gear_60 两个 Body，中心距 47 mm。

34 齿轮保留方程曲线、阵列和参数历史。60 齿轮先在临时文档中按方程建模，再将实体结果无链接地写入同一个 CATPart，临时文件随后清理。当前样件参数为模数 1 mm、压力角 20°、齿宽 6 mm、零变位和零侧隙。Verify-SpurPair.vbs 检查活动 CATPart 的两个实体及更新状态。

## VB.NET 圆角版

双击 GearPairR.exe，保留同目录的 EquationSeed.CATPart。界面可设两轮齿数、共同模数、主动轮基准端面中心、轴向和 R 值。默认 34/60 齿、模数 1 mm、中心 (0,0,0)、+Y、R0.10 mm。

圆角作用于齿顶与齿侧、齿根与齿侧之间沿齿宽的棱。R=0 可关闭圆角。当前界面范围为 18–120 齿、模数 0.1–20 mm、R 不超过模数的 0.2 倍。标准压力角为 20°，齿宽 6 mm，零变位，中心距为模数乘两轮齿数和的一半。输出到 Output，两个 Body 位于同一个 CATPart。主动轮保留历史，从动轮实体以无外链结果保存。
