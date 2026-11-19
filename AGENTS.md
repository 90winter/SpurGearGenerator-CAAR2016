# AI 智能体接手指南 (`AGENTS.md`)

> 本文件专为后续接手本仓库的 AI 编程智能体（Coding Agents）编写，提供最短路径的上下文定位、不可破坏的不变量、精确构建/验证命令以及端到端修改清单。

---

## 1. 仓库真相源（Source of Truth）

- **当前活跃主线**：根目录 [`GearPairCAA.exe`](GearPairCAA.exe) + [`CAA_GearPair/`](CAA_GearPair/)
  - C++ 建模内核：[`CAA_GearPair/GearPairGen.edu/GearPairWorker.m/src/GearPairWorker.cpp`](CAA_GearPair/GearPairGen.edu/GearPairWorker.m/src/GearPairWorker.cpp)
  - WinForms 桌面前端：[`CAA_GearPair/GearPairCAA_UI.vb`](CAA_GearPair/GearPairCAA_UI.vb)
  - CAA 配置：[`IdentityCard.xml`](CAA_GearPair/GearPairGen.edu/IdentityCard/IdentityCard.xml)、[`Imakefile.mk`](CAA_GearPair/GearPairGen.edu/GearPairWorker.m/Imakefile.mk)、[`CATIAV5Level.lvl`](CAA_GearPair/CATIAV5Level.lvl)
- **冻结的历史原型**：[`CodexGear/`](CodexGear/)（VB.NET / VBS + COM Automation）。除非用户明确要求修改 `CodexGear`，否则**不要改动该目录**，所有新功能与缺陷修复只落在 `CAA_GearPair/`。
- **推荐首轮阅读顺序**：`AGENTS.md`（本文件）→ [`HANDOFF.md`](HANDOFF.md) → 按任务只读取 [`GearPairWorker.cpp`](CAA_GearPair/GearPairGen.edu/GearPairWorker.m/src/GearPairWorker.cpp) 或 [`GearPairCAA_UI.vb`](CAA_GearPair/GearPairCAA_UI.vb) 的目标行区间。

---

## 2. 硬性不变量（严禁破坏）

1. **禁止更换曲线生成方案**：必须保持 `GearSeed.CATPart` 原生 `GSMCurveEquations` + `CATICkeLaw` 解析参数方程方案，严禁降级为 Spline 或离散点拟合。
2. **保护种子模板与特征别名**：
   - [`CAA_GearPair/win_b64/code/bin/GearSeed.CATPart`](CAA_GearPair/win_b64/code/bin/GearSeed.CATPart) 为核心运行时依赖，不可删除或覆盖损坏。
   - 种子内 `LogSpiral_Equation` 几何集、`LogSpiral_Standard_Equation` 曲线与 `LogSpiral_X_StandardEquation` / `LogSpiral_Y_StandardEquation` / `LogSpiral_Z_StandardEquation` 3 条 Law 是 `BuildSingleGearDoc` 的硬依赖契约。
3. **保护运行时视图目录与 Git 洁净度**：
   - 保留 `CAA_GearPair/win_b64/code/dictionary/`、`productIC/`、`control/`，否则 `CATSTART` + `GearPairWorker.exe` 会启动失败。
   - 严格遵守 `.gitignore`：严禁提交任何 `*.dll`、生成输出（`Output_CAA/`、`*.CATPart` 除两个种子外）、`mkmk` 构建中间目录（`Objects/`、`LocalGenerated/`、`ToolsData/`、`Install_config_win_b64/`、`various/`、`ImportedInterfaces/`）以及 `*.pdb`/`*.exp`/`*.lib`/`*.log`/`*.tmp`。
4. **注意 CATIA CKE 单位差异**：
   - `CATICkeParmFactory::CreateLength` 使用 **米 (m)**（需 `/ 1000.0`），`CreateAngle` 使用 **弧度 (rad)**。
   - `CATIPad` 拉伸长度、CGM 体积/包围盒检查、界面/任务文件数值均使用 **毫米 (mm)** 与 **度 (°)**。
   - 添加公式与 `CATICkeRelationExp::Modify` 必须使用位置参数 `a1..aN` 并传 `CATCke::False`。
5. **前后端协议与预览同步**：新增或修改任何参数时，必须同时更新 UI 预览计算与 `GEAR_JOB_FILE` 解析逻辑，确保预览中心距/尺寸与实际生成模型一致。
6. **编译前文件锁检查**：重编 `GearPairCAA.exe` 前确认桌面没有运行中的 `GearPairCAA.exe` 进程，否则 `vbc.exe` 报 `BC31019`。

---

## 3. 精确构建与验证命令（Windows）

### 编译 CAA C++ 内核 (`GearPairWorker.exe`)
```bat
c:\齿轮生成\CAA_GearPair\build_caa_gear.bat
```
- 成功标志：控制台输出 `=== Build Finished with code 0 ===` 且生成 `CAA_GearPair\win_b64\code\bin\GearPairWorker.exe`。
- 说明：`CATSTART.exe` 运行时在 stderr 输出 `MecModLiveInterfaces.iid line 1 ... CATIBRepModeCont` 属于本机 B26 环境无害字典提示，不代表失败。

### 编译 WinForms 前端 (`GearPairCAA.exe`)
```bat
C:\Windows\Microsoft.NET\Framework64\v4.0.30319\vbc.exe /nologo /target:winexe /win32icon:"c:\齿轮生成\CAA_GearPair\GearPair.ico" /out:"c:\齿轮生成\GearPairCAA.exe" /r:System.dll,System.Drawing.dll,System.Windows.Forms.dll "c:\齿轮生成\CAA_GearPair\GearPairCAA_UI.vb"
```

### 命令行无界面冒烟测试（修改代码后必跑）
```bat
cmd /c "c:\齿轮生成\CAA_GearPair\run_gear_caa.bat"
```
**必须验证以下全部输出信号**：
- 进程退出码为 `0`（`EXIT=0`）
- 存在 `[VERIFIED SINGLE_PART_GEAR1] Vol=...` 与 `[VERIFIED SINGLE_PART_GEAR2] Vol=...` 且体积均 `> 0`
- 存在 `[VERIFIED REF Pitch] rw1=... rw2=...` 与 `PitchC1/PitchC2/CenterLine/Shaft1/Shaft2` 包围盒打印
- 结尾出现 `=== GearPairWorker finished: SUCCESS ===`
- 输出目录下生成 `GearPair_z<N1>_z<N2>.CATPart`（测试完毕后注意清理临时生成的输出文件，避免污染仓库）

### 额外参数化重建验证（可选）
`GearPairWorker.exe` 支持 `--verify-z <partPath> <newZ...>` 分支（见 `GearPairWorker.cpp:1576`），用于在批处理中验证打开已生成零件并修改 `齿数z` 后是否能成功 `Update`。

---

## 4. 端到端新增/修改参数的 4 处检查清单

1. **WinForms 控件与输入清理**（`GearPairCAA_UI.vb`）：新增控件并加入 `TextChanged` 监听；数值解析统一走 `CleanNumStr` + `InvariantCulture`。
2. **预览与 `GEAR_JOB_FILE` 写入**（`GearPairCAA_UI.vb`）：更新 `UpdatePreview`（`L203`）与 `OnGenerate` 中的 `jobLines` 数组（`L352–L373`），文件保持 **UTF-8 无 BOM**。
3. **C++ 解析与数学计算**（`GearPairWorker.cpp`）：更新结构体、`LoadUtf8JobFile`（`L1258`）及 `ComputeGearPair` / `ResolvePlacement`。
4. **建模/验证逻辑**（`GearPairWorker.cpp`）：同步更新 `BuildSingleGearDoc` 或 `BuildSinglePartGearPair`，并在改动依赖库时检查 `IdentityCard.xml` 与 `Imakefile.mk`。

---

## 5. 常见故障速查

| 现象 | 常见原因 | 排查方向 |
| --- | --- | --- |
| `vbc.exe` 报错 `BC31019` | `GearPairCAA.exe` 正在运行中导致 EXE 被锁定 | 提示用户关闭或结束进程后再编译 |
| `FindCornerByRadii` 报错无解 | `filletR` 过大（超出齿顶/齿根弧段空间）或极角条件过严 | 用 `filletR=0` 验证基础轮廓是否正常，检查 `thetaTip`/`thetaRoot` 与圆弧半径比例 |
| 中文路径或参数乱码 | 未使用 UTF-8 无 BOM 写 `GEAR_JOB_FILE` 或未走 `BytesToWide(..., CP_UTF8)` | 检查 `GearPairCAA_UI.vb` 的 `UTF8Encoding(False)` 与 `LoadUtf8JobFile` |
| `Create_Session` / `CATSTART` 失败 | `CATEnv` 名称/路径与本机 B26 不匹配，或 `win_b64` 运行时组件不完整 | 核对 `CATIA_P3.V5-6R2016.B26`、环境目录及重新执行 `build_caa_gear.bat` |
| 双实体更新后某轮缺失 | `CATMmrInterPartCopy` 或 `CreateAxisToAxis` 失败 | 检查 `SafeUpdate` 打印的 `CATMfErrUpdate` 诊断信息 |
