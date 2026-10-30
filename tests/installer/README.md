The extracted-folder checks build independently of the ReXGlue SDK, game assets,
renderer, and installer UI. Run from the repository root with a C++23 compiler:

```sh
cmake -S tests/installer -B build/installer-tests
cmake --build build/installer-tests --config Release
ctest --test-dir build/installer-tests -C Release --output-on-failure
```

Tests use non-game fixtures in a unique temporary directory. They check required
modules, deterministic inventories, exclusion of the disc's system-update
directory, source preservation, destination overlap, and unsafe links. Symbolic
link tests report a skip when the platform denies link creation; Windows also
tests directory junctions without requiring symbolic-link privileges.

To validate and inventory a real extracted disc without copying or installing it:

```sh
extracted_disc_tests --inspect "/path/to/extracted/game"
```

With multi-configuration generators the executable is in the `Release`
subdirectory. Folder validation checks readable, nonempty `Default.xex` and
`Data/GameLogic.dll`; it does not verify game hashes or replace the required
Title Update and optional DLC package flow. A fingerprint identifies the
source's sizes and is not a content hash.
