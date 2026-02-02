Import libraries for the build.

- VolkDMA.lib, VolkDMA.pdb — Built by the VolkDMA project (output goes here). VolkDMA merges leechcore.lib and vmm.lib into VolkDMA.lib, so the exe links only VolkDMA.lib.
- leechcore.lib, vmm.lib — From MemProcFS release (used when building VolkDMA so they get merged into VolkDMA.lib). FTD3XX.lib is not required for the build; FTDI device support loads at runtime if needed.
