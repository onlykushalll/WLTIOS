# wlt_fm_safe

Memory-safe, symlink-correct recursive copy/delete, written to replace the
vulnerable logic in `../operations.c`'s `wlt_fm_copy_directory()`/
`wlt_fm_delete_directory()` (see `docs/STATUS_ROADMAP.md` gap #37).

## Status

Compiles clean and passes all 9 unit tests. Verified twice, both by real
execution: once in the authoring sandbox, and a second time by pulling the
exact bytes that landed in this directory back out and re-running the full
suite against them (2026-08-28) - confirming the file transfer itself
didn't corrupt anything, not just assuming it based on individual write
calls succeeding.

NOT yet compiled in this project's actual target toolchain (WSL2/QEMU), and
NOT yet linked into the real build. Both of those are open steps, not
overlooked ones - see below.

## What this actually is

Two functions with a C-compatible FFI:

    int wlt_fm_safe_copy_directory(const char *src, const char *dst);
    int wlt_fm_safe_delete_directory(const char *path);

Both return 0 on success, negative on error. Both handle symlinks by never
dereferencing them during traversal - a symlink anywhere in the tree
(including as the top-level argument itself) gets recreated (copy) or
unlinked (delete), never followed into its target. That's the property the
original C implementation got wrong: it used a `stat()`-based helper to
decide whether to recurse into a directory entry, which follows symlinks,
so a symlink to an external directory had its *target* recursively deleted
or copied, unbounded by what the caller actually selected.

## Before linking this in

1. Confirm `rustc`/`cargo` (or a cross-compilation setup) is actually
   available in the WSL2/QEMU build environment this project uses for
   everything else. If it isn't, this needs either adding that toolchain
   or vendoring a pre-built static lib - don't assume either without
   checking directly.
2. `cargo build --release` here produces `target/release/libwlt_fm_safe.a`
   (static) and `.so` (dynamic). Prefer the static `.a` for linking into
   `wlt-filemanager` so there's no new runtime dependency introduced.
3. In `wlt-filemanager/Makefile`, add a build step that runs
   `cargo build --release` in this directory before the final link, and
   add `wlt_fm_safe/target/release/libwlt_fm_safe.a` to the link line.
4. In `operations.c`, either replace the bodies of
   `wlt_fm_copy_directory()`/`wlt_fm_delete_directory()` with calls to the
   FFI functions above, or add them as a separate, explicitly-safer path
   and migrate callers incrementally - whichever this project's actual
   risk tolerance prefers. Deliberately not decided here: swapping
   production logic for something not yet verified in the real target
   environment is exactly the kind of thing that needs a build-and-test
   pass first, same principle as everything else flagged this session.
5. Re-run this crate's own test suite (`cargo test`) in the real target
   environment once the toolchain is confirmed - a difference in libc,
   filesystem, or permissions model between here and there is exactly the
   kind of thing that wouldn't show up by reading the code.

## Why Rust for just this one piece

Not proposing a rewrite of wlt-filemanager. This is targeted at the one bug
class that was actually found (twice, in different files) this session in
C - the symlink-recursion bug here, and a related permission-preservation
gap in `file_dnd.c`. Rust's `symlink_metadata` vs `metadata` split makes
"did I mean to follow this symlink" a conscious API choice every time,
structurally, rather than something a single easy-to-miss `stat()` vs
`lstat()` swap can quietly get wrong - which is exactly how the original
bug happened.
