## What this is
I'm trying to get `ntoskrnl.exe`, `hal.dll`, `bootvid.dll`, `ntdll.dll`, and `kdcom.dll` to build with Clang,
in the hope of seeing them work with the MSVC binaries in a normal build.

Currently, it hangs during boot.

## Instructions
You need Clang, LLVM, and CMake.

To compile things, this should work:
```shell
# add -DNT_MSVC=1 to build with modern MSVC
cmake -S. -Bbuild -DCMAKE_TOOLCHAIN_FILE=nt.cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=TRUE -GNinja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

To install the binaries, mount your install and do `tools\copybins <drive letter>`. It backs up the original files.

You can also go to `minkernel/llvmtests` and run `compile.bat`, then run `testload <system32 path>` to make sure the imports all resolve.

## To do

- make libcntpr.lib compile with Clang
- figure out the hang on `SYSTEM32\acpitabl.dat` in NTLDR

