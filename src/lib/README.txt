LIB FOLDER - VolkDMA only
=========================

This folder contains only the compiled VolkDMA libraries:

  VolkDMADebug.lib   - Debug build (built when you build solution in Debug)
  VolkDMARelease.lib - Release build (built when you build solution in Release)
  *.pdb              - Debug symbols from last build

When you compile VolkDMA, it combines everything it needs into these libs.
Your app links only VolkDMADebug.lib or VolkDMARelease.lib (and d3d11.lib).
No other .lib files belong here.

The import libs (leechcore.lib, vmm.lib) used during the VolkDMA build live
inside ext/VolkDMA/external/ and are merged into the VolkDMA output.
