# WLTIOS Coding Style

> **Enforced.** All code must follow this style. The reviewer agent checks compliance.

## 1. File Organization

### File Header (MANDATORY)

Every source file starts with a header comment:

**C/C:**
```c
/**
 * @file memory.c
 * @brief Parses UEFI memory map for the bootloader
 * @module wlt-boot/memory
 * @purpose Provides the memory map to seL4 so it knows available RAM
 * @dependencies: efi.h, log.h
 * @tested_by: tests/boot/memory_test.c
 * @spec: docs/specs/wlt-boot.md
 * @adr: docs/adr/01-uefi-vs-bios.md
 */
```

**Rust:**
```rust
//! @file package.rs
//! @brief Package manager core logic
//! @module wlt-pkg/core
//! @purpose Installs, removes, updates packages atomically
//! @dependencies: serde, ring (crypto)
//! @tested_by: tests/package_test.rs
//! @spec: docs/specs/wlt-pkg.md
```

### File Length

- **Max 500 lines per file.** If longer, split into multiple files.
- **One responsibility per file.** Don't mix concerns.

## 2. Naming Conventions

### Files

| Type | Pattern | Example |
|------|---------|---------|
| Source | `snake_case.c` / `.rs` | `memory_map.c` |
| Header | `snake_case.h` | `boot_config.h` |
| Test | `snake_case_test.c` | `memory_map_test.c` |
| Spec | `kebab-case.md` | `wlt-boot.md` |

### Functions

**C:**
```c
// snake_case, prefix with module name
wlt_boot_load_kernel()      // ✅ Good
wlt_load()                  // ❌ No module prefix
LoadKernel()                // ❌ No camelCase in C
```

**Rust:**
```rust
// snake_case
fn load_kernel() -> Result<()> { }    // ✅ Good
fn LoadKernel() { }                   // ❌ No camelCase for functions
```

### Types (structs, enums, typedefs)

**C:**
```c
// PascalCase with wlt_ prefix
typedef struct {
    uint64_t address;
    uint64_t size;
} wlt_MemoryRegion;          // ✅ Good

typedef struct memory_region {  // ❌ No snake_case for types
```

**Rust:**
```rust
// PascalCase
struct MemoryRegion {       // ✅ Good
    address: u64,
    size: u64,
}
```

### Constants & Macros

**C:**
```c
#define WLT_BOOT_TIMEOUT_MS 3000    // ✅ UPPER_CASE
#define MAX_SERVICES 64             // ✅ Good
#define timeout 3                   // ❌ No lowercase macros
```

**Rust:**
```rust
const WLT_BOOT_TIMEOUT_MS: u64 = 3000;    // ✅ UPPER_CASE
const MAX_SERVICES: usize = 64;           // ✅ Good
```

### Variables

**C:**
```c
// snake_case
uint64_t kernel_size = 0;       // ✅ Good
uint64_t KernelSize = 0;        // ❌ No camelCase
uint64_t ksz = 0;               // ❌ No abbreviations (except loop vars)
```

**Rust:**
```rust
let kernel_size: u64 = 0;       // ✅ snake_case
```

### Loop Variables

```c
// i, j, k are OK for loops
for (int i = 0; i < count; i++) { }    // ✅ Good
for (int index = 0; index < count; index++) { }  // ✅ Also good
```

## 3. Formatting

### Indentation

- **C:** 4 spaces (no tabs)
- **Rust:** 4 spaces (no tabs) — `rustfmt` enforces this
- **Assembly:** 8 spaces (no tabs)

### Line Length

- **Max 100 characters.** Break long lines.

### Braces

**C (Allman style):**
```c
if (condition)
{
    do_something();
}
else
{
    do_other_thing();
}
```

**Rust (default rustfmt):**
```rust
if condition {
    do_something();
} else {
    do_other_thing();
}
```

### Functions

```c
// One return type per line, params aligned
static wlt_Error
wlt_boot_load_kernel(const char *path,
                     uint64_t *load_address,
                     uint64_t *entry_point)
{
    // ...
}
```

## 4. Comments

### When to Comment

- **WHY, not WHAT.** The code shows what; comments explain why.
- **Non-obvious logic.** If you had to think about it, comment it.
- **Workarounds.** Always explain why you're not doing it the "normal" way.
- **TODOs.** Use `TODO(module): description` format. Never bare `TODO`.

### Comment Style

```c
// Single-line comment (preferred for C99+)

/* 
 * Multi-line comment
 * for longer explanations
 */

/** 
 * Documentation comment (for headers)
 * Parsed by doxygen
 */
```

### Bad Comments

```c
i++;  // Increment i                    // ❌ Obvious
i++;  // TODO: fix this                 // ❌ No context, no module
// This code was written by Kushal      // ❌ Useless
```

## 5. Error Handling

### C — Return Codes

```c
typedef enum {
    WLT_OK = 0,
    WLT_ERR_INVALID_ARG = -1,
    WLT_ERR_NOT_FOUND = -2,
    WLT_ERR_PERMISSION = -3,
    WLT_ERR_IO = -4,
    WLT_ERR_NOMEM = -5,
} wlt_Error;
```

**Every function returns `wlt_Error`.** Check all return values.

```c
wlt_Error err = wlt_boot_load_kernel(path, &addr, &entry);
if (err != WLT_OK)
{
    wlt_log_error("Failed to load kernel: %s", wlt_error_str(err));
    return err;
}
```

### Rust — Result

```rust
fn load_kernel(path: &str) -> Result<Kernel, WltError> {
    // ...
}
```

**Never use `unwrap()` in production code.** Use `?` operator or explicit match.

## 6. Memory Management

### C

- **No malloc in PID 1 (WLT-Init).** Use static buffers.
- **All malloc'd memory must be freed.** Use RAII patterns where possible.
- **Check all allocations:**
```c
void *ptr = malloc(size);
if (ptr == NULL)
{
    return WLT_ERR_NOMEM;
}
```

### Rust

- Use ownership system (no manual memory management)
- Use `Box`, `Rc`, `Arc` appropriately
- Avoid `unsafe` unless absolutely necessary

## 7. Security

### Input Validation (MANDATORY)

All external input must be validated:

```c
wlt_Error
wlt_parse_config(const char *path)
{
    if (path == NULL)
    {
        return WLT_ERR_INVALID_ARG;
    }
    
    size_t len = strlen(path);
    if (len == 0 || len > WLT_MAX_PATH)
    {
        return WLT_ERR_INVALID_ARG;
    }
    
    // ... proceed with parsing
}
```

### No Hardcoded Credentials

```c
// ❌ NEVER
const char *api_key = "sk-1234567890";

// ✅ Good — read from environment or key vault
const char *api_key = wlt_keyvault_get("api_key");
```

### Buffer Operations

- Always use bounded string operations: `strncpy`, `snprintf` — never `strcpy`, `sprintf`
- Always check buffer sizes before memcpy

## 8. Testing

### Test File Naming

```
tests/<module>/<file>_test.c
```

### Test Structure

```c
// tests/boot/memory_map_test.c

void test_memory_map_parse(void)
{
    // Setup
    wlt_MemoryMap map;
    
    // Execute
    wlt_Error err = wlt_boot_parse_memory_map("test_data/memmap.bin", &map);
    
    // Verify
    ASSERT(err == WLT_OK);
    ASSERT(map.region_count > 0);
    ASSERT(map.regions[0].type == WLT_MEM_CONVENTIONAL);
    
    // Cleanup
    wlt_memory_map_free(&map);
}
```

## 9. Git Commit Messages

Format: `[module] action: description`

```
[wlt-boot] implement: UEFI memory map parsing

- Added EfiMemoryMap parser to boot/memory.c
- Required for seL4 to know available RAM
- Spec: docs/specs/wlt-boot.md
- ADR: docs/adr/01-uefi-vs-bios.md
```

| Action | When |
|--------|------|
| `implement` | New feature |
| `fix` | Bug fix |
| `refactor` | Code restructure (no behavior change) |
| `docs` | Documentation only |
| `test` | Test additions/changes |
| `chore` | Build, config, tooling |

## 10. Forbidden Patterns

| Pattern | Why | Do Instead |
|---------|-----|------------|
| `goto` | Spaghetti code | Use early returns |
| `strcpy`, `sprintf` | Buffer overflow | `strncpy`, `snprintf` |
| `system()` | Shell injection | Use `execve` directly |
| `eval` (in any language) | Code injection | Parse + execute safely |
| Hardcoded secrets | Security risk | Use key vault |
| `// TODO` without context | Forgotten work | `TODO(module): description` |
| Magic numbers | Unclear code | Named constants |
| Files > 500 lines | Hard to maintain | Split into modules |
| Functions > 50 lines | Hard to understand | Break into smaller functions |

---

**This document is enforced by the Reviewer agent. Code that violates these rules is rejected.**
