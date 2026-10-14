using Atom;
using Atom.Managed;
using Game.Assets;
using Microsoft.CodeAnalysis;
using Microsoft.CodeAnalysis.Completion;
using Microsoft.CodeAnalysis.CSharp;
using Microsoft.CodeAnalysis.Host.Mef;
using Microsoft.CodeAnalysis.Text;
using System.Numerics;
using System.Reflection;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using System.Text;
using System.Text.Json;
using System.Text.RegularExpressions;
using System.Xml.Linq;
using Microsoft.CodeAnalysis.CSharp.Syntax;

static class Program
{
    private static readonly bool[,,] states=new bool[3,256,3];
    private static readonly float[] axes=new float[6];
    private static bool focused=true;
    private static byte[] response=[];
    private static string? played;
    private static int plays;
    private static void Check(bool value,string message){if(!value)throw new Exception(message);Console.WriteLine("PASS "+message);}
    private static void Reject(Action action,string message){try{action();}catch(ArgumentException){Console.WriteLine("PASS "+message);return;}throw new Exception(message);}
    [UnmanagedCallersOnly(CallConvs=[typeof(CallConvCdecl)])]
    private static ulong Epoch(nint context)=>1;
    [UnmanagedCallersOnly(CallConvs=[typeof(CallConvCdecl)])]
    private static int Valid(nint context,uint entity)=>entity==1?1:0;
    [UnmanagedCallersOnly(CallConvs=[typeof(CallConvCdecl)])]
    private static float Axis(int axis)=>focused?axes[axis]:0;
    [UnmanagedCallersOnly(CallConvs=[typeof(CallConvCdecl)])]
    private static unsafe int Audio(nint context,uint entity,int operation,byte* path,float volume){played=Marshal.PtrToStringUTF8((nint)path);plays++;return 1;}
    [UnmanagedCallersOnly(CallConvs=[typeof(CallConvCdecl)])]
    private static unsafe int Exchange(nint context,byte* request,byte* output,int capacity)
    {
        if(request!=null){using var doc=JsonDocument.Parse(Marshal.PtrToStringUTF8((nint)request)!);var r=doc.RootElement;
            bool value=focused && states[r.GetProperty("device").GetInt32(),r.GetProperty("code").GetInt32(),r.GetProperty("state").GetInt32()];
            response=Encoding.UTF8.GetBytes((value?"true":"false")+"\0");}
        if(output!=null && capacity>=response.Length)response.CopyTo(new Span<byte>(output,capacity));return response.Length;
    }
    private static unsafe void RuntimeApiChecks()
    {
        var unbound=new GameObject();Check(!unbound.IsValid,"unbound GameObject is invalid");bool rejected=false;try{unbound.Input.IsDown(KeyCode.Space);}catch(InvalidOperationException){rejected=true;}Check(rejected,"unbound typed input fails safely");
        var owner=new GameObject {Context=1,Entity=1,Generation=1,Api=new NativeApi {Epoch=&Epoch,Valid=&Valid,Axis=&Axis,Audio=&Audio,Exchange=&Exchange}};
        states[0,(int)KeyCode.W,0]=states[0,(int)KeyCode.W,1]=true;
        Check(owner.Input.IsDown(KeyCode.W) && owner.Input.WasPressed(KeyCode.W) && !owner.Input.WasReleased(KeyCode.W),"typed key held/pressed/released");
        states[0,(int)KeyCode.W,0]=states[0,(int)KeyCode.W,1]=false;states[0,(int)KeyCode.W,2]=true;
        Check(owner.Input.WasReleased(KeyCode.W) && !owner.Input.IsDown(KeyCode.W),"typed key release transition");
        states[1,(int)MouseButton.Right,1]=true;states[2,(int)GamepadButton.A,1]=true;states[2,(int)GamepadButton.B,2]=true;
        Check(owner.Input.WasPressed(MouseButton.Right) && owner.Input.WasPressed(GamepadButton.A) && owner.Input.WasReleased(GamepadButton.B),"mouse/gamepad named transitions");
        states[0,(int)KeyCode.W,0]=states[0,(int)KeyCode.D,0]=true;
        Check(MathF.Abs(owner.Input.Movement().Length()-1)<1e-6f,"movement preset diagonal normalization");
        states[0,(int)KeyCode.W,0]=states[0,(int)KeyCode.D,0]=false;states[0,(int)KeyCode.Left,0]=true;
        Check(owner.Input.Movement(InputPreset.ArrowKeys)==new Vector2(-1,0),"arrow-key preset mapping");
        Array.Clear(states);axes[(int)GamepadAxis.LeftX]=.5f;
        Check(owner.Input.Movement()==new Vector2(.5f,0) && owner.GamepadAxis(GamepadAxis.LeftX)==.5f,"named stick movement");
        focused=false;Check(!owner.Input.WasPressed(GamepadButton.A) && owner.Input.Movement()==Vector2.Zero,"focus gate for typed input");focused=true;
        Reject(()=>owner.Input.IsDown((KeyCode)999),"undefined key rejected");Reject(()=>owner.Input.Axis((GamepadAxis)999),"undefined axis rejected");
        Check(!owner.PlayAudio(AudioClip.None) && plays==0,"unassigned audio reference does not play");
        Check(owner.PlayAudio(AudioAssets.SeMp3,.5f) && plays==1 && played=="Asset/Sounds/se.mp3","generated audio resource routes to playback");
        Reject(()=>new AudioClip("bad.txt"),"unsupported audio extension rejected");
        Reject(()=>owner.PlayAudio(AudioAssets.SeMp3,float.NaN),"invalid volume rejected");
        using var doc=JsonDocument.Parse("{\"Clip\":\"Asset/Sounds/se.mp3\",\"Key\":\"Space\",\"Controls\":\"ArrowKeys\"}");
        var values=FieldCodec.Prepare(typeof(Settings),doc.RootElement);var settings=new Settings();foreach(var (field,value) in values)field.SetValue(settings,value);
        Check(settings.Clip==AudioAssets.SeMp3 && settings.Key==KeyCode.Space && settings.Controls==InputPreset.ArrowKeys,"typed audio and input fields deserialize");
        Check(FieldCodec.Kind(typeof(AudioClip))=="audioClip" && FieldCodec.Format(AudioClip.None)=="","audio field metadata and default reset encoding");
        var header=File.ReadAllText(Path.Combine(Directory.GetCurrentDirectory(),"Engine/Source/Runtime/Function/Input/KeyCode.h"));
        Check(Enum.GetValues<KeyCode>().All(key=>Regex.IsMatch(header,@"\b"+key+@"\s*=\s*0x"+((int)key).ToString("X2")+@"\b")),"all native/managed key codes match");
    }
    public sealed class Settings {public AudioClip Clip=AudioClip.None;public KeyCode Key=KeyCode.None;public InputPreset Controls=InputPreset.Wasd;}
    private static async Task Complete(string expression,string[] expected,string? documentation=null)
    {
        var assemblies=MefHostServices.DefaultAssemblies.Concat(new[]{Assembly.Load("Microsoft.CodeAnalysis.CSharp.Workspaces"),Assembly.Load("Microsoft.CodeAnalysis.Features"),Assembly.Load("Microsoft.CodeAnalysis.CSharp.Features")}).Distinct();
        using var workspace=new AdhocWorkspace(MefHostServices.Create(assemblies));
        var paths=((string)AppContext.GetData("TRUSTED_PLATFORM_ASSEMBLIES")!).Split(Path.PathSeparator)
            .Concat(new[]{typeof(GameObject).Assembly.Location,typeof(AudioAssets).Assembly.Location}).Distinct(StringComparer.OrdinalIgnoreCase);
        var references=paths.Select(p=>MetadataReference.CreateFromFile(p,documentation:File.Exists(Path.ChangeExtension(p,".xml"))?XmlDocumentationProvider.CreateFromFile(Path.ChangeExtension(p,".xml")):null));
        var project=workspace.AddProject("Completion",LanguageNames.CSharp).WithMetadataReferences(references).WithCompilationOptions(new CSharpCompilationOptions(OutputKind.DynamicallyLinkedLibrary));
        string source="using Atom;using Game.Assets;public class Sample:Behaviour {void Test(){var x="+expression+";}}";
        int position=source.IndexOf("$$",StringComparison.Ordinal);source=source.Replace("$$","");
        var document=project.AddDocument("Sample.cs",SourceText.From(source));var service=CompletionService.GetService(document)!;
        var list=await service.GetCompletionsAsync(document,position)??throw new Exception("No completions: "+expression);
        foreach(var name in expected)Check(list.ItemsList.Any(i=>i.DisplayText==name),"IDE completion "+expression+" -> "+name);
        if(documentation!=null){var item=list.ItemsList.First(i=>i.DisplayText==expected[0]);var description=await service.GetDescriptionAsync(document,item);Check(description?.Text.Contains(documentation,StringComparison.Ordinal)==true,"IDE XML tooltip "+expected[0]);}
    }
    private static async Task CatalogChecks()
    {
        var root=Directory.GetCurrentDirectory();var fixture=Path.Combine(root,"generated/script-api-fixtures");Directory.CreateDirectory(fixture);
        var audio=Path.Combine(fixture,"Sounds");Directory.CreateDirectory(audio);var source=Path.Combine(fixture,"AudioAssets.g.cs");
        // No decode is needed here; this validates resource discovery and identifier generation.
        foreach(var name in new[]{"a-b.mp3","a b.mp3","中文.wav","ignore.txt"})File.WriteAllBytes(Path.Combine(audio,name),[]);
#if DEBUG
        var configuration="Debug";
#else
        var configuration="Release";
#endif
        var generator=Path.Combine(root,"Engine/Managed/AssetCodegen/bin",configuration,"net10.0/AssetCodegen.dll");
        async Task Generate(){var start=new System.Diagnostics.ProcessStartInfo("dotnet"){UseShellExecute=false,CreateNoWindow=true,RedirectStandardOutput=true,RedirectStandardError=true};foreach(var a in new[]{generator,audio,source})start.ArgumentList.Add(a);using var process=System.Diagnostics.Process.Start(start)!;var stdout=process.StandardOutput.ReadToEndAsync();var stderr=process.StandardError.ReadToEndAsync();await process.WaitForExitAsync();if(process.ExitCode!=0)throw new Exception(await stderr);await stdout;}
        await Generate();var first=File.ReadAllText(source);var fields=Regex.Matches(first,@"public static AudioClip (\w+)").Select(m=>m.Groups[1].Value).ToArray();
        Check(fields.Length==3 && fields.Distinct().Count()==3 && !first.Contains("ignore.txt"),"audio catalog filters extensions and resolves identifier collisions");
        var compilation=CSharpCompilation.Create("Fixture",[CSharpSyntaxTree.ParseText(first)],
            ((string)AppContext.GetData("TRUSTED_PLATFORM_ASSEMBLIES")!).Split(Path.PathSeparator).Append(typeof(AudioClip).Assembly.Location).Distinct(StringComparer.OrdinalIgnoreCase).Select(p=>MetadataReference.CreateFromFile(p)),new CSharpCompilationOptions(OutputKind.DynamicallyLinkedLibrary));
        Check(!compilation.GetDiagnostics().Any(d=>d.Severity==DiagnosticSeverity.Error),"generated Unicode audio catalog compiles");
        var stamp=File.GetLastWriteTimeUtc(source);await Generate();Check(File.GetLastWriteTimeUtc(source)==stamp,"unchanged catalog avoids rewriting source");
        File.Delete(Path.Combine(audio,"a-b.mp3"));await Generate();Check(!File.ReadAllText(source).Contains("a-b.mp3"),"deleted audio removed from catalog");
    }
    private static async Task Main()
    {
        RuntimeApiChecks();
        DocumentationChecks();
        await CatalogChecks();
        await Complete("KeyCode.$$",["W","Space","Escape","F11"]);
        await Complete("Object.Input.$$",["IsDown","WasPressed","WasReleased","Movement","Axis"]);
        await Complete("Atom.$$",["Behaviour","GameObject","AudioClip","KeyCode","Mathf","ScriptInput"]);
        await Complete("AudioAssets.$$",["SeMp3"],"Asset/Sounds/se.mp3");
        await Complete("Time.$$",["DeltaTime","FrameCount","FixedDeltaTime","ElapsedTime","FixedFrameCount","FramesPerSecond"],"当前回调的时间间隔");
        await Complete("Object.Input.$$",["Movement"],"组合键盘方向");
        await Complete("SceneManager.$$",["LoadScene","Scenes","CurrentScene","LastLoadError","IsLoading"]);
        Console.WriteLine("All typed script API and Roslyn completion checks passed.");
    }
    private static void DocumentationChecks()
    {
        var root=Path.Combine(Directory.GetCurrentDirectory(),"Engine/Managed/Atom.Managed");
        var trees=Directory.GetFiles(root,"*.cs").Select(p=>CSharpSyntaxTree.ParseText(File.ReadAllText(p),new CSharpParseOptions(documentationMode:DocumentationMode.Diagnose),p)).ToArray();
        var refs=((string)AppContext.GetData("TRUSTED_PLATFORM_ASSEMBLIES")!).Split(Path.PathSeparator).Select(p=>MetadataReference.CreateFromFile(p));
        var compilation=CSharpCompilation.Create("Docs",trees,refs,new CSharpCompilationOptions(OutputKind.DynamicallyLinkedLibrary,allowUnsafe:true));
        int count=0;
        foreach(var tree in trees){var model=compilation.GetSemanticModel(tree);foreach(var node in tree.GetRoot().DescendantNodes()){
            if(node is not (MethodDeclarationSyntax or ConstructorDeclarationSyntax or PropertyDeclarationSyntax or EnumMemberDeclarationSyntax or VariableDeclaratorSyntax or BaseTypeDeclarationSyntax))continue;
            var symbol=model.GetDeclaredSymbol(node);if(symbol==null || symbol.DeclaredAccessibility!=Accessibility.Public || symbol.ContainingNamespace?.ToDisplayString()!="Atom")continue;
            if(symbol.ContainingType!=null && symbol.ContainingType.DeclaredAccessibility!=Accessibility.Public)continue;
            if(node is TypeDeclarationSyntax type && type.Modifiers.Any(SyntaxKind.PartialKeyword) && type.Identifier.Text=="GameObject" && Path.GetFileName(tree.FilePath)!="Behaviour.cs")continue;
            var xml=symbol.GetDocumentationCommentXml();Check(!string.IsNullOrEmpty(xml) && Regex.IsMatch(xml,@"[\u4e00-\u9fff]"),"Chinese XML documentation "+symbol.ToDisplayString());
            if(symbol is IMethodSymbol method){var doc=XElement.Parse(xml!);foreach(var parameter in method.Parameters)Check(doc.Elements("param").Any(p=>(string?)p.Attribute("name")==parameter.Name),"Chinese parameter documentation "+symbol.Name+"."+parameter.Name);}
            count++;
        }}
        Check(count>200,"all public script API declarations documented");
        var generated=XDocument.Load(Path.ChangeExtension(typeof(GameObject).Assembly.Location,".xml"));
        Check(generated.Descendants("member").Any(m=>(string?)m.Attribute("name")=="P:Atom.Time.DeltaTime" && m.Value.Contains("当前回调")),"compiled Time XML documentation");
    }
}
