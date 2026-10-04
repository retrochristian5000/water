# File Logic
- Don't shove components into the wrong files.
- Identify the owning component before editing; an error being reported by a component does not prove that component owns the bug.
- Follow the call path across boundaries before moving code or responsibilities.

# Compatibility Boundaries
- Keep DOS, Win16, Win9x, and Windows NT personalities distinct unless evidence proves shared behavior.
- Do not flatten KRNL286, KRNL386, WIN386, NTVDM, KERNEL32, NTDLL, DOS/BIOS, or toolchain responsibilities into one generic layer.
- Keep Windows version, execution mode, ISA, object format, and subsystem distinctions explicit.
- Treat upstream Wine behavior as evidence, not automatically as the Windows specification.
- QEMU, DOSBox-X, WSL, and other projects may provide techniques or comparison points, but must not define Water's architecture.

# ABI Discipline
- Identify the guest ABI and host ABI before ABI-sensitive changes.
- Keep 16-bit, 32-bit, and 64-bit guest layouts explicit.
- Use fixed-width guest-facing types where layout matters; do not derive guest layouts from host `sizeof(void *)`, `long`, or `size_t`.
- Preserve calling conventions, structure packing, ordinals, exports, pointer widths, and object-format rules unless evidence supports a change.
- Do not assume a host-native pointer or integer representation is valid for guest data.

# Research and Search
- Recover existing Water code, comments, tests, and relevant commit history before designing a new fix.
- Search narrowly by owning component, symbol, ABI, error, and Windows version before widening the search.
- Prefer contemporary documentation, binaries, observable behavior, tests, and independent implementations when resolving compatibility questions.
- Do not replace older Water behavior merely because a newer external implementation differs.

# Change Scope
- Make the smallest change that fixes the owning problem without crossing unrelated component boundaries.
- Do not mix unrelated cleanup, refactors, or formatting churn into a functional fix.
- Remove dead code only after confirming that no supported personality or build path still owns it.
- Reuse existing mechanisms when they already model the required behavior correctly.

# Verification
- Distinguish source-audited, compiled, test-passed, and runtime-confirmed results.
- Do not call a problem fixed solely because the code looks correct or compiles.
- Test the affected Windows personality and ABI when practical, and add a regression test when the bug can be isolated.
- If a claim has not been runtime-tested, say so instead of implying otherwise.

# Git Safety
- Do not rewrite published history or force-push normal fixes.
- Do not create extra branches unless the task actually requires one.
- Keep commits coherent and narrowly scoped.
- Do not leave completed repo changes only in a local working tree when they are intended for the shared repository.

# Permissions
- The configure file must have +x enabled.
- Preserve executable bits and other intentional file modes when editing scripts.
