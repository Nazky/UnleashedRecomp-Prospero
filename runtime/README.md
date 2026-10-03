# The generated runtime module (libc.prx)

`sce_module/libc.prx` is the module a title carries where the console's loader
expects a C library companion. It is generated, not copied: `make libc` (or
`tools/rebuild-libc.sh`) builds `tooling/native/libc_builder.cpp` and runs it on the
two manifests in `tooling/native/runtime/`, then signs the result as a fake SELF.
The code is Copyright (C) 2026 BlackBearReloaded (ps5-native-app-boilerplate),
GPL-3.0-or-later.

What the module contains:

- a few dozen bytes of machine code written by the builder: an initializer that hands
  the title's `malloc`/`free`/`posix_memalign` to the kernel's heap hook and registers
  three thread callbacks, and functions that return zero;
- loader metadata: the NIDs, ELF bindings, types and sizes of the symbols the
  console's libc exports (`api-surface.txt`, recorded by observing that module, as
  its header says), the imports it declares (`imports.txt`), and a
  segment layout that matches what the loader accepts.

It contains no instructions or data copied from a console module. That is
established by the generator's source, and by reproduction: on 2026-09-29 the
module rebuilt from this repository's committed files alone was byte-identical to
the one below, which is also the `libc.prx` in every published PS5_RetroArch release
(v0.1.0-alpha.1 to v0.5.0-alpha.5). The name "clean-room", used for this module
elsewhere, is BlackBearReloaded's description of how it was written; this
repository has no record of that process beyond the source itself.

The expected artifact has SHA-256:

```text
e6ff45d16adf687855cc3b33b0c8a4132b6504360b221e0a34c7e99fb3ba0036
```

Generate it from the repository root with `make libc`, then verify it from this
directory with `sha256sum -c libc.prx.sha256`. The generated file is ignored by
Git; a normal build creates it too. The upstream description of the generator and
its compatibility scope is ps5-native-app-boilerplate's `docs/RUNTIME_SHIM.md`.
