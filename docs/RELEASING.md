# Releasing MyBuddyBot

MyBuddyBot uses Semantic Versioning (SemVer) and annotated git tags.

## Version Policy

- `MAJOR` (`X.0.0`): incompatible API/behavior changes.
- `MINOR` (`X.Y.0`): backward-compatible features.
- `PATCH` (`X.Y.Z`): backward-compatible fixes (increment `Z`).

Tags are always formatted as `vX.Y.Z` (for example: `v1.2.3`).

## Release Checklist

1. Ensure `master`/`main` is clean and validation passes (`ctest --test-dir <build-dir> --output-on-failure`, `cargo test`, and any relevant manual checks).
2. Move items from `## [Unreleased]` to a new version section in `CHANGELOG.md`.
3. Update `project(MyBuddyBot VERSION X.Y.Z ...)` in `CMakeLists.txt`.
4. Commit release metadata:
   `git commit -am "Release vX.Y.Z"`
5. Create an annotated tag:
   `git tag -a vX.Y.Z -m "Release vX.Y.Z"`
6. Push commit and tag:
   `git push origin <default-branch>`
   `git push origin vX.Y.Z`

## Notes

- `/health` shows the compiled SemVer and the exact git tag (or `untagged`).
- Keep `CHANGELOG.md` primarily user-facing: focus on behavior changes, and include internal refactors only when they materially affect build, architecture, or contributor workflows.
- If the release touches Rust crates, run `cargo fmt --all --check` and `cargo clippy --all-targets --all-features` before tagging.
