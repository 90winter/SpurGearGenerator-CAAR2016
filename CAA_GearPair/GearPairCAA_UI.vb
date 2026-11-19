Imports System
Imports System.Diagnostics
Imports System.Drawing
Imports System.Globalization
Imports System.IO
Imports System.Runtime.InteropServices
Imports System.Text
Imports System.Threading.Tasks
Imports System.Windows.Forms

Public Class MainForm
    Inherits Form

    Private txtM, txtAlpha, txtZ1, txtZ2, txtB1, txtB2, txtX1, txtX2, txtBacklash, txtFilletR, txtOutDir As TextBox
    Private txtCx1, txtCy1, txtCz1, txtCx2, txtCy2, txtCz2 As TextBox
    Private chkUseP2, chkOpenCatia, chkCustomModule As CheckBox
    Private cmbAxis, cmbModule As ComboBox
    Private txtPreview, txtLog As TextBox
    Private lblStatus As Label
    Private btnRun, btnOpenDir, btnBrowse As Button

    Public Sub New()
        Me.Text = "CATIA V5 R2016 直齿轮对生成器"
        Try : Me.Icon = Icon.ExtractAssociatedIcon(Application.ExecutablePath) : Catch : End Try
        Me.ClientSize = New Size(740, 732)
        Me.StartPosition = FormStartPosition.CenterScreen
        Me.FormBorderStyle = FormBorderStyle.FixedSingle
        Me.MaximizeBox = False
        Me.Font = New Font("Microsoft YaHei UI", 9.0F)

        Dim y As Integer = 14
        Dim lblMod As New Label() With {.Text = "标准模数 m (mm):", .Location = New Point(20, y + 3), .Size = New Size(145, 22)}
        cmbModule = New ComboBox() With {.DropDownStyle = ComboBoxStyle.DropDownList, .Location = New Point(168, y), .Size = New Size(185, 24)}
        cmbModule.Items.AddRange(New Object() {"0.5", "0.8", "1", "1.25", "1.5", "2", "2.5", "3", "4", "5", "6", "8", "10"})
        cmbModule.SelectedIndex = 2 ' "1"
        chkCustomModule = New CheckBox() With {.Text = "非标模数 (自填):", .Location = New Point(380, y + 2), .Size = New Size(145, 22), .Checked = False}
        txtM = New TextBox() With {.Text = "1", .Location = New Point(528, y), .Size = New Size(185, 24), .Enabled = False}
        Me.Controls.AddRange(New Control() {lblMod, cmbModule, chkCustomModule, txtM})
        y += 32

        txtZ1 = AddField("主动轮齿数 z1:", "34", 20, y)
        txtZ2 = AddField("从动轮齿数 z2:", "60", 380, y) : y += 32
        txtB1 = AddField("主动轮齿宽 b1 (mm):", "6", 20, y)
        txtB2 = AddField("从动轮齿宽 b2 (mm):", "6", 380, y) : y += 32
        txtX1 = AddField("主动轮变位 x1:", "0", 20, y)
        txtX2 = AddField("从动轮变位 x2:", "0", 380, y) : y += 32
        txtAlpha = AddField("法向压力角 α (°):", "20", 20, y)
        txtBacklash = AddField("总齿侧间隙 jn (mm):", "0", 380, y) : y += 32
        txtFilletR = AddField("倒角半径 R (mm):", "0.1", 20, y)

        Dim lblAxis As New Label() With {.Text = "齿轮轴向方向:", .Location = New Point(380, y + 3), .Size = New Size(145, 22)}
        cmbAxis = New ComboBox() With {.DropDownStyle = ComboBoxStyle.DropDownList, .Location = New Point(528, y), .Size = New Size(185, 24)}
        cmbAxis.Items.AddRange(New Object() {"Y", "Z", "X"})
        cmbAxis.SelectedIndex = 0
        Me.Controls.AddRange(New Control() {lblAxis, cmbAxis})
        y += 32

        chkOpenCatia = New CheckBox() With {.Text = "生成后在 CATIA 中打开并居中显示", .Location = New Point(20, y + 2), .Size = New Size(350, 22), .Checked = True}
        Me.Controls.Add(chkOpenCatia)
        y += 30

        Dim grpPos As New GroupBox() With {.Text = "齿轮空间坐标定位 (mm)", .Location = New Point(20, y), .Size = New Size(695, 92)}
        Dim lblP1 As New Label() With {.Text = "主动轮(Gear1)中心:", .Location = New Point(14, 26), .Size = New Size(140, 22)}
        txtCx1 = AddCoordBox(grpPos, "X:", "0", 160, 24)
        txtCy1 = AddCoordBox(grpPos, "Y:", "0", 335, 24)
        txtCz1 = AddCoordBox(grpPos, "Z:", "0", 510, 24)

        chkUseP2 = New CheckBox() With {.Text = "指定从动轮(Gear2)中心:", .Location = New Point(14, 57), .Size = New Size(165, 22), .Checked = False}
        txtCx2 = AddCoordBox(grpPos, "X:", "47", 185, 56)
        txtCy2 = AddCoordBox(grpPos, "Y:", "0", 355, 56)
        txtCz2 = AddCoordBox(grpPos, "Z:", "0", 525, 56)
        txtCx2.Enabled = False : txtCy2.Enabled = False : txtCz2.Enabled = False
        grpPos.Controls.AddRange(New Control() {lblP1, chkUseP2})
        Me.Controls.Add(grpPos)
        y += 102

        Dim lblOut As New Label() With {.Text = "输出目录:", .Location = New Point(20, y + 3), .Size = New Size(140, 22)}
        txtOutDir = New TextBox() With {.Text = "c:\齿轮生成\Output_CAA", .Location = New Point(165, y), .Size = New Size(450, 24)}
        btnBrowse = New Button() With {.Text = "浏览...", .Location = New Point(625, y - 1), .Size = New Size(90, 26)}
        AddHandler btnBrowse.Click, AddressOf OnBrowse
        Me.Controls.AddRange(New Control() {lblOut, txtOutDir, btnBrowse})
        y += 34

        txtPreview = New TextBox() With {
            .Multiline = True, .ReadOnly = True, .Location = New Point(20, y), .Size = New Size(695, 86),
            .BackColor = Color.FromArgb(245, 248, 250), .Font = New Font("Consolas", 9.0F)
        }
        Me.Controls.Add(txtPreview)
        y += 96

        btnRun = New Button() With {
            .Text = "生成双齿轮零件 (.CATPart)",
            .Location = New Point(20, y), .Size = New Size(500, 36),
            .BackColor = Color.FromArgb(0, 102, 204), .ForeColor = Color.White, .FlatStyle = FlatStyle.Flat
        }
        btnOpenDir = New Button() With {.Text = "打开输出目录", .Location = New Point(535, y), .Size = New Size(180, 36)}
        AddHandler btnRun.Click, AddressOf OnGenerate
        AddHandler btnOpenDir.Click, Sub(s, e)
                                         If Directory.Exists(txtOutDir.Text.Trim()) Then Process.Start("explorer.exe", txtOutDir.Text.Trim())
                                     End Sub
        Me.Controls.AddRange(New Control() {btnRun, btnOpenDir})
        y += 44

        lblStatus = New Label() With {.Text = "就绪（无需提前打开 CATIA 窗口）", .Location = New Point(20, y), .Size = New Size(695, 22), .ForeColor = Color.DimGray}
        Me.Controls.Add(lblStatus)
        y += 26

        txtLog = New TextBox() With {
            .Multiline = True, .ReadOnly = True, .ScrollBars = ScrollBars.Vertical,
            .Location = New Point(20, y), .Size = New Size(695, 145),
            .Font = New Font("Consolas", 8.5F), .BackColor = Color.White
        }
        Me.Controls.Add(txtLog)

        For Each tb In New TextBox() {txtM, txtAlpha, txtZ1, txtZ2, txtB1, txtB2, txtX1, txtX2, txtBacklash, txtFilletR, txtCx1, txtCy1, txtCz1, txtCx2, txtCy2, txtCz2}
            AddHandler tb.TextChanged, Sub(s, e) UpdatePreview()
        Next
        AddHandler chkUseP2.CheckedChanged, Sub(s, e)
                                                Dim en As Boolean = chkUseP2.Checked
                                                txtCx2.Enabled = en : txtCy2.Enabled = en : txtCz2.Enabled = en
                                                UpdatePreview()
                                            End Sub
        AddHandler cmbModule.SelectedIndexChanged, Sub(s, e)
                                                       If Not chkCustomModule.Checked AndAlso cmbModule.SelectedItem IsNot Nothing Then
                                                           txtM.Text = cmbModule.SelectedItem.ToString()
                                                       End If
                                                       UpdatePreview()
                                                   End Sub
        AddHandler chkCustomModule.CheckedChanged, Sub(s, e)
                                                       Dim custom As Boolean = chkCustomModule.Checked
                                                       txtM.Enabled = custom
                                                       cmbModule.Enabled = Not custom
                                                       If Not custom AndAlso cmbModule.SelectedItem IsNot Nothing Then
                                                           txtM.Text = cmbModule.SelectedItem.ToString()
                                                       ElseIf custom Then
                                                           txtM.Focus()
                                                       End If
                                                       UpdatePreview()
                                                   End Sub
        AddHandler cmbAxis.SelectedIndexChanged, Sub(s, e) UpdatePreview()
        UpdatePreview()
    End Sub

    Private Function AddField(labelText As String, defaultVal As String, x As Integer, y As Integer) As TextBox
        Dim lbl As New Label() With {.Text = labelText, .Location = New Point(x, y + 3), .Size = New Size(145, 22)}
        Dim tb As New TextBox() With {.Text = defaultVal, .Location = New Point(x + 148, y), .Size = New Size(185, 24)}
        Me.Controls.Add(lbl)
        Me.Controls.Add(tb)
        Return tb
    End Function

    Private Function AddCoordBox(parent As Control, caption As String, defaultVal As String, x As Integer, y As Integer) As TextBox
        Dim lbl As New Label() With {.Text = caption, .Location = New Point(x, y + 3), .Size = New Size(22, 20)}
        Dim tb As New TextBox() With {.Text = defaultVal, .Location = New Point(x + 24, y), .Size = New Size(125, 22)}
        parent.Controls.Add(lbl)
        parent.Controls.Add(tb)
        Return tb
    End Function

    Private Sub OnBrowse(sender As Object, e As EventArgs)
        Using dlg As New FolderBrowserDialog()
            dlg.SelectedPath = txtOutDir.Text.Trim()
            If dlg.ShowDialog() = DialogResult.OK Then txtOutDir.Text = dlg.SelectedPath
        End Using
    End Sub

    Private Function Inv(phi As Double) As Double
        Return Math.Tan(phi) - phi
    End Function

    Private Function SolveInv(target As Double) As Double
        Dim phi As Double = Math.Pow(3.0 * Math.Max(target, 1.0E-12), 1.0 / 3.0)
        For i As Integer = 1 To 40
            Dim f As Double = Math.Tan(phi) - phi - target
            Dim df As Double = Math.Tan(phi) * Math.Tan(phi)
            If Math.Abs(df) < 1.0E-12 Then Exit For
            Dim stepVal As Double = f / df
            phi -= stepVal
            If Math.Abs(stepVal) < 1.0E-13 Then Exit For
        Next
        Return phi
    End Function

    Private Function CleanNumStr(s As String) As String
        If s Is Nothing Then Return ""
        Dim t As String = s.Trim()
        If t.EndsWith("mm", StringComparison.OrdinalIgnoreCase) Then t = t.Substring(0, t.Length - 2).Trim()
        If t.EndsWith("deg", StringComparison.OrdinalIgnoreCase) Then t = t.Substring(0, t.Length - 3).Trim()
        If t.EndsWith("°") Then t = t.Substring(0, t.Length - 1).Trim()
        Return t
    End Function

    Private Function ParseDouble(tb As TextBox, ByRef val As Double) As Boolean
        Return Double.TryParse(CleanNumStr(tb.Text), NumberStyles.Float, CultureInfo.InvariantCulture, val)
    End Function

    Private Function GetModuleText() As String
        If chkCustomModule.Checked Then Return CleanNumStr(txtM.Text)
        If cmbModule.SelectedItem IsNot Nothing Then Return cmbModule.SelectedItem.ToString()
        Return "1"
    End Function

    Private Sub UpdatePreview()
        Dim m, aDeg, b1, b2, x1, x2, jn, filletR As Double
        Dim cx1, cy1, cz1, cx2, cy2, cz2 As Double
        Dim z1, z2 As Integer
        If Not (Double.TryParse(GetModuleText(), NumberStyles.Float, CultureInfo.InvariantCulture, m) AndAlso ParseDouble(txtAlpha, aDeg) AndAlso
                Integer.TryParse(txtZ1.Text.Trim(), z1) AndAlso Integer.TryParse(txtZ2.Text.Trim(), z2) AndAlso
                ParseDouble(txtB1, b1) AndAlso ParseDouble(txtB2, b2) AndAlso
                ParseDouble(txtX1, x1) AndAlso ParseDouble(txtX2, x2) AndAlso
                ParseDouble(txtBacklash, jn) AndAlso ParseDouble(txtFilletR, filletR) AndAlso
                ParseDouble(txtCx1, cx1) AndAlso ParseDouble(txtCy1, cy1) AndAlso ParseDouble(txtCz1, cz1) AndAlso
                (Not chkUseP2.Checked OrElse (ParseDouble(txtCx2, cx2) AndAlso ParseDouble(txtCy2, cy2) AndAlso ParseDouble(txtCz2, cz2)))) _
                OrElse m <= 0 OrElse z1 < 8 OrElse z2 < 8 OrElse b1 <= 0 OrElse b2 <= 0 OrElse aDeg <= 0 OrElse aDeg >= 45 OrElse filletR < 0 Then
            txtPreview.Text = "请输入有效齿轮参数和坐标（m > 0, z1 >= 8, z2 >= 8, b > 0, 0 < α < 45）。"
            Return
        End If
        Dim aRad As Double = aDeg * Math.PI / 180.0
        Dim stdA As Double = m * (z1 + z2) / 2.0
        Dim invAw As Double = Inv(aRad) + 2.0 * (x1 + x2) / (z1 + z2) * Math.Tan(aRad)
        If invAw < 0 Then
            txtPreview.Text = "总变位系数过负，无法求解啮合方程。"
            Return
        End If
        Dim awRad As Double = SolveInv(invAw)
        Dim workA As Double = stdA * Math.Cos(aRad) / Math.Cos(awRad)
        Dim dy As Double = (x1 + x2) - (workA - stdA) / m
        Dim d1 = m * z1, db1 = d1 * Math.Cos(aRad), da1 = d1 + 2 * (1 + x1 - dy) * m, df1 = d1 - 2 * (1.25 - x1) * m
        Dim d2 = m * z2, db2 = d2 * Math.Cos(aRad), da2 = d2 + 2 * (1 + x2 - dy) * m, df2 = d2 - 2 * (1.25 - x2) * m
        Dim pb = Math.PI * m * Math.Cos(aRad)
        Dim ra1 = da1 * 0.5, rb1 = db1 * 0.5, ra2 = da2 * 0.5, rb2 = db2 * 0.5
        Dim eps As Double = (Math.Sqrt(ra1 * ra1 - rb1 * rb1) + Math.Sqrt(ra2 * ra2 - rb2 * rb2) - workA * Math.Sin(awRad)) / pb

        Dim ax As String = If(cmbAxis.SelectedItem IsNot Nothing, cmbAxis.SelectedItem.ToString(), "Y")
        Dim wx As Double = 0, wy As Double = 1, wz As Double = 0
        Dim ux0 As Double = 1, uy0 As Double = 0, uz0 As Double = 0
        If ax = "X" Then
            wx = 1 : wy = 0 : wz = 0 : ux0 = 0 : uy0 = 1 : uz0 = 0
        ElseIf ax = "Z" Then
            wx = 0 : wy = 0 : wz = 1 : ux0 = 1 : uy0 = 0 : uz0 = 0
        End If

        Dim p2x, p2y, p2z As Double
        Dim p2Note As String
        If Not chkUseP2.Checked Then
            p2x = cx1 + workA * ux0 : p2y = cy1 + workA * uy0 : p2z = cz1 + workA * uz0
            p2Note = "自动按默认径向定距"
        Else
            Dim dx = cx2 - cx1, dyv = cy2 - cy1, dz = cz2 - cz1
            Dim dAx = dx * wx + dyv * wy + dz * wz
            Dim px = dx - dAx * wx, py = dyv - dAx * wy, pz = dz - dAx * wz
            Dim lPerp = Math.Sqrt(px * px + py * py + pz * pz)
            If lPerp < 1.0E-6 Then
                p2x = cx1 + workA * ux0 : p2y = cy1 + workA * uy0 : p2z = cz1 + workA * uz0
                p2Note = "连线与轴向平行，回退默认径向"
            ElseIf Math.Abs(lPerp - workA) <= 0.01 Then
                p2x = cx2 : p2y = cy2 : p2z = cz2
                p2Note = String.Format(CultureInfo.InvariantCulture, "符合中心距 (实测={0:F4})，直接采用", lPerp)
            Else
                Dim ux = px / lPerp, uy = py / lPerp, uz = pz / lPerp
                p2x = cx1 + workA * ux : p2y = cy1 + workA * uy : p2z = cz1 + workA * uz
                p2Note = String.Format(CultureInfo.InvariantCulture, "中心距不符 ({0:F3}≠{1:F3})，沿P1→P2方向定距", lPerp, workA)
            End If
        End If

        txtPreview.Text = String.Format(CultureInfo.InvariantCulture,
            "中心距 aw = {0:F4} mm | 啮合角 α' = {1:F4}° | 重合度 ε = {2:F3} | 轴向: {11}" & vbCrLf &
            "Gear1: d={3:F3}, db={4:F3}, df={5:F3}, da={6:F3} | 中心=({12:F2}, {13:F2}, {14:F2})" & vbCrLf &
            "Gear2: d={7:F3}, db={8:F3}, df={9:F3}, da={10:F3} | 中心=({15:F2}, {16:F2}, {17:F2})" & vbCrLf &
            "定位策略: {18}",
            workA, awRad * 180.0 / Math.PI, eps,
            d1, db1, df1, da1, d2, db2, df2, da2,
            ax, cx1, cy1, cz1, p2x, p2y, p2z, p2Note)
    End Sub

    Private Sub CloseIfOpenInCatia(partPath As String)
        Try
            Dim catia As Object = Marshal.GetActiveObject("CATIA.Application")
            If catia Is Nothing Then Exit Sub
            Dim fullTarget As String = Path.GetFullPath(partPath)
            Dim docs As Object = catia.Documents
            For i As Integer = CInt(docs.Count) To 1 Step -1
                Dim d As Object = docs.Item(i)
                Dim dFull As String = CStr(d.FullName)
                Dim dName As String = CStr(d.Name)
                If String.Equals(dFull, fullTarget, StringComparison.OrdinalIgnoreCase) OrElse
                   String.Equals(dName, Path.GetFileName(fullTarget), StringComparison.OrdinalIgnoreCase) Then
                    d.Close()
                End If
            Next
        Catch
        End Try
    End Sub

    Private Function TryOpenAndCenterInCatia(partPath As String) As String
        Try
            Dim catia As Object = Marshal.GetActiveObject("CATIA.Application")
            If catia Is Nothing Then Return "（未检测到运行中的 CATIA 窗口）"
            CloseIfOpenInCatia(partPath)
            Dim fullTarget As String = Path.GetFullPath(partPath)
            Dim openedDoc As Object = catia.Documents.Open(fullTarget)
            Try
                Dim win As Object = catia.ActiveWindow
                If win IsNot Nothing Then
                    Dim viewer As Object = win.ActiveViewer
                    If viewer IsNot Nothing Then
                        viewer.Update()
                        viewer.Reframe()
                        viewer.Update()
                    End If
                End If
            Catch
            End Try
            Try
                If openedDoc IsNot Nothing Then openedDoc.Save()
            Catch
            End Try
            Return "，并已在 CATIA 中居中打开显示。"
        Catch ex As COMException
            Return "（未检测到运行中的 CATIA 窗口，请直接用 CATIA 打开）"
        Catch ex As Exception
            Return "（CATIA 打开/居中提示：" & ex.Message & "）"
        End Try
    End Function

    Private Async Sub OnGenerate(sender As Object, e As EventArgs)
        Dim baseDir As String = AppDomain.CurrentDomain.BaseDirectory
        Dim wsDir As String = If(Directory.Exists(Path.Combine(baseDir, "CAA_GearPair\win_b64")), Path.Combine(baseDir, "CAA_GearPair"), baseDir)
        Dim workerExe As String = Path.Combine(wsDir, "win_b64\code\bin\GearPairWorker.exe")
        Dim seedPart As String = Path.Combine(wsDir, "win_b64\code\bin\GearSeed.CATPart")
        Dim catstart As String = "C:\Program Files\Dassault Systemes\B26\win_b64\code\bin\CATSTART.exe"

        If Not File.Exists(workerExe) OrElse Not File.Exists(seedPart) OrElse Not File.Exists(catstart) Then
            MessageBox.Show("缺少后台建模组件：" & vbCrLf & workerExe & vbCrLf & seedPart & vbCrLf & catstart, "路径检查失败", MessageBoxButtons.OK, MessageBoxIcon.Error)
            Return
        End If

        Dim outDir As String = txtOutDir.Text.Trim()
        Dim z1Val As Integer = 34, z2Val As Integer = 60
        Integer.TryParse(CleanNumStr(txtZ1.Text), z1Val)
        Integer.TryParse(CleanNumStr(txtZ2.Text), z2Val)
        Dim outPartPath As String = Path.Combine(outDir, String.Format(CultureInfo.InvariantCulture, "GearPair_z{0}_z{1}.CATPart", z1Val, z2Val))
        If chkOpenCatia.Checked Then CloseIfOpenInCatia(outPartPath)

        btnRun.Enabled = False
        lblStatus.ForeColor = Color.DarkBlue
        lblStatus.Text = "正在后台生成双齿轮零件..."
        txtLog.Clear()

        Dim ax As String = If(cmbAxis.SelectedItem IsNot Nothing, cmbAxis.SelectedItem.ToString(), "Y")
        Dim jobFile As String = Path.Combine(Path.GetTempPath(), "caa_gear_job_" & Guid.NewGuid().ToString("N") & ".txt")
        Dim jobLines As String() = {
            "seed=" & seedPart,
            "outDir=" & outDir,
            "m=" & GetModuleText(),
            "z1=" & CleanNumStr(txtZ1.Text),
            "z2=" & CleanNumStr(txtZ2.Text),
            "b1=" & CleanNumStr(txtB1.Text),
            "b2=" & CleanNumStr(txtB2.Text),
            "alpha=" & CleanNumStr(txtAlpha.Text),
            "x1=" & CleanNumStr(txtX1.Text),
            "x2=" & CleanNumStr(txtX2.Text),
            "backlash=" & CleanNumStr(txtBacklash.Text),
            "filletR=" & CleanNumStr(txtFilletR.Text),
            "axis=" & ax,
            "cx1=" & CleanNumStr(txtCx1.Text),
            "cy1=" & CleanNumStr(txtCy1.Text),
            "cz1=" & CleanNumStr(txtCz1.Text),
            "useP2=" & If(chkUseP2.Checked, "1", "0"),
            "cx2=" & CleanNumStr(txtCx2.Text),
            "cy2=" & CleanNumStr(txtCy2.Text),
            "cz2=" & CleanNumStr(txtCz2.Text)
        }
        File.WriteAllLines(jobFile, jobLines, New UTF8Encoding(False))

        Dim psi As New ProcessStartInfo() With {
            .FileName = catstart,
            .Arguments = "-env ""CATIA_P3.V5-6R2016.B26"" -direnv ""C:\ProgramData\DassaultSystemes\CATEnv"" -nowindow -run """ & workerExe & """",
            .UseShellExecute = False,
            .RedirectStandardOutput = True,
            .RedirectStandardError = True,
            .StandardOutputEncoding = Encoding.UTF8,
            .StandardErrorEncoding = Encoding.UTF8,
            .CreateNoWindow = True
        }
        psi.EnvironmentVariables("GEAR_JOB_FILE") = jobFile

        Dim outputText As String = Await Task.Run(Function() As String
            Try
                Using p As Process = Process.Start(psi)
                    Dim stdoutTask = p.StandardOutput.ReadToEndAsync()
                    Dim stderrTask = p.StandardError.ReadToEndAsync()
                    p.WaitForExit()
                    Return stdoutTask.Result & vbCrLf & stderrTask.Result
                End Using
            Finally
                Try : File.Delete(jobFile) : Catch : End Try
            End Try
        End Function)

        txtLog.Text = outputText
        If outputText.Contains("=== GearPairWorker finished: SUCCESS ===") Then
            Dim catNote As String = If(chkOpenCatia.Checked, TryOpenAndCenterInCatia(outPartPath), "")
            lblStatus.ForeColor = Color.DarkGreen
            lblStatus.Text = "生成成功！输出目录：" & outDir & catNote
        Else
            lblStatus.ForeColor = Color.Firebrick
            lblStatus.Text = "生成失败，请查看下方日志。"
        End If
        btnRun.Enabled = True
    End Sub

    <STAThread>
    Public Shared Sub Main()
        Application.EnableVisualStyles()
        Application.SetCompatibleTextRenderingDefault(False)
        Application.Run(New MainForm())
    End Sub
End Class
