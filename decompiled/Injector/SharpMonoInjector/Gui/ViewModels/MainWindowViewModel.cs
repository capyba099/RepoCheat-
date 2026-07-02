// Decompiled with csdecomp
// Type index: 49

using System;

namespace SharpMonoInjector.Gui.ViewModels
{
    public class MainWindowViewModel : SharpMonoInjector.Gui.ViewModels.ViewModel
    {
        public bool _isRefreshing;
        public bool _isExecuting;
        public object<SharpMonoInjector.Gui.Models.MonoProcess> _processes;
        public SharpMonoInjector.Gui.Models.MonoProcess _selectedProcess;
        public string _status;
        public bool _avalert;
        public string _avcolor;
        public string _assemblyPath;
        public string _injectNamespace;
        public string _injectClassName;
        public string _injectMethodName;
        public object<SharpMonoInjector.Gui.Models.InjectedAssembly> _injectedAssemblies;
        public SharpMonoInjector.Gui.Models.InjectedAssembly _selectedAssembly;
        public string _ejectNamespace;
        public string _ejectClassName;
        public string _ejectMethodName;
        public object b2351297List;
        private static <>c @<>9;
        private static object @<>9__0_0;
        private object<SharpMonoInjector.Gui.Models.MonoProcess> processes;
        private int @<>1__state;
        private object @<>t__builder;
        /* property */ _0;
        /* property */ _0;
        /* property */ _0;
        /* property */ _0;
        /* property */ _0;
        /* property */ @8E90;
        /* property */ @8E90;
        /* property */ ecuteRefreshCommand>b__0;
        /* property */ shCommand>b__0;
        /* property */ @8E90;
        /* property */ and>b__0;
        /* property */ @1CD80;
    public void .ctor()
    {
        // IL not decompiled (encrypted, unsupported, or auto-property stub)
    }

    public byte[] LoadEmbeddedDll(string p0)
    {
        // IL not decompiled (encrypted, unsupported, or auto-property stub)
    }

    public SharpMonoInjector.Gui.ViewModels.RelayCommand get_RefreshCommand()
    {
        // IL not decompiled (encrypted, unsupported, or auto-property stub)
    }

    public SharpMonoInjector.Gui.ViewModels.RelayCommand get_BrowseCommand()
    {
        // IL not decompiled (encrypted, unsupported, or auto-property stub)
    }

    public SharpMonoInjector.Gui.ViewModels.RelayCommand get_InjectCommand()
    {
        // unsupported: ldtoken
        loc1 = object.get_Module(object.GetTypeFromHandle());
        loc2 = object.op_Explicit(object.GetHINSTANCE(loc1));
        loc3 = (loc2 + 60);
    }

        // Failed to decompile method: IL stream ended unexpectedly

    public SharpMonoInjector.Gui.ViewModels.RelayCommand get_CopyStatusCommand()
    {
        // unsupported: ldelem.i1
    }

    public void ExecuteCopyStatusCommand(object p0)
    {
        if (/*missing*/ > /*missing*/) goto IL_-81;
    }

        // Failed to decompile method: IL stream ended unexpectedly

    public void ExecuteRefreshCommand(object p0)
    {
        Olesya.B6D6C8CC = 942;
        Olesya.E0C51076 = 117;
        Olesya.D58A25F0 = 1059;
        Olesya.ED01F736 = 1060;
        Olesya.F94C8DD4 = 1057;
        Olesya.DEEAA24F = 316;
        Olesya.E49EA153 = 315;
        Olesya.C6BC8A31 = 318;
        Olesya.A8240934 = 860;
        Olesya.F892F438 = -247;
        Olesya.A7E83699 = 2096905;
        Olesya.ED22BF4B = 308;
        Olesya.B95452EA = 319;
        Olesya.E611D7A8 = 317;
        Olesya.D17BB32B = 320;
        Olesya.F6212A71 = 1324;
        Olesya.F331385A = -246;
        return;
    }

        // Failed to decompile method: IL stream ended unexpectedly

        // Failed to decompile method: IL stream ended unexpectedly

    public void ExecuteInjectCommand(object p0)
    {
        loc1 = /*missing*/;
        // unsupported: cpobj
    }

    public bool CanExecuteEjectCommand(object p0)
    {
        // unsupported: conv.ovf.i4
        // unsupported: cpobj
    }

        // Failed to decompile method: IL stream ended unexpectedly

    public bool get_IsRefreshing()
    {
        // IL not decompiled (encrypted, unsupported, or auto-property stub)
    }

    public void set_IsRefreshing(bool p0)
    {
        // IL not decompiled (encrypted, unsupported, or auto-property stub)
    }

        // Failed to decompile method: IL stream ended unexpectedly

    public void set_IsExecuting(bool p0)
    {
        loc28 = arg1;
        if (loc28) goto IL_-127;
        loc23 = (loc23 + 1);
        loc29 = (loc23 < loc11);
        if (loc29) goto IL_-158;
        goto IL_1181;
    }

    public object<SharpMonoInjector.Gui.Models.MonoProcess> get_Processes()
    {
        // IL not decompiled (encrypted, unsupported, or auto-property stub)
    }

    public void set_Processes(object<SharpMonoInjector.Gui.Models.MonoProcess> p0)
    {
        // unsupported: constrained.
        if (/*missing*/ >= /*missing*/) goto IL_-2;
    }

        // Failed to decompile method: IL stream ended unexpectedly

    public void set_SelectedProcess(SharpMonoInjector.Gui.Models.MonoProcess p0)
    {
        // IL not decompiled (encrypted, unsupported, or auto-property stub)
    }

    public string get_Status()
    {
        // IL not decompiled (encrypted, unsupported, or auto-property stub)
    }

    public void set_Status(string p0)
    {
        // IL not decompiled (encrypted, unsupported, or auto-property stub)
    }

    public bool get_AVAlert()
    {
        // IL not decompiled (encrypted, unsupported, or auto-property stub)
    }

    public void set_AVAlert(bool p0)
    {
        // IL not decompiled (encrypted, unsupported, or auto-property stub)
    }

    public string get_AVColor()
    {
        return;
    }

    public void set_AVColor(string p0)
    {
        // IL not decompiled (encrypted, unsupported, or auto-property stub)
    }

    public string get_AssemblyPath()
    {
        // IL not decompiled (encrypted, unsupported, or auto-property stub)
    }

        // Failed to decompile method: IL stream ended unexpectedly

    public string get_InjectNamespace()
    {
        // unsupported: conv.i4
        // unsupported: conv.r8
        // unsupported: shr.un
    }

    public void set_InjectNamespace(string p0)
    {
        // IL not decompiled (encrypted, unsupported, or auto-property stub)
    }

    public string get_InjectClassName()
    {
        // IL not decompiled (encrypted, unsupported, or auto-property stub)
    }

    public void set_InjectClassName(string p0)
    {
        // unsupported: shr.un
        // unsupported: conv.r8
    }

    public string get_InjectMethodName()
    {
        if ((~/*missing*/)) goto IL_-10971286;
    }

    public void set_InjectMethodName(string p0)
    {
        // IL not decompiled (encrypted, unsupported, or auto-property stub)
    }

    public object<SharpMonoInjector.Gui.Models.InjectedAssembly> get_InjectedAssemblies()
    {
        // IL not decompiled (encrypted, unsupported, or auto-property stub)
    }

    public void set_InjectedAssemblies(object<SharpMonoInjector.Gui.Models.InjectedAssembly> p0)
    {
        loc0 = (/*missing*/ / this);
    }

    public SharpMonoInjector.Gui.Models.InjectedAssembly get_SelectedAssembly()
    {
        // unsupported: break
    }

    public void set_SelectedAssembly(SharpMonoInjector.Gui.Models.InjectedAssembly p0)
    {
        // IL not decompiled (encrypted, unsupported, or auto-property stub)
    }

    public string get_EjectNamespace()
    {
        return;
    }

    public void set_EjectNamespace(string p0)
    {
        // IL not decompiled (encrypted, unsupported, or auto-property stub)
    }

        // Failed to decompile method: IL stream ended unexpectedly

    public void set_EjectClassName(string p0)
    {
        // IL not decompiled (encrypted, unsupported, or auto-property stub)
    }

    public string get_EjectMethodName()
    {
        // IL not decompiled (encrypted, unsupported, or auto-property stub)
    }

    public void set_EjectMethodName(string p0)
    {
        if (loc58) goto IL_-65;
    }

    public static string GetProcessUser(object p0)
    {
        // IL not decompiled (encrypted, unsupported, or auto-property stub)
    }

    public static bool B4B125BB(nint p0, uint p1, unknown p2)
    {
        goto IL_1213952574;
    }

        // Failed to decompile method: IL stream ended unexpectedly

    public static bool AntivirusInstalled()
    {
        // unsupported: break
        // unsupported: break
    }

        // Failed to decompile method: IL stream ended unexpectedly

    public object F4207F85()
    {
        return;
    }

    public void F91F7642()
    {
        return;
    }

        // Failed to decompile method: IL stream ended unexpectedly

    public void EB7DCAAA()
    {
        // IL not decompiled (encrypted, unsupported, or auto-property stub)
    }

    public object E22504C4()
    {
        return;
    }

    public void AD9DE760()
    {
        loc36 = arg1;
        if (loc36) goto IL_-70;
        // unsupported: shr.un
        // unsupported: conv.u8
    }

    public object BAC45E8E()
    {
        // IL not decompiled (encrypted, unsupported, or auto-property stub)
    }

    public static void IO()
    {
        // unsupported: conv.u8
    }

        // Failed to decompile method: IL stream ended unexpectedly

    public static void Xml()
    {
        // IL not decompiled (encrypted, unsupported, or auto-property stub)
    }

    public static void D562ADA()
    {
        return;
        // unsupported: conv.r.un
    }

    public static void E6B403AB()
    {
        // unsupported: conv.i2
    }

    public static void D85A100D()
    {
        // unsupported: shl
    }

    public static void AB909132()
    {
        // IL not decompiled (encrypted, unsupported, or auto-property stub)
    }

    public static void F6198A71()
    {
        return;
    }

    public static void E4CE7355()
    {
        // unsupported: ldelema
        // unsupported: ldloca.s
    }

    public double E7994417(int p0, double p1)
    {
        loc4 = 0;
    }

    public byte BBD1BAB0()
    {
        // IL not decompiled (encrypted, unsupported, or auto-property stub)
    }

        // Failed to decompile method: IL stream ended unexpectedly

    public double D1D2698E(byte p0, int p1)
    {
        if (/*missing*/ == /*missing*/) goto IL_-804651002;
    }

    public double F8A7CC09()
    {
        // unsupported: conv.i2
    }

    public void set_F8A7CC09(double p0)
    {
        // IL not decompiled (encrypted, unsupported, or auto-property stub)
    }

    public long ED9D9461()
    {
        // IL not decompiled (encrypted, unsupported, or auto-property stub)
    }

    public void set_ED9D9461(long p0)
    {
        // IL not decompiled (encrypted, unsupported, or auto-property stub)
    }

    public long A7273A2E()
    {
        // IL not decompiled (encrypted, unsupported, or auto-property stub)
    }

    public void set_A7273A2E(long p0)
    {
        // IL not decompiled (encrypted, unsupported, or auto-property stub)
    }

        public abstract class @<>c
        {
            private SharpMonoInjector.Gui.ViewModels.MainWindowViewModel @<>4__this;
            public <>c__DisplayClass19_0 @<>8__1;
    public static void .cctor()
    {
        return;
    }

            // Failed to decompile method: IL stream ended unexpectedly

    protected void <.ctor>b__0_0()
    {
        if (/*missing*/ > /*missing*/) goto IL_6;
        IL_6:
    }

        }
        public abstract class @<>c__DisplayClass19_0
        {
            public object @<>u__1;
            // Failed to decompile method: IL stream ended unexpectedly

    protected void <ExecuteRefreshCommand>b__0()
    {
        if (/*missing*/ >= /*missing*/) goto IL_13;
        // unsupported: break
        loc1 = /*missing*/;
    }

        }
        public abstract class @<ExecuteRefreshCommand>d__19
        {
            public object CanExecuteChanged;
            public object<object> _execute;
            public object<object, bool> _canExecute;
    public sealed virtual void MoveNext()
    {
        loc4 = loc0;
        goto IL_-112;
        // unsupported: break
        // unsupported: starg.s
        // unsupported: ldobj
    }

            // Failed to decompile method: IL stream ended unexpectedly

        }
        public abstract class BC67102D
        {
    public void .ctor(object p0, nint p1)
    {
        // No IL body (abstract/extern/pinvoke)
    }

    public virtual bool Invoke(nint p0, uint p1, unknown p2)
    {
        // No IL body (abstract/extern/pinvoke)
    }

        }
    }
}
