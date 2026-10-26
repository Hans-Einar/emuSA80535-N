# SPR-010 — Governance reconciliation and AlmaLinux delivery

Status: active
Sprint ID: SPR-010
Started: 2026-10-26
Authority: current user's cleanup, ROM-readiness and RPM-publication request
Baseline: c8d647a29d5a160637025fa387e31f15628c3ec1

## Goal

Reconcile current navigation/Timer2 closeout and deliver the existing generic
emulator/debug executable plus an embedding SDK as AlmaLinux 10.2 RPMs.
Publish verified packages to the user's existing Beinerud app repository.

## Scope and exit

ITR-018 / SLC-018 owns only documentation reconciliation, generic build/install
support, RPM packaging and release evidence. Fresh Worker implementation,
independent review, Master verification and traceable publication are required.
No CPU/protocol semantic change, Gate-B implementation, P1000 simulator
implementation, ROM redistribution or SDP installation/migration is included.
