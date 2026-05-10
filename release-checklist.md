# PowerTOP release checklist

Prior to making a release or release candidate, perform all of the
checks below to ensure a smooth experience for our users.

> **Agent note:** When performing these steps, treat every section as
> a gate — do not proceed to the next section if any item in the
> current section fails.

## Version bump

- [ ] Ask the user for the new version number.
      Suggest at least two options (use git tag to check for current latest):
      1. Next minor/major release (e.g. `v2.X`)
      2. Next release candidate (e.g. `v2.X-rc1`)
- [ ] Update the version in `meson.build` (`version:` field)
- [ ] Verify the version appears correctly in `powertop --version` output

## Translations

- [ ] Audit and update `POTFILES.in` (check for new source files with translatable strings)
- [ ] Regenerate the gettext template (`powertop.pot`) and merge into all `.po` files

## Build checks (all must compile with zero errors and zero warnings)

Each build must be a **clean build** (`meson setup --wipe`), not incremental.
All builds except standard should include `-Denable-tests=true`.

- [ ] Meson standard build: `meson setup --wipe -Denable-tests=true build`
- [ ] Meson release build: `meson setup --wipe --buildtype=release -Denable-tests=true build_release`
- [ ] Meson debug build: `meson setup --wipe --buildtype=debug -Denable-tests=true build_debug`
- [ ] Meson ASAN build: `meson setup --wipe -Denable-tests=true -Db_coverage=true -Db_sanitize=address build_acov`
- [ ] Meson gcov build: `meson setup --wipe -Denable-tests=true -Db_coverage=true build_cov`

## Tests (all five build types, all must pass with no failures)

- [ ] Standard build: full test suite
- [ ] Release build: full test suite (`ninja -C build_release test`)
- [ ] Debug build: full test suite (`ninja -C build_debug test`)
- [ ] ASAN (Address Sanitizer) build: full test suite (`ninja -C build_acov test`)
- [ ] gcov build: full test suite (`ninja -C build_cov test`)

## Memory leak check

- [ ] Valgrind full leak check: `sudo valgrind --leak-check=full --show-leak-kinds=all build/powertop --html --time=3`
      Only the two known library-internal leaks are acceptable (see `local.md`).
      Any new leaks must be fixed before release.

## Test coverage

- [ ] Test coverage report on the standard test suite
- [ ] Additional coverage from running `sudo powertop --once --time=3`

## Release notes

Store in `doc/relnotes.md`.

- [ ] Update the "Recent releases" table near the top of `README.md`
      with the new version and a one-line summary
- [ ] Summary of top user-visible enhancements and changes
      (new tunables, UI changes, other new features)
- [ ] Summary of new command-line options
- [ ] **Short** summary of internal changes (no more than 3 lines)

## Tagging the release

> **Agent note:** Only perform this section if all items above have
> passed. Ask the user explicitly for confirmation before tagging.

- [ ] Confirm with the user that all checks passed and they are ready to tag
- [ ] `git tag -a vX.Y -m "Release vX.Y"`
- [ ] `git push origin vX.Y`
