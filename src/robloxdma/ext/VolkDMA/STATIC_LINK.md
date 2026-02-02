# VolkDMA is statically linked

## What a static library is

- **Static library (`.lib` on Windows)**: A file of compiled object code. At **link time** the linker copies that code **into your executable**. The result is a single exe with no separate DLL for that library at runtime.
- **Import library (also `.lib`)**: Only describes exports of a **DLL**. The linker records a dependency on the DLL; at **runtime** the OS loads the DLL. So you still need the DLL next to the exe.

VolkDMA is built as a **static library** and linked into `robloxdma.exe`:

1. **VolkDMA.vcxproj** has `ConfigurationType=StaticLibrary` → it produces `VolkDMA.lib` (real static lib: VolkDMA’s `.obj` code).
2. **robloxdma** has a `ProjectReference` to VolkDMA with `LinkLibraryDependencies=true`, and links `VolkDMA.lib` in `AdditionalDependencies` → the linker pulls VolkDMA’s code into `robloxdma.exe`.
3. There is **no** VolkDMA.dll; VolkDMA’s code lives inside the exe.

So VolkDMA is **statically linked**: its code is embedded in the final executable. The exe also links `leechcore.lib` and `vmm.lib` (import libs) so VMMDLL_* / Lc* symbols resolve; at runtime the exe still loads `vmm.dll` and `leechcore.dll` for DMA.

**Note:** VolkDMA.lib is built by merging `leechcore.lib` and `vmm.lib` from the MemProcFS release. Those are **import libs** for `leechcore.dll` and `vmm.dll`, so at runtime the exe still loads those DLLs for DMA. To remove those DLL dependencies you would need static builds of MemProcFS/LeechCore from source (if available).
