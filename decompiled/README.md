# Decompiled output: injector.exe

This folder contains the best available static decompilation of `injector.exe`
(SharpMonoInjector GUI, protected with **Olesya** obfuscator), produced by
**csdecomp v0.1.21**.

## Download

| Format | Path |
|--------|------|
| Single `.cs` file | [`../injector.decompiled.cs`](../injector.decompiled.cs) |
| Visual Studio project | [`Injector.sln`](Injector.sln) + [`Injector/`](Injector/) |

Raw link (single file, after merge to `main`):

```
https://raw.githubusercontent.com/capyba099/RepoCheat-/main/injector.decompiled.cs
```

## Important limitation

**Most method bodies cannot be recovered from this binary.**

Olesya encrypts IL at build time and decrypts it only at runtime. Static
decompilation can restore:

- class / namespace structure
- field names (`_injectNamespace`, `_isRefreshing`, …)
- some partial method logic

But ~154 methods still show:

```csharp
// IL not decompiled (encrypted, unsupported, or auto-property stub)
```

This is **not a csdecomp bug** — the IL bytes in the PE file are encrypted.

## Need full readable source?

Use the original open-source project instead:

- https://github.com/warbler/SharpMonoInjector (MIT, archived)
- https://github.com/wh0am15533/SharpMonoInjector (community fixes)

Your `injector.exe` is a modified/obfuscated build; full source is not
recoverable by decompilation alone.

## Regenerate locally

```bash
cmake -S . -B build && cmake --build build
./build/csdecomp injector.exe -o injector.decompiled.cs
./build/csdecomp injector.exe --project decompiled --project-name Injector
```
