LIB FOLDER - What goes here and why
====================================

VOLKDMA OUTPUT (built when you compile):
----------------------------------------
  VolkDMADebug.lib   - Debug build of VolkDMA (used when you build robloxdma in Debug)
  VolkDMARelease.lib - Release build of VolkDMA (used when you build robloxdma in Release)
  VolkDMA.pdb        - Debug symbols (from last build, Debug or Release)

These are produced by the VolkDMA project. Debug and Release configs output
different .lib names so both can live in this folder and the right one is
linked automatically when you build the main app.

REQUIRED FOR BUILD (must be present to build VolkDMA from source):
-----------------------------------------------------------------
  leechcore.lib - Import library for leechcore.dll (from VolkDMA repo external/)
  vmm.lib       - Import library for vmm.dll (from VolkDMA repo external/)

WHY these two are needed:
  VolkDMA's code calls functions inside vmm.dll and leechcore.dll. To build
  VolkDMA.lib, the linker must resolve those symbols. leechcore.lib and vmm.lib
  are the "import libraries" that describe those DLL exports. Without them,
  building VolkDMA would fail with "unresolved external symbol" errors.

  So: they stay in this folder because the VolkDMA project needs them at
  build time. Your exe only links VolkDMADebug.lib or VolkDMARelease.lib;
  it does not link leechcore.lib or vmm.lib directly.

THE .DLL FILES (vmm.dll, leechcore.dll) - no Debug/Release
---------------------------------------------------------
  The DMA DLLs (vmm.dll, leechcore.dll, FTD3XX.dll) do NOT have separate
  Debug and Release builds from VolkDMA. You use the same DLLs for both
  Debug and Release runs. Put them next to your .exe (e.g. in final/build/
  or final/dlls/ and copy to output). Get them from:
    https://github.com/lyk64/VolkDMA (dlls folder)
  or your DMA device/driver package.

Summary:
  - .lib folder: VolkDMADebug.lib, VolkDMARelease.lib, leechcore.lib, vmm.lib
  - .dll: same vmm.dll/leechcore.dll for both Debug and Release; place next to exe
