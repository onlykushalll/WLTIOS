//! wlt_fm_safe — memory-safe, symlink-correct recursive copy/delete.
//!
//! Built in response to a real, severe finding in WLTIOS's C implementation
//! (wlt-filemanager/operations.c, see STATUS_ROADMAP.md gap #37):
//! `wlt_fm_copy_directory()` and `wlt_fm_delete_directory()` decided
//! whether to recurse into a directory entry using a `stat()`-based
//! helper, which follows symlinks. A directory containing a symlink to
//! another directory would have that symlink's *target* recursively
//! deleted (or copied) — unbounded by what the caller actually selected.
//! The C fix added explicit `lstat()` guards at both the recursion entry
//! point and the per-entry level, in two separate places that have to be
//! kept in sync by hand.
//!
//! This Rust version makes the bug structurally harder to reintroduce:
//! there is exactly ONE recursive function per operation, and the very
//! first thing it does is call `symlink_metadata` (Rust's non-following
//! stat) rather than `metadata` (the following one). The top-level-path-
//! is-itself-a-symlink case and the nested-symlink-encountered-during-
//! traversal case are the same code path, not two checks a future editor
//! could accidentally desync.
//!
//! STATUS: compiles clean and passes all 9 unit tests (including the
//! exact exploit scenario, both directions, plus symlink-cycle
//! termination and a real C-linkage smoke test) in the author's sandbox
//! (Ubuntu, rustc 1.75.0, 2026-08-26). NOT yet build-verified in this
//! project's actual target toolchain (WSL2/QEMU) — rustc/cargo
//! availability there is unconfirmed, and this is intentionally not
//! wired into the default Makefile `all:` target until that's checked.
//! See the README in this directory for exact integration steps.

use std::ffi::{CStr, c_char, c_int};
use std::fs;
use std::io;
use std::os::unix::fs::{symlink, MetadataExt, PermissionsExt};
use std::path::Path;

/// Recursively delete `path`.
///
/// - If `path` itself is a symlink, unlink just the symlink. Never follow it.
/// - For a real directory, recurse into entries. Any entry that is itself a
///   symlink is unlinked directly, never recursed into — this is the fix for
///   the WLTIOS finding: a symlink anywhere in the tree pointing outside it
///   (a parent, `/`, `/home`, anywhere) no longer has its target destroyed.
/// - Regular files are removed normally.
pub fn delete_directory_safe(path: &Path) -> io::Result<()> {
    let meta = fs::symlink_metadata(path)?;

    if meta.file_type().is_symlink() {
        return fs::remove_file(path);
    }
    if !meta.is_dir() {
        return fs::remove_file(path);
    }

    for entry in fs::read_dir(path)? {
        let entry = entry?;
        let entry_path = entry.path();
        let entry_meta = fs::symlink_metadata(&entry_path)?;

        if entry_meta.file_type().is_symlink() {
            fs::remove_file(&entry_path)?;
        } else if entry_meta.is_dir() {
            delete_directory_safe(&entry_path)?;
        } else {
            fs::remove_file(&entry_path)?;
        }
    }
    fs::remove_dir(path)
}

/// Recursively copy `src` to `dst`.
///
/// - If `src` is a symlink, recreate a symlink at `dst` pointing at the same
///   target. Never dereference it.
/// - For a real directory, create the destination directory (preserving
///   permission bits) and recurse into entries with the same symlink-safe
///   rule as delete.
/// - Regular files are copied with their permission bits preserved.
pub fn copy_directory_safe(src: &Path, dst: &Path) -> io::Result<()> {
    let meta = fs::symlink_metadata(src)?;

    if meta.file_type().is_symlink() {
        let target = fs::read_link(src)?;
        return symlink(&target, dst);
    }
    if !meta.is_dir() {
        return copy_file_preserve_mode(src, dst);
    }

    fs::create_dir_all(dst)?;
    fs::set_permissions(dst, fs::Permissions::from_mode(meta.mode()))?;

    for entry in fs::read_dir(src)? {
        let entry = entry?;
        let src_entry = entry.path();
        let dst_entry = dst.join(entry.file_name());
        let entry_meta = fs::symlink_metadata(&src_entry)?;
        if entry_meta.file_type().is_symlink() {
            let target = fs::read_link(&src_entry)?;
            symlink(&target, &dst_entry)?;
        } else if entry_meta.is_dir() {
            copy_directory_safe(&src_entry, &dst_entry)?;
        } else {
            copy_file_preserve_mode(&src_entry, &dst_entry)?;
        }
    }

    Ok(())
}

fn copy_file_preserve_mode(src: &Path, dst: &Path) -> io::Result<()> {
    let meta = fs::symlink_metadata(src)?;
    fs::copy(src, dst)?;
    fs::set_permissions(dst, fs::Permissions::from_mode(meta.mode()))?;
    Ok(())
}

// ---------------------------------------------------------------------
// C-compatible FFI layer, so this can actually be linked into the
// existing C codebase rather than staying a demonstration.
// ---------------------------------------------------------------------

const WLT_FM_SAFE_OK: c_int = 0;
const WLT_FM_SAFE_ERR_INVALID_ARG: c_int = -1;
const WLT_FM_SAFE_ERR_IO: c_int = -2;
/// # Safety
/// `path` must be a valid, NUL-terminated C string pointer.
#[no_mangle]
pub unsafe extern "C" fn wlt_fm_safe_delete_directory(path: *const c_char) -> c_int {
    if path.is_null() {
        return WLT_FM_SAFE_ERR_INVALID_ARG;
    }
    let path = match CStr::from_ptr(path).to_str() {
        Ok(s) => Path::new(s),
        Err(_) => return WLT_FM_SAFE_ERR_INVALID_ARG,
    };
    match delete_directory_safe(path) {
        Ok(()) => WLT_FM_SAFE_OK,
        Err(_) => WLT_FM_SAFE_ERR_IO,
    }
}

/// # Safety
/// `src` and `dst` must be valid, NUL-terminated C string pointers.
#[no_mangle]
pub unsafe extern "C" fn wlt_fm_safe_copy_directory(
    src: *const c_char,
    dst: *const c_char,
) -> c_int {
    if src.is_null() || dst.is_null() {
        return WLT_FM_SAFE_ERR_INVALID_ARG;
    }
    let src = match CStr::from_ptr(src).to_str() {
        Ok(s) => Path::new(s),
        Err(_) => return WLT_FM_SAFE_ERR_INVALID_ARG,
    };
    let dst = match CStr::from_ptr(dst).to_str() {
        Ok(s) => Path::new(s),
        Err(_) => return WLT_FM_SAFE_ERR_INVALID_ARG,
    };
    match copy_directory_safe(src, dst) {
        Ok(()) => WLT_FM_SAFE_OK,
        Err(_) => WLT_FM_SAFE_ERR_IO,
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::fs::File;
    use std::io::Write;

    /// The actual exploit scenario: a directory contains a symlink pointing
    /// OUTSIDE the tree being deleted. Deleting the outer directory must
    /// not touch the target's contents — only the symlink disappears.
    #[test]
    fn delete_does_not_follow_symlink_to_external_target() {
        let tmp = tempdir();
        let victim_dir = tmp.join("victim_external_dir");
        let victim_file = victim_dir.join("do_not_delete_me.txt");
        fs::create_dir(&victim_dir).unwrap();
        File::create(&victim_file).unwrap().write_all(b"important data").unwrap();

        let to_delete = tmp.join("folder_user_selected");
        fs::create_dir(&to_delete).unwrap();
        File::create(to_delete.join("normal_file.txt")).unwrap();
        symlink(&victim_dir, to_delete.join("sneaky_link")).unwrap();
        delete_directory_safe(&to_delete).unwrap();

        assert!(!to_delete.exists(), "the selected folder should be gone");
        assert!(victim_dir.exists(), "the external symlink TARGET directory must survive");
        assert!(victim_file.exists(), "the external symlink target's FILE must survive");
        assert_eq!(fs::read_to_string(&victim_file).unwrap(), "important data");

        cleanup(&tmp);
    }

    /// A symlink that is itself the top-level argument must also just be
    /// unlinked, not dereferenced-and-recursed-into. This matters because
    /// wlt_fm_move()'s EXDEV fallback in the original C code reaches
    /// delete_directory() with `path` itself potentially being a symlink.
    #[test]
    fn delete_top_level_symlink_only_unlinks_the_link() {
        let tmp = tempdir();
        let victim_dir = tmp.join("victim_top_level");
        fs::create_dir(&victim_dir).unwrap();
        File::create(victim_dir.join("keep_me.txt")).unwrap();

        let link_path = tmp.join("link_to_victim");
        symlink(&victim_dir, &link_path).unwrap();

        delete_directory_safe(&link_path).unwrap();

        assert!(!link_path.exists(), "the symlink itself should be gone");
        assert!(victim_dir.exists(), "the target directory must survive");
        assert!(victim_dir.join("keep_me.txt").exists());

        cleanup(&tmp);
    }
    /// A symlink forming a cycle (pointing back to an ancestor) must not
    /// cause infinite recursion during delete.
    #[test]
    fn delete_does_not_infinite_loop_on_a_cycle() {
        let tmp = tempdir();
        let to_delete = tmp.join("cyclic_folder");
        fs::create_dir(&to_delete).unwrap();
        symlink(&tmp, to_delete.join("cycle_back_to_ancestor")).unwrap();

        // Must terminate. If this hangs, the safety property is broken.
        delete_directory_safe(&to_delete).unwrap();
        assert!(!to_delete.exists());
        assert!(tmp.exists(), "the ancestor the cycle pointed to must survive");

        cleanup(&tmp);
    }

    /// Copy must recreate a symlink as a symlink, not duplicate the
    /// target's contents into the destination tree.
    #[test]
    fn copy_recreates_symlink_instead_of_dereferencing() {
        let tmp = tempdir();
        let external_target = tmp.join("external_target_dir");
        fs::create_dir(&external_target).unwrap();
        File::create(external_target.join("f.txt")).unwrap().write_all(b"x").unwrap();

        let src = tmp.join("src_folder");
        fs::create_dir(&src).unwrap();
        symlink(&external_target, src.join("link")).unwrap();

        let dst = tmp.join("dst_folder");
        copy_directory_safe(&src, &dst).unwrap();
        let copied_link = dst.join("link");
        let link_meta = fs::symlink_metadata(&copied_link).unwrap();
        assert!(link_meta.file_type().is_symlink(), "must be recreated as a symlink");
        assert_eq!(fs::read_link(&copied_link).unwrap(), external_target);

        cleanup(&tmp);
    }

    /// A symlink cycle must not cause infinite recursion during copy either.
    #[test]
    fn copy_does_not_infinite_loop_on_a_cycle() {
        let tmp = tempdir();
        let src = tmp.join("cyclic_src");
        fs::create_dir(&src).unwrap();
        symlink(&tmp, src.join("cycle_back")).unwrap();

        let dst = tmp.join("cyclic_dst");
        copy_directory_safe(&src, &dst).unwrap();
        assert!(fs::symlink_metadata(dst.join("cycle_back")).unwrap().file_type().is_symlink());

        cleanup(&tmp);
    }

    /// Ordinary regression sanity: a plain nested tree with real files,
    /// no symlinks at all, must still copy and delete correctly — this
    /// rewrite is worthless if it breaks the common case to fix the rare
    /// one.
    #[test]
    fn ordinary_nested_tree_copies_and_deletes_correctly() {
        let tmp = tempdir();
        let src = tmp.join("ordinary_src");
        fs::create_dir_all(src.join("sub/subsub")).unwrap();
        File::create(src.join("a.txt")).unwrap().write_all(b"aaa").unwrap();
        File::create(src.join("sub/b.txt")).unwrap().write_all(b"bbb").unwrap();
        File::create(src.join("sub/subsub/c.txt")).unwrap().write_all(b"ccc").unwrap();

        let dst = tmp.join("ordinary_dst");
        copy_directory_safe(&src, &dst).unwrap();

        assert_eq!(fs::read_to_string(dst.join("a.txt")).unwrap(), "aaa");
        assert_eq!(fs::read_to_string(dst.join("sub/b.txt")).unwrap(), "bbb");
        assert_eq!(fs::read_to_string(dst.join("sub/subsub/c.txt")).unwrap(), "ccc");

        delete_directory_safe(&src).unwrap();
        assert!(!src.exists());
        assert!(dst.exists(), "deleting src must not touch the independent copy at dst");

        cleanup(&tmp);
    }

    /// Permission bits should survive a copy — matches what the original
    /// C wlt_fm_copy_file() got right and file_dnd.c (audited earlier
    /// this session, gap #38) got wrong.
    #[test]
    fn copy_preserves_file_permissions() {
        let tmp = tempdir();
        let src = tmp.join("perm_src");
        fs::create_dir(&src).unwrap();
        let f = src.join("exec_me.sh");
        File::create(&f).unwrap();
        fs::set_permissions(&f, fs::Permissions::from_mode(0o750)).unwrap();

        let dst = tmp.join("perm_dst");
        copy_directory_safe(&src, &dst).unwrap();
        let copied_mode = fs::metadata(dst.join("exec_me.sh")).unwrap().mode() & 0o777;
        assert_eq!(copied_mode, 0o750);

        cleanup(&tmp);
    }

    #[test]
    fn ffi_layer_rejects_null_pointers() {
        unsafe {
            assert_eq!(
                wlt_fm_safe_delete_directory(std::ptr::null()),
                WLT_FM_SAFE_ERR_INVALID_ARG
            );
            assert_eq!(
                wlt_fm_safe_copy_directory(std::ptr::null(), std::ptr::null()),
                WLT_FM_SAFE_ERR_INVALID_ARG
            );
        }
    }

    #[test]
    fn ffi_layer_round_trip_through_c_strings() {
        use std::ffi::CString;
        let tmp = tempdir();
        let src = tmp.join("ffi_src");
        fs::create_dir(&src).unwrap();
        File::create(src.join("x.txt")).unwrap();
        let dst = tmp.join("ffi_dst");
        let c_src = CString::new(src.to_str().unwrap()).unwrap();
        let c_dst = CString::new(dst.to_str().unwrap()).unwrap();
        unsafe {
            assert_eq!(
                wlt_fm_safe_copy_directory(c_src.as_ptr(), c_dst.as_ptr()),
                WLT_FM_SAFE_OK
            );
        }
        assert!(dst.join("x.txt").exists());

        let c_src2 = CString::new(src.to_str().unwrap()).unwrap();
        unsafe {
            assert_eq!(wlt_fm_safe_delete_directory(c_src2.as_ptr()), WLT_FM_SAFE_OK);
        }
        assert!(!src.exists());

        cleanup(&tmp);
    }

    fn tempdir() -> std::path::PathBuf {
        let mut p = std::env::temp_dir();
        let unique = format!(
            "wlt_fm_safe_test_{}_{}",
            std::process::id(),
            std::time::SystemTime::now()
                .duration_since(std::time::UNIX_EPOCH)
                .unwrap()
                .as_nanos()
        );
        p.push(unique);
        fs::create_dir_all(&p).unwrap();
        p
    }

    fn cleanup(tmp: &Path) {
        let _ = fs::remove_dir_all(tmp);
    }
}
