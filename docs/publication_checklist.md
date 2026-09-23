# Public repository checklist

Run this checklist against the exact commit that will be pushed.

- [ ] `git status` contains no unexpected files.
- [ ] no build directories, ELF/map/object/log/cache files are tracked.
- [ ] no passwords, tokens, private keys, usernames, student IDs, private URLs,
      internal IPs, or machine-specific absolute paths are present.
- [ ] NXP files marked confidential/proprietary are not tracked.
- [ ] FreeRTOS and BSD file-level notices remain intact.
- [ ] firmware release binaries are rebuilt from tagged source.
- [ ] firmware SHA-256 checksums are recorded in the Release notes.
- [ ] redistribution rights for every linked binary component are verified.
- [ ] README claims match tests that were actually performed.
- [ ] real screenshots are reviewed for usernames, paths, serial numbers, and
      unrelated desktop information before being added to `images/`.
- [ ] no `git push` is performed from an account/repository not intended for
      public publication.

The current source layout intentionally omits restricted NXP-generated files.
That omission is a publication safety measure and must not be bypassed by
force-adding ignored files.
