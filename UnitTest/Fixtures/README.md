# UnitTest/Fixtures — Binary Test Fixture Inventory

**Phase F0 / Spec 004 (Apple //e Fidelity)** — staged in F0; populated for real
in later phases (D1, D2, US2). Audited per plan.md
§"Test fixtures (provenance + license posture)".

> **Test isolation contract (constitution §II)**: every test that needs to read
> bytes from a fixture MUST go through `IFixtureProvider`, which is the only
> sanctioned path. No `std::ifstream` of host paths from any test code.
> Anything outside `UnitTest/Fixtures/` is a violation.

## Inventory & provenance matrix

| Fixture                  | Size (bytes)   | Status (F0)  | Provenance                                       | License                             | Commit posture                                        |
|--------------------------|----------------|--------------|--------------------------------------------------|-------------------------------------|-------------------------------------------------------|
| `apple2e.rom`            | 16384          | Real (F0)    | Copied from `ROMs/apple2e.rom` (already in repo) | Apple //e ROM (existing repo policy) | Tracked binary, identical to ROMs/                    |
| `apple2e-video.rom`      | 4096           | Real (F0)    | Copied from `ROMs/apple2e-enhanced-video.rom`    | Apple //e ROM (existing repo policy) | Tracked binary, identical to ROMs/                    |
| `dos33.dsk`              | 0 (placeholder)| Placeholder  | Real DOS 3.3 image lands in D2/US2               | TBD (D2)                            | Zero-byte placeholder so build sees the path           |
| `prodos.po`              | 0 (placeholder)| Placeholder  | Real ProDOS image lands in D2/US2                | TBD (D2)                            | Zero-byte placeholder so build sees the path           |
| `sample.woz`             | 0 (placeholder)| Placeholder  | Real WOZ image lands in D2/US2                   | TBD (D2)                            | Zero-byte placeholder so build sees the path           |
| `copyprotected.woz`      | 0 (placeholder)| Placeholder  | Real protected WOZ lands in D2/US2               | TBD (D2)                            | Zero-byte placeholder so build sees the path           |
| `golden/`                | dir            | Empty        | Golden hashes/framebuffers populated in V1       | n/a                                 | Tracked directory (`.gitkeep`)                         |

## Rules

- All fixtures here are accessed through `IFixtureProvider::OpenFixture()` with
  a path **relative to this directory**.
- `IFixtureProvider` rejects `..`, drive letters, and absolute roots
  (returns `E_INVALIDARG`).
- Real disk-image fixtures (DOS 3.3, ProDOS, WOZ) are intentionally zero-byte
  placeholders in F0; they are populated when the disk-controller phases (D1,
  D2) and US2 lock down their license posture.
- Adding a new fixture: append a row to the matrix above with provenance,
  license, and size, then commit the file.

## See also

- `specs/004-apple-iie-fidelity/plan.md` §"Test fixtures (provenance + license posture)"
- `specs/004-apple-iie-fidelity/contracts/IFixtureProvider.md`
