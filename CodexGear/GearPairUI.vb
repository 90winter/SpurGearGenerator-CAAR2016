
Option Strict Off
Imports System
Imports System.IO
Imports System.Globalization
Imports System.Windows.Forms
Imports System.Drawing
Imports System.Text.RegularExpressions
Imports System.Runtime.InteropServices
Imports System.Reflection

Module GearApp
    Private app As Object
    Private seed As String
    Private opened As New Collections.Generic.List(Of Object)

    <STAThread>
    Public Sub Main(args As String())
        If args.Length = 1 AndAlso args(0).StartsWith("--test", StringComparison.Ordinal) Then
            File.WriteAllText(IO.Path.Combine(AppDomain.CurrentDomain.BaseDirectory,"GearTest.txt"),"Running")
            Try
                Dim path = Generate(New Double(){10,20,30}, "+Y", If(args(0) = "--test-flat", 0.0, 0.1), If(args(0) = "--test-alt", 24, 34), If(args(0) = "--test-alt", 37, 60), If(args(0) = "--test-alt", 1.25, 1.0))
                File.WriteAllText(IO.Path.Combine(AppDomain.CurrentDomain.BaseDirectory,"GearTest.txt"),path)
            Catch ex As Exception
                File.WriteAllText(IO.Path.Combine(AppDomain.CurrentDomain.BaseDirectory,"GearTest.txt"),ex.ToString())
                Environment.ExitCode = 1
            End Try
            Return
        End If
        Application.EnableVisualStyles()
        Application.Run(New GearForm())
    End Sub

    Public Function Generate(center As Double(), direction As String, radius As Double, firstTeeth As Integer, secondTeeth As Integer, moduleMm As Double) As String
        If radius > 0.2 * moduleMm Then Throw New ArgumentException("R 角不得超过模数的 0.2 倍。")
        app = Marshal.GetActiveObject("CATIA.Application")
        Dim folder = AppDomain.CurrentDomain.BaseDirectory
        seed = IO.Path.Combine(folder,"EquationSeed.CATPart")
        Dim id = Guid.NewGuid().ToString("N").Substring(0,8)
        Dim output = Directory.CreateDirectory(IO.Path.Combine(folder,"Output",id))
        Dim file = IO.Path.Combine(output.FullName,"GearPair_" & id & ".CATPart")
        Dim temp = IO.Path.Combine(Path.GetTempPath(),"Driven_" & id & ".CATPart")
        Dim alerts As Boolean = app.DisplayFileAlerts
        Dim success As Boolean = False
        app.DisplayFileAlerts = False
        Try
            Dim doc As Object = Wheel(file,firstTeeth,0,0,radius,moduleMm)
            Dim distance As Double = moduleMm * (firstTeeth + secondTeeth) / 2
            Dim phase As Double = If(secondTeeth Mod 2 = 0, 180.0 / secondTeeth, 0.0)
            Dim other As Object = Wheel(temp,secondTeeth,distance,phase,radius,moduleMm)
            other.Selection.Clear()
            other.Selection.Add((other.Part.MainBody))
            other.Selection.Copy()
            doc.Activate()
            doc.Selection.Clear()
            doc.Selection.Add((doc.Part))
            doc.Selection.PasteSpecial("CATPrtResultWithOutLink")
            doc.Selection.Clear()
            Dim part As Object = doc.Part
            If part.Bodies.Count <> 2 Then Throw New InvalidOperationException("预期两个独立实体。")
            Put(part.Bodies.Item(1),"Name","Driver_" & firstTeeth)
            Put(part.Bodies.Item(2),"Name","Driven_" & secondTeeth)
            PlacePair(part,center,direction)
            part.Update()
            doc.Save()
            doc.Close()
            opened.Remove(doc)
            Dim check As Object = app.Documents.Open(file)
            If check.Part.Bodies.Count <> 2 Then Throw New InvalidOperationException("重开后实体数量错误。")
            check.Part.Update()
            success = True
            Return file
        Finally
            Try
                For Each pending In opened
                    pending.Close()
                Next
                opened.Clear()
                If IO.File.Exists(temp) Then IO.File.Delete(temp)
                If Not success AndAlso IO.File.Exists(file) Then IO.File.Delete(file)
            Finally
                app.DisplayFileAlerts = alerts
            End Try
        End Try
    End Function

    Private Sub Put(target As Object, name As String, value As Object)
        target.GetType().InvokeMember(name,BindingFlags.PutDispProperty,Nothing,target,New Object(){value})
    End Sub

    Private Sub PlacePair(part As Object, center As Double(), direction As String)
        Dim x As Object() = {1.0,0.0,0.0}
        Dim y As Object() = {0.0,1.0,0.0}
        Select Case direction
            Case "+X" : x = New Object(){0.0,1.0,0.0} : y = New Object(){0.0,0.0,1.0}
            Case "-X" : x = New Object(){0.0,1.0,0.0} : y = New Object(){0.0,0.0,-1.0}
            Case "+Y" : y = New Object(){0.0,0.0,-1.0}
            Case "-Y" : y = New Object(){0.0,0.0,1.0}
            Case "-Z" : y = New Object(){0.0,-1.0,0.0}
        End Select
        Dim source As Object = part.AxisSystems.Add()
        source.Name = "GearLocal"
        Dim target As Object = part.AxisSystems.Add()
        target.Name = "GearPlacement"
        target.PutOrigin(New Object(){center(0),center(1),center(2)})
        For i As Integer = 0 To 2
            y(i) = -CDbl(y(i))
        Next
        target.PutVectors(x,y)
        part.Update()
        For i As Integer = 1 To 2
            Dim body As Object = part.Bodies.Item(i)
            Put(part,"InWorkObject",body.Shapes.Item(body.Shapes.Count))
            Dim featureCount As Integer = CInt(body.Shapes.Count)
            part.ShapeFactory.AddNewAxisToAxis2(part.CreateReferenceFromObject(source),part.CreateReferenceFromObject(target))
            If CInt(body.Shapes.Count) <> featureCount + 1 Then Throw New InvalidOperationException("未创建定位变换，实体 " & i)
            Dim transform As Object = body.Shapes.Item(featureCount + 1)
            part.UpdateObject(transform)
        Next
    End Sub

    Private Sub RoundToothCorners(doc As Object, part As Object, factory As Object, size As Double, centerX As Double, teeth As Integer, moduleMm As Double)
        doc.Activate()
        Dim selection As Object = doc.Selection
        selection.Clear()
        selection.Add((part.MainBody))
        selection.Search("Topology.CGMEdge,sel")
        Dim ends As New Collections.Generic.List(Of Object)
        Dim spa As Object = doc.GetWorkbench("SPAWorkbench")
        For i As Integer = 1 To CInt(selection.Count2)
            Dim r As Object = selection.Item2(i).Reference
            Dim measure As Object = spa.GetMeasurable(r)
            Dim points(8) As Object
            measure.GetPointsOnCurve(points)
            Dim x As Double = CDbl(points(0))
            Dim y As Double = CDbl(points(1))
            If Math.Abs(CDbl(points(6))-x) < 0.00001 AndAlso Math.Abs(CDbl(points(7))-y) < 0.00001 AndAlso
                Math.Abs(Math.Abs(CDbl(points(8))-CDbl(points(2)))-6) < 0.00001 Then
                Dim radial As Double = Math.Sqrt((x-centerX)^2 + y^2)
                Dim tip As Double = moduleMm * (teeth / 2 + 1)
                Dim root As Double = moduleMm * (teeth / 2 - 1.25)
                If Math.Abs(radial-tip) < 0.0001 OrElse Math.Abs(radial-root) < 0.0001 Then ends.Add(r)
            End If
        Next
        If ends.Count <> teeth * 4 Then Throw New InvalidOperationException(teeth & " 齿轮识别到 " & ends.Count & " 条齿顶或齿根棱，预期 " & teeth * 4 & " 条。")
        Dim fillet As Object = factory.AddNewEdgeFilletWithConstantRadius((ends(0)),0,size)
        For i As Integer = 1 To ends.Count-1
            fillet.AddObjectToFillet((ends(i)))
        Next
        part.UpdateObject(fillet)
    End Sub
Private Function Wheel(path As String,n As Integer,centerX As Double,phase As Double,radius As Double,moduleMm As Double) As Object
 Dim d,p,g,h,r,s,j,i,f,x,y,z,c,t,e,m,l,u,law,parts,phi,xy,zx,rr,rf,rt,rb,sf,pad,pat,ax,plane,center,rotation
 File.Copy(seed,path)
 d=app.Documents.Open(path)
 opened.Add(d)
 p=d.Part
 Put(p.MainBody,"Name","Gear_" & n)
 center=Length(p,"GearCenterX",centerX)
 rotation=p.Parameters.CreateDimension("GearPhase","ANGLE",0)
 rotation.ValuateFromString(CStr(phase) & "deg")
 g=p.HybridBodies.Item("LogSpiral_Equation")
 h=p.HybridShapeFactory
 r=p.Relations
 s=d.Selection
 f=g.HybridShapes.Item("LogSpiral_Standard_Equation")
 For i=1 To r.Count
   If InStr(r.Item(i).Name,"StandardEquation")=0 Then s.Add(r.Item(i))
 Next
 If s.Count>0 Then s.Delete() s.Clear()
 c=p.Parameters.CreateInteger("ToothCount",n)
 m=Length(p,"GearModule",moduleMm)
 rr=Length(p,"FaceWidth",6)
 rf=Length(p,"RootRadius",1)
 rt=Length(p,"TipRadius",1)
 rb=Length(p,"BaseRadius",1)
 Formula(r,"RootSize",rf,"GearModule*(ToothCount/2-1.25)")
 Formula(r,"TipSize",rt,"GearModule*(ToothCount/2+1)")
 Formula(r,"BaseSize",rb,"GearModule*ToothCount/2*cos(20deg)")
 For Each item As Object In New Object(){"X","Y","Z"}
   law=r.Item("LogSpiral_" & item & "_StandardEquation")
   t=Regex.Match(CStr(law.Value),"FormalReal\.[0-9]+").Value
   parts=Split(law.Value,"=",2)
   u="(sqrt((TipRadius/BaseRadius)*(TipRadius/BaseRadius)-1)*" & t & ")"
   phi="(PI/(2*ToothCount)+tan(20deg)-20deg/1rad+GearPhase/1rad-" & u & ")"
   Dim body As String
 Select Case item
     Case "X": body="GearCenterX+BaseRadius*(cos(" & phi & "*1rad)-" & u & "*sin(" & phi & "*1rad))"
     Case "Y": body="BaseRadius*(sin(" & phi & "*1rad)+" & u & "*cos(" & phi & "*1rad))"
     Case "Z": body="0mm*" & t
     Case Else: Throw New InvalidOperationException("Invalid equation axis")
   End Select
   law.Modify(Trim(parts(0)) & " = " & body)
 Next
 p.UpdateObject(f)
 c=Add(g,h.AddNewPointCoord(centerX,0,0))
 c=p.CreateReferenceFromObject(c)
 xy=p.CreateReferenceFromObject((p.OriginElements.PlaneXY))
 zx=p.CreateReferenceFromObject((p.OriginElements.PlaneZX))
 ax=Add(g,h.AddNewLinePtDir(c,h.AddNewDirectionByCoord(0,0,1),0,10,False))
 If phase <> 0 Then
   plane=Add(g,h.AddNewPlaneAngle(zx,p.CreateReferenceFromObject(ax),phase,False))
   zx=p.CreateReferenceFromObject(plane)
 End If
 p.Update()
 f=p.CreateReferenceFromObject(f)
 l=Add(g,h.AddNewSymmetry(f,zx))
 x=Add(g,h.AddNewPointOnCurveFromPercent(f,0,False))
 y=Add(g,h.AddNewPointOnCurveFromPercent(f,1,False))
 z=Add(g,h.AddNewSymmetry(p.CreateReferenceFromObject(y),zx))
 e=Add(g,h.AddNewSymmetry(p.CreateReferenceFromObject(x),zx))
 m=Add(g,h.AddNewPointCoord(1,0,0))
 Formula(r,"TipMidpointX",m.X,"GearCenterX+TipRadius*cos(GearPhase)")
 Formula(r,"TipMidpointY",m.Y,"TipRadius*sin(GearPhase)") p.Update()
 t=Add(g,h.AddNewCircle3Points(p.CreateReferenceFromObject(y),p.CreateReferenceFromObject(m),p.CreateReferenceFromObject(z)))
 t.SetLimitation(2)
 x=Add(g,h.AddNewLinePtPt(c,p.CreateReferenceFromObject(x)))
 e=Add(g,h.AddNewLinePtPt(p.CreateReferenceFromObject(e),c))
 j=Add(g,h.AddNewJoin(p.CreateReferenceFromObject(x),f))
 For Each item As Object In New Object(){t,l,e}
   j.AddElement(p.CreateReferenceFromObject(item))
 Next
 p.UpdateObject(j)
 x=Add(g,h.AddNewCircleCtrRad(c,xy,False,1))
 x.SetLimitation(1)
 Formula(r,"RootCircleSize",x.Radius,"RootRadius") p.Update()
 Put(p,"InWorkObject",p.MainBody)
 sf=p.ShapeFactory
 pad=sf.AddNewPadFromRef(p.CreateReferenceFromObject(x),6)
 Formula(r,"RootWidth",pad.FirstLimit.Dimension,"FaceWidth") p.Update()
 pad=sf.AddNewPadFromRef(p.CreateReferenceFromObject(j),6)
 Formula(r,"ToothWidth",pad.FirstLimit.Dimension,"FaceWidth") p.Update()
 pat=sf.AddNewCircPattern(pad,1,n,1,360/n,1,1,c,p.CreateReferenceFromObject(ax),True,0,True)
 Formula(r,"PatternCount",pat.AngularRepartition.InstancesCount,"ToothCount")
 Formula(r,"PatternSpacing",pat.AngularRepartition.AngularSpacing,"360deg/ToothCount") p.Update()
 Put(p,"InWorkObject",pat)
 If radius > 0 Then RoundToothCorners(d,p,sf,radius,centerX,n,moduleMm)
 s.Add(g)
 s.VisProperties.SetShow(1) s.Clear()
 Return d
End Function
Function Add(g As Object,f As Object) As Object
 g.AppendHybridShape(f)
 Return f
End Function
Function Length(p As Object,n As String,v As Double) As Object
 Dim parameter As Object =p.Parameters.CreateDimension(n,"LENGTH",0)
 parameter.ValuateFromString(CStr(v) & "mm")
 Return parameter
End Function
Sub Formula(r As Object,n As String,p As Object,expr As String)
 r.CreateFormula(n,"",p,expr)
End Sub






End Module

Public Class GearForm
    Inherits Form
    Private values As New Collections.Generic.List(Of NumericUpDown)
    Private axis As New ComboBox()
    Private status As New Label()
    Private build As New Button()

    Public Sub New()
        Text = "CATIA 直齿轮对生成器"
        StartPosition = FormStartPosition.CenterScreen
        ClientSize = New Size(460, 430)
        Font = New Font("Microsoft YaHei UI",10)
        Dim layout As New TableLayoutPanel With {.Dock=DockStyle.Fill,.Padding=New Padding(16),.ColumnCount=2,.AutoScroll=True}
        layout.ColumnStyles.Add(New ColumnStyle(SizeType.Percent,55))
        layout.ColumnStyles.Add(New ColumnStyle(SizeType.Percent,45))
        Controls.Add(layout)
        Dim heading As New Label With {.Text="直齿轮对 · 单 CATPart",.AutoSize=True,.Font=New Font(Font,FontStyle.Bold)}
        layout.Controls.Add(heading)
        layout.SetColumnSpan(heading,2)
        For Each label In New String(){"主动轮端面中心 X (mm)","主动轮端面中心 Y (mm)","主动轮端面中心 Z (mm)","齿顶 / 齿根 R (mm)","主动轮齿数","从动轮齿数","模数 (mm)"}
            layout.Controls.Add(New Label With {.Text=label,.AutoSize=True,.Anchor=AnchorStyles.Left})
            Dim box As New NumericUpDown With {.DecimalPlaces=3,.Minimum=-100000,.Maximum=100000,.Dock=DockStyle.Fill,.Increment=0.1D}
            values.Add(box)
            layout.Controls.Add(box)
        Next
        values(3).Minimum=0
        values(3).Maximum=10D
        values(3).Value=0.1D
        For i As Integer=4 To 5
            values(i).Minimum=18
            values(i).Maximum=120
            values(i).DecimalPlaces=0
            values(i).Increment=1
        Next
        values(4).Value=34
        values(5).Value=60
        values(6).Minimum=0.1D
        values(6).Maximum=20D
        values(6).Value=1
        layout.Controls.Add(New Label With {.Text="齿轮轴向",.AutoSize=True})
        axis.DropDownStyle=ComboBoxStyle.DropDownList
        axis.Items.AddRange(New Object(){"+X","-X","+Y","-Y","+Z","-Z"})
        axis.SelectedIndex=2
        axis.Dock=DockStyle.Fill
        layout.Controls.Add(axis)
        build.Text="生成 CATPart"
        build.AutoSize=True
        layout.Controls.Add(build)
        layout.SetColumnSpan(build,2)
        status.Text="请先启动 CATIA V5。"
        status.AutoSize=True
        status.MaximumSize=New Size(420,0)
        layout.Controls.Add(status)
        layout.SetColumnSpan(status,2)
        AddHandler build.Click,AddressOf RunBuild
    End Sub

    Private Sub RunBuild(sender As Object,e As EventArgs)
        build.Enabled=False
        status.Text="正在生成齿轮对，圆角可能需要一两分钟…"
        Dim center As Double()={CDbl(values(0).Value),CDbl(values(1).Value),CDbl(values(2).Value)}
        Dim direction As String=CStr(axis.SelectedItem)
        Dim radius As Double=CDbl(values(3).Value)
        Dim firstTeeth As Integer=CInt(values(4).Value)
        Dim secondTeeth As Integer=CInt(values(5).Value)
        Dim moduleMm As Double=CDbl(values(6).Value)
        Dim worker As New Threading.Thread(
            Sub()
                Dim message As String
                Try
                    message=GearApp.Generate(center,direction,radius,firstTeeth,secondTeeth,moduleMm)
                Catch ex As Exception
                    message="生成失败 " & ex.Message
                End Try
                BeginInvoke(New Action(
                    Sub()
                        status.Text=message
                        build.Enabled=True
                    End Sub))
            End Sub)
        worker.SetApartmentState(Threading.ApartmentState.STA)
        worker.Start()
    End Sub

    Protected Overrides Sub OnFormClosing(e As FormClosingEventArgs)
        If Not build.Enabled Then
            e.Cancel=True
            Return
        End If
        MyBase.OnFormClosing(e)
    End Sub
End Class


