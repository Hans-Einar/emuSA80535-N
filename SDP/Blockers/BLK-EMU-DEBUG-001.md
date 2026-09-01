# BLK-EMU-DEBUG-001 -- required SLC-010 baseline is not on master

## Status

Open; blocks Issue #6 before implementation.

## Conflict

Issue #6 requires work to start from exact current master after PR #8 /
SLC-010 is merged, and requires all accepted SLC-010 Stage-2 semantics to be
preserved as regressions. On 2026-09-01:

- `origin/master` was `88ccb2b45976c137820cdc56eec550d953bcf76d`;
- PR #8 was open and unmerged at
  `fe62b9127c27e075a1353bcac1564f713491a70c`;
- accepted SLC-010 product commit
  `ba00ef17af57076b01c7f548e8996a7d36a5c591` was not an ancestor of
  `origin/master`.

Starting from master would omit the required regression semantics. Starting
from PR #8 would violate the explicit current-master baseline instruction.
Merging PR #8 is not authorized as part of Issue #6.

## Required Steering action

Merge PR #8 to master, or explicitly authorize a replacement exact baseline
that contains the accepted SLC-010 semantics. After the action, Master must
fetch, identify the new exact `origin/master`, confirm SLC-010 ancestry, repeat
the frozen DAP PR #4 contract comparison, and only then define an active Sprint,
Iteration and Slice.

## Checkpoint

https://github.com/Hans-Einar/emuSA80535-N/issues/6#issuecomment-5491928272
