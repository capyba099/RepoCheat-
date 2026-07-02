// Decompiled with csdecomp
// Type index: 60

using System;

namespace SharpMonoInjector.Gui.Converters
{
    public class MonoProcessToStringConverter
    {
    public sealed virtual object Convert(object p0, object p1, object p2, object p3)
    {
        // IL not decompiled (encrypted, unsupported, or auto-property stub)
    }

    public sealed virtual object ConvertBack(object p0, object p1, object p2, object p3)
    {
        if (!(/*missing*/)) goto IL_27;
    }

    public void .ctor()
    {
        // unsupported: ldelem.i
    }

    public void FECB11FA()
    {
        loc0 = /*missing*/;
        loc1 = GetProcAddress();
        if (!(loc1)) goto IL_24;
    }

    public object DA33E91F()
    {
        // IL not decompiled (encrypted, unsupported, or auto-property stub)
    }

    public void F52DD642()
    {
        loc38 = (this == 0);
        if (loc38) goto IL_-51;
        result = loc5;
        goto IL_24;
        goto IL_-969;
        IL_24:
        return result;
        if (/*missing*/ > 5) goto IL_34;
    }

    public object BC6251A()
    {
        // unsupported: conv.r8
        loc4 = object.ToCharArray(this);
        loc8 = 0;
        goto IL_36;
    }

    public void EDA31ADF()
    {
        // unsupported: break
        loc28 = (/*missing*/ == 0);
        if (!(loc28)) goto IL_42;
        // unsupported: shr.un
        loc24 = (7 & 131071);
        // unsupported: shr.un
        loc20 = ((2 & 31) + 2);
        loc2 = (loc2 + 3);
        goto IL_69;
        IL_42:
        // unsupported: shr.un
        loc24 = 15;
        // unsupported: shr.un
        loc20 = ((7 & 255) + 3);
    }

    public object E979993D()
    {
        // unsupported: conv.ovf.i
    }

    public static void ComponentModel()
    {
        // IL not decompiled (encrypted, unsupported, or auto-property stub)
    }

    public static void IO()
    {
        // unsupported: conv.u2
    }

        // Failed to decompile method: IL stream ended unexpectedly

    public static void Tasks()
    {
        // unsupported: calli
    }

    public static void A4233DEF()
    {
        // IL not decompiled (encrypted, unsupported, or auto-property stub)
    }

    public static void A4885136()
    {
        // IL not decompiled (encrypted, unsupported, or auto-property stub)
    }

        // Failed to decompile method: IL stream ended unexpectedly

    public static void FE5DB167()
    {
        // IL not decompiled (encrypted, unsupported, or auto-property stub)
    }

    public static void A673123()
    {
        return arg2;
        return Olesya.F6212A71;
        return Olesya.F331385A;
        return 0;
        loc48 = -1;
        // unsupported: break
    }

    public static void C3578E5D()
    {
        // IL not decompiled (encrypted, unsupported, or auto-property stub)
    }

    public long B4A54AA3(int p0, string p1)
    {
        // unsupported: ldtoken
        loc0 = object.get_Module(object.GetTypeFromHandle());
        loc1 = object.get_FullyQualifiedName(loc0);
        if (object.get_Length(loc1) <= 0) goto IL_59;
        goto IL_60;
        IL_59:
        IL_60:
        loc2 = 0;
        loc3 = object.op_Explicit(object.GetHINSTANCE(loc0));
    }

        // Failed to decompile method: IL stream ended unexpectedly

    public float C6EC218D()
    {
        // unsupported: break
        loc0 = /*missing*/;
        if (!(loc0)) goto IL_18;
        Olesya.C114011C = new parent?1.ctor();
        IL_18:
        loc1 = Olesya.C114011C;
        loc2 = 0;
        // unsupported: ldloca.s
        // unsupported: ldloca.s
        loc6 = parent?1.TryGetValue(object.Enter(loc1), Olesya.C114011C, this);
        if (!(loc6)) goto IL_61;
        loc7 = loc3;
    }

    public void set_C6EC218D(float p0)
    {
        // unsupported: stelem.i
    }

        // Failed to decompile method: IL stream ended unexpectedly

    public void set_B6C59AFC(bool p0)
    {
        // unsupported: ldarga.s
        if (!(/*missing*/)) goto IL_-74;
        // unsupported: ldloca.s
    }

    }
}
