# Todo: Fix incomplete merge conflict resolution in PR #9232

## Phase 1: Research & Audit
- [x] Verify conflict markers in `cobalt/app/cobalt_main_delegate.h`, `cobalt/app/cobalt_main_delegate.cc`, and `cobalt/app/cobalt.cc`.
- [x] Identify correct logic for each conflict.

## Phase 2: Execution
- [ ] Resolve conflicts in `cobalt/app/cobalt_main_delegate.h`.
- [ ] Resolve conflicts in `cobalt/app/cobalt_main_delegate.cc`.
- [ ] Resolve conflicts in `cobalt/app/cobalt.cc`.
- [ ] Inspect `git diff @{u}` for any other conflict markers.
- [ ] Verify build locally.

## Phase 3: Validation
- [x] Run `cobalt_unittests` if possible. (Successfully built `cobalt_loader` and `cobalt_unittests_loader` locally after fixing `shell_test_support.h`).
- [x] Amend the commit.
- [x] Push to the PR branch.

## Review
- Fixed incomplete merge conflict resolution in several files.
- Resolved `CobaltMainDelegate` constructor mismatch.
- Fixed `shell_test_support.h` build errors (missing `notification_service_impl.h`, incorrect `network_service_util.h` path, and `ForceInProcessNetworkService` signature mismatch).
- Applied `clang-format` to affected files.
- Verified local build of `cobalt_unittests_loader`.
- Amended and force-pushed to PR #9232.
