# FlatBuffers (vendored runtime headers)

FlatBuffers 25.12.19, taken from the server's vcpkg install:

    Server/build/windows-x64/vcpkg_installed/x64-windows/include/flatbuffers/

Only the 16 headers the generated code actually reaches are here (183 KB). The
compiler-side headers — `idl.h`, `flatc.h`, `code_generators.h`, `reflection*.h`
and friends — are not needed at runtime and are left out.

Unreal 5.8 ships a `Flatbuffers_v24.3.25.tps` licence notice but no usable
headers, so there is nothing in the engine to depend on instead.

## Updating

The version has to match the `flatc` that generates
`Source/HeavenHyperVoice/Net/Generated/field_generated.h`. Wire format is stable
across these versions, but the generated code calls into the runtime, so keep
them together. Re-copy the same 16 files after a vcpkg update.
