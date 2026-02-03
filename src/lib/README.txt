LIB FOLDER - VolkDMA only
=========================

This folder contains only the compiled VolkDMA libraries:

  VolkDMADebug.lib   - Debug build (build solution in Debug | x64)
  VolkDMARelease.lib - Release build (build solution in Release | x64)
  *.pdb              - Debug symbols

No leechcore.lib, vmm.lib, or other DMA libs here. When you compile VolkDMA,
it merges everything from ext/VolkDMA/external/ into these two libs. Your app
links only VolkDMADebug.lib or VolkDMARelease.lib (and d3d11.lib).
