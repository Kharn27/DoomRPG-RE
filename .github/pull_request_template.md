## Summary

<!-- What bounded milestone or repository change does this PR implement? -->

## Validation

- [ ] Base is this repository's `main` branch.
- [ ] `ESP32 CYD Build` CI is green.
- [ ] Real classic CYD hardware was tested if runtime/gameplay/render behavior changed.
- [ ] Relevant Serial logs were captured for hardware claims.
- [ ] `ESP32/PORTING_STATUS.md` was updated only after hardware PASS, when applicable.
- [ ] Commits after the hardware-tested code boundary are documentation-only, when applicable.

## ESP32 invariants

- [ ] `shapeData == NULL` remains preserved.
- [ ] `mediaTexels == NULL` remains preserved.
- [ ] Runtime assets remain backed by `/DoomRPG-ESP32.pak`; no runtime ZIP dependency was introduced.
- [ ] No new map-wide/pointer-heavy ownership was introduced for migrated native gameplay.

## Notes

<!-- Deferred behavior, fail-closed boundaries, RAM witness, or anything useful for review. -->
