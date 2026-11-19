# 齿轮生产 — CATIA V5-6R2016 (B26) 直齿轮对生成器

基于 **CATIA V5-6R2016 (`B26`)** 的纯解析渐开线直齿轮对生成工具，采用 **CAA C++ 独立批处理内核 (`GearPairWorker.exe`) + .NET Framework 4.0 WinForms 桌面端 (`GearPairCAA.exe`)**。

---

## 核心特性

- **纯解析渐开线建模**：基于种子零件 [`GearSeed.CATPart`](CAA_GearPair/win_b64/code/bin/GearSeed.CATPart) 中的原生 `GSMCurveEquations` 与 3 条 `CATICkeLaw`（`LogSpiral_X/Y/Z_StandardEquation`），通过 `CATICkeRelationExp::Modify(..., CATCke::False)` 写入标准渐开线解析参数方程（**零 Spline、零离散点拟合**）。
- **单 `.CATPart` 双实体集成**：在同一个输出零件 `GearPair_z<N1>_z<N2>.CATPart` 中构建主动轮 `主动轮_Gear1`（保留完整参数与特征历史）与从动轮 `从动轮_Gear2`（通过 `CATMmrInterPartCopy` 无链接拷贝 + `CATIMf3DAxisSystemFactory` / `CATIPrtFactory::CreateAxisToAxis` 空间定位并对齐啮合相位 $\pi + \pi/z_2$）。
- **完整几何参数支持**：
  - 模数 $m$：WinForms 下拉提供常用标准模数（`0.5 / 0.8 / 1 / 1.25 / 1.5 / 2 / 2.5 / 3 / 4 / 5 / 6 / 8 / 10`），勾选 `非标模数 (自填)` 后支持任意正实数。
  - 齿数 $z_1, z_2$、齿宽 $b_1, b_2$、法向压力角 $\alpha$、变位系数 $x_1, x_2$、总齿侧间隙 $j_n$。
  - **齿顶/齿根倒圆 (`filletR`)**：默认 `0.1 mm`（设为 `0` 时跳过倒圆保持尖角），通过双相切圆弧、极角单调性筛选与轮廓裁剪生成。
- **空间坐标定位与轴向**：
  - 支持齿轮轴向 `Y`（默认）/ `Z` / `X`。
  - 支持指定主动轮中心 `P1`，以及可选指定从动轮中心 `P2`（垂直轴向投影距离满足工作中心距 $a_w \pm 0.01\text{ mm}$ 时直接采用，否则沿 `P1→P2` 投影方向按 $a_w$ 自动定距）。
- **模型显隐与啮合参考特征**：
  - 自动隐藏 `xy/yz/zx` 基准面、局部坐标系及 `齿轮线框` 几何图形集。
  - 仅显示双齿轮实体与 `啮合基准特征` 几何图形集（黄色节圆/圆心/节点、青蓝色轴线、绿色中心连线）。
- **交互与容错**：
  - 实时计算中心距 $a_w$、啮合角 $\alpha'$、重合度 $\varepsilon$ 与两轮节/基/顶/根圆尺寸。
  - 自动清理粘贴参数末尾的 `mm`、`deg`、`°` 单位后缀；通过 UTF-8 无 BOM 临时 `GEAR_JOB_FILE` 传参，避免中文路径与命令行引号问题。
  - 支持生成完成后通过 COM 自动关闭旧同名文档、在运行中的 CATIA 中打开并居中显示（`Reframe` + `Save`）。

---

## 目录结构

```text
.
├── README.md                                 # 项目总览与快速上手（本文件）
├── HANDOFF.md                                # 详细工程交接文档（架构、源码索引、排坑与待办）
├── AGENTS.md                                 # AI 智能体接手指南（硬约束、构建验证命令、修改清单）
├── .gitignore                                # 排除输出零件、构建缓存与本机运行时文件
├── GearPairCAA.exe                           # WinForms 桌面端可执行文件
└── CAA_GearPair/                             # CATIA B26 CAA C++ 内核 + WinForms 前端
    ├── CATIAV5Level.lvl                      # B26 级次定义
    ├── build_caa_gear.bat                    # VS2012 x64 + TCK + mkmk 一键编译脚本
    ├── run_gear_caa.bat                      # 无界面命令行冒烟测试脚本
    ├── GearPairCAA_UI.vb                     # WinForms 前端源码 (419 行)
    ├── GearPair.ico                          # 双齿轮啮合图标
    ├── GearPairGen.edu/
    │   ├── IdentityCard/IdentityCard.xml     # CAA Framework 依赖声明
    │   └── GearPairWorker.m/
    │       ├── Imakefile.mk                  # LOAD MODULE 链接库清单
    │       └── src/GearPairWorker.cpp        # CAA C++ 核心建模源码 (1,641 行)
    └── win_b64/                              # CAA 运行时视图（dictionary / productIC / control / bin）
        └── code/bin/
            ├── GearPairWorker.exe            # 编译好的 CAA C++ 批处理内核
            └── GearSeed.CATPart              # 含原生 GSMCurveEquations 的种子模板
```

> 说明：本仓库遵循 `.gitignore`，**不包含**测试输出目录（`Output_CAA/`）、`mkmk`/MSVC 编译中间产物以及与本机环境相关的运行时 `.dll` 文件；本机若需独立运行 `CAA_GearPair`，需确保 CATIA B26 运行环境完整（详见 [HANDOFF.md](HANDOFF.md)）。

---

## 环境依赖与快速开始

### 运行环境要求
- **操作系统**：Windows 64-bit
- **CAD 平台**：CATIA V5-6R2016 (`B26`)，默认环境名 `CATIA_P3.V5-6R2016.B26`，环境目录 `C:\ProgramData\DassaultSystemes\CATEnv`
- **桌面端运行库**：.NET Framework 4.0+（Windows 10/11 默认自带）
- **C++ 二次编译额外要求**：Visual Studio 2012 (VS 11.0 `vcvarsall.bat amd64`) + CATIA B26 CAA RADE / TCK 工具链

### 方式一：直接使用桌面端
1. 双击根目录下的 `GearPairCAA.exe`（默认无需先打开 CATIA 即可后台建模；若勾选“生成后在 CATIA 中打开并居中显示”，建议先打开 CATIA V5-6R2016 窗口）。
2. 设置模数、齿数、齿宽、压力角、变位系数、侧隙、倒角半径 $R$、轴向及中心坐标，点击 **“生成双齿轮零件 (.CATPart)”**。
3. 默认输出至 `c:\齿轮生成\Output_CAA\GearPair_z<N1>_z<N2>.CATPart`（可在界面修改输出目录）。

### 方式二：命令行无界面冒烟测试
```bat
cmd /c "CAA_GearPair\run_gear_caa.bat"
```
或自定义参数调用：
```bat
cmd /c "CAA_GearPair\run_gear_caa.bat" "<输出目录>" <m> <z1> <z2> <b1> <b2> <alpha> <x1> <x2>
```

### 从源码重新编译

1. **编译 CAA C++ 内核 (`GearPairWorker.exe`)**：
   ```bat
   CAA_GearPair\build_caa_gear.bat
   ```
2. **编译 WinForms 前端 (`GearPairCAA.exe`)**（编译前请关闭运行中的 `GearPairCAA.exe`）：
   ```bat
   C:\Windows\Microsoft.NET\Framework64\v4.0.30319\vbc.exe /nologo /target:winexe /win32icon:"CAA_GearPair\GearPair.ico" /out:"GearPairCAA.exe" /r:System.dll,System.Drawing.dll,System.Windows.Forms.dll "CAA_GearPair\GearPairCAA_UI.vb"
   ```

---

## 交接与开发文档导航

- **面向研发/维护工程师**：请阅读 [HANDOFF.md](HANDOFF.md)，包含完整数据流图、`GEAR_JOB_FILE` 协议、1,641 行 C++ 内核与 419 行 VB.NET 前端逐函数索引、环境迁移指南、关键排坑记录与后续演进建议。
- **面向 AI 编程智能体**：请优先阅读 [AGENTS.md](AGENTS.md)，包含工作区真源边界、不可破坏的不变量、一键构建/验证命令及端到端新增参数检查表。
