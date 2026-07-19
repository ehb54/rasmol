# RasMol `align` command — research notes

**Status:** the `align` command is **undocumented, unfinished, and non-functional**
in this fork. It is *not* part of RasMol's supported command surface. This
document captures everything learned while investigating it, so that a future
effort to actually complete the feature has a starting point.

**Date of investigation:** 2026-07-18
**Branch:** `somo-modernize` (based on the `RasMol-2.7.6.0` tag)

---

## TL;DR

- `align <n>` superposes the current molecule onto another loaded molecule.
- It is reachable only after loading ≥2 molecules (that path was itself crashing
  until the `InitialiseMultiple()` fix in commit `c7a51f0`; it now runs).
- It **produces wrong results**: aligning two *identical* structures yields
  rmsd ≈ 2.58 Å where a correct superposition must give ≈ 0. This is true for
  **all** modes, including Kabsch (which does not use the reconstructed
  `CQRHLERP`), so the fault is in the alignment feature itself, not only in the
  reconstruction.
- The feature was **work-in-progress in Bernstein's own development** — his
  commit messages explicitly say the field calculations still needed revising
  and that a CNearTree patch was required.
- It depends on `CQRHLERP`, a quaternion routine that exists in **no public
  library and nowhere in RasMol source** — only in Bernstein's private/
  unreleased CQRlib. We ship a best-effort **reconstruction** so the code
  compiles.
- `align` appears **0 times** in `doc/rasmol.hlp` (vs. `wireframe` 22×,
  `molecule` 140×). No user manual references it.

**Recommendation:** leave `align` alone unless SOMO specifically requires
structure superposition. Completing it is a research project (see last section).

---

## 1. What `align` is

Command token `AlignTok` (keyword `ALIGN`, `src/tokens.c`).

Grammar (from `src/command.c:6117`, `case(AlignTok)`):

```
align <n> [kabsch|local] [none|angle|distance] [+] [structure|substructure ["script"]] [centre|translate]
```

- `<n>` — 1-based index of the target molecule to align onto (must be a loaded
  molecule other than the current one). Requires ≥2 molecules loaded.
- Mode option groups (constants in `src/transfor.h:243-251`):
  - `kabsch` (`ALIGN_KABSCH=1`) / `local` (`ALIGN_LOCAL=2`) — final transform method.
  - `none` / `angle` / `distance` / `+` (`ALIGN_NONE=1`, `ALIGN_ANGLE=2`,
    `ALIGN_DISTANCE=3`, `ALIGN_ANGLE_SUM=4`, `ALIGN_DISTANCE_SUM=5`).
  - `structure` (`ALIGN_STRUCTURE=1`) / `substructure` (`ALIGN_SUBSTRUCTURE=2`).
- Defaults applied when unspecified (`command.c:6244-6246`):
  `kabsch_local = ALIGN_LOCAL`, `none_ang_dist = ALIGN_DISTANCE`,
  `findsubstructure = ALIGN_STRUCTURE`.
- `substructure` writes selection scripts and needs `set write true` (checks
  `AllowWrite`, `command.c:6202`).

---

## 2. Call chain

```
align <n>  command
  → command.c:6117  case(AlignTok)              (parse options)
  → command.c:6251  AlignToMolecule(...)         (single, unconditional endpoint)
      → transfor.c:5334  int AlignToMolecule(int MoleculeRemote, double *rmsd, ...)
          → transfor.c:5859  CQRHLERP(&qsum[0], &qsum[0], &q, count, 1.)   (always, in substructure loop)
          → transfor.c:5868  CQRHLERP(&localqtemp, localq, &q, localcount, 1.)  (only ALIGN_ANGLE / ALIGN_ANGLE_SUM)
          → transfor.c:5874  CQRHLERP(...)                                       (same, ANGLE modes)
```

`AlignToMolecule` builds an `AtomTree` (CNearTree) of each structure, finds
corresponding atom pairs within a distance band (`mindist=0.5`, `maxdist=7.5`,
`seqrange=5`, all hard-coded at `command.c:6142-6144`), accumulates per-pair
rotations as quaternions, averages them with `CQRHLERP`, and applies either a
Kabsch (SVD) or "local" transform.

---

## 3. The `CQRHLERP` problem

`CQRHLERP` is **not** in public CQRlib 1.0.3 (which exports no interpolation
functions at all — only Add/Subtract/Multiply/Divide/Conjugate/Inverse/
RotateByQuaternion/etc.), and is defined nowhere else in RasMol. RasMol 2.7.6
was clearly built against a **private/unreleased CQRlib** that added it. This is
one of several signs the 2.7.6 snapshot was a work-in-progress checkout.

**master does not resolve this** — `master:src/transfor.c` also only *calls*
`CQRHLERP`; it never defines it. So Bernstein's later work still relied on the
same private library.

### Our reconstruction (`src/transfor.c:255`)

Inferred from the call sites (`CQRHLERP(out, a, b, wa, wb)` used for incremental
quaternion averaging, always followed by a re-normalization):

```c
static int CQRHLERP( CQRQuaternionHandle out, CQRQuaternionHandle a,
                     CQRQuaternionHandle b, double wa, double wb )
{
    double dot, sgn;
    if( !out || !a || !b ) return -1;
    dot = a->w*b->w + a->x*b->x + a->y*b->y + a->z*b->z;
    sgn = ( dot < 0.0 ) ? -wb : wb;          /* double-cover hemisphere fix */
    out->w = wa*a->w + sgn*b->w;
    out->x = wa*a->x + sgn*b->x;
    out->y = wa*a->y + sgn*b->y;
    out->z = wa*a->z + sgn*b->z;
    return 0;
}
```

Interpretation: homogeneous linear combination `wa·a + wb·b` (a "homogeneous
lerp"), with a sign flip so quaternions in opposite hemispheres average
sensibly. This is the mathematically standard robust quaternion average. It
lets the code compile and is defensible, **but it is an inference, not
Bernstein's actual code** — the real one may use a different weighting, may not
do the hemisphere flip, or may be a true SLERP.

**This reconstruction is almost certainly NOT the root cause of the wrong
results** (see §4 — Kabsch mode fails equally and never calls it), but it must
still be validated if the feature is ever completed.

---

## 4. Observed behaviour (the bug)

Test: load the same file twice (identical coordinates → correct rmsd is 0),
then align. Run with `build/rasmol-text`:

```
load pdb "data/1crn.pdb"
load pdb "data/1crn.pdb"
align 1 <mode>
```

| command             | reported rmsd |
|---------------------|---------------|
| `align 1` (default) | 2.58922       |
| `align 1 kabsch`    | 2.58142       |
| `align 1 local`     | 2.58922       |
| `align 1 kabsch none` | 2.58142     |
| `align 1 local none`  | 2.58922     |

All ≈ 2.58 Å for identical structures (should be ≈ 0). Crucially, **Kabsch mode
also fails** — Kabsch is a closed-form SVD rigid-body superposition that does
not depend on `CQRHLERP`. So the defect is in the alignment machinery
(correspondence finding and/or the "field calculations" Bernstein flagged),
not merely in the reconstructed quaternion routine.

No crash — the command runs to completion (exit 0).

---

## 5. Evidence it was unfinished (Bernstein's own words)

`master` has 13+ commits touching the align area that our `2.7.6.0`-tag branch
lacks. Their messages (author HJB):

- `5b7bac6` — "Allow longer chain ids **Add align command**"
- `3d1f9c2` — "**Preliminary** changes for template alignment. **More to do.**"
- `2e6e4cd` — "Cleanup for solid kabsch substructure. **Needs patch in CNearTree.c**"
- `538a8be` — "Add code to write selection scripts for alignments"
- `b35704d` — "Correct some output sorting bugs on substructure align. Fix error in chain selection."
- `016c4df` — "Clean up substructure visual alignment to be non-destructive of the coordinate data. **Still need to revise the field calculations to agree.**"
- `e708160` — "clean up output, fix distance field"
- `a860473` — "Fix errors in display of alignment due to **conflicting mappings between Euler angles and quaternions**"
- `f9c7297` — "Make the **quaternion calculation for the Kabsch alignment consistent** with the local alignment and the display"
- `1a6e11c` — "Remove incorrect save of various name table limits from multiple.c to avoid problems with alignments against templates…"

The recurring themes — field calculations that don't agree, Euler↔quaternion
mapping conflicts, a needed CNearTree patch — describe a feature under active
debugging that was never finished.

---

## 6. `master` vs our branch

The two lines diverged (master = HJB's later feature work; the `2.7.6.0` tag we
forked = the modern-compiler/build-fix line). Align-relevant differences of
`master` over `somo-modernize`:

| file            | delta (master vs ours) |
|-----------------|------------------------|
| `src/transfor.c` (incl. `AlignToMolecule`) | 157 lines changed |
| `src/wbrotate.c` | 186 lines changed |
| `src/render.c`   | 18 lines changed |
| `src/multiple.c` | 4 lines removed (the "incorrect name table limits" fix) |

`master`'s `AlignToMolecule` **differs** from ours (it is HJB's later version),
but as noted it still calls an undefined `CQRHLERP`. So adopting master's align
code would still require the private CQRlib **and** may still be unfinished.

---

## 7. If you want to complete `align` — a research plan

Prerequisites / inputs to find first:
1. **Bernstein's private CQRlib** containing the real `CQRHLERP` — most likely on
   the user's older server, or in the unreleased **RasMol 2.8.x** the user
   worked on directly with Bernstein. This is the single authoritative source
   for the quaternion routine. (See the modernization memory / conversation for
   the 2.8.x lead.)
2. The **CNearTree patch** referenced in commit `2e6e4cd`.

Then, roughly:
3. Decide the base: cherry-pick `master`'s later `transfor.c` / `wbrotate.c` /
   `render.c` / `multiple.c` align work onto `somo-modernize`, *or* keep our
   base and port fixes selectively. Expect conflicts (our branch has the CMake/
   SDL/CIF modernization; master has none of it but has the align evolution).
4. Replace the reconstructed `CQRHLERP` (`transfor.c:255`) with the real one.
5. **Validation harness** (the key deliverable): align two identical structures
   → assert rmsd ≈ 0; align a structure against a rotated/translated copy of
   itself → assert it recovers the inverse transform (rmsd ≈ 0); align two
   genuinely homologous structures → compare rmsd against a reference tool
   (e.g. PyMOL `align`, or `TMalign`). The identical-structure rmsd≈0 test is
   the fastest go/no-go signal.
6. Work through the "field calculations to agree" issue HJB flagged — the
   Euler↔quaternion mapping and the per-pair distance/angle field math in
   `AlignToMolecule`.
7. Once correct, **document it** (add an `align` entry to `doc/rasmol.hlp`).

---

## 8. Key references

- **Command parse:** `src/command.c:6117` (`case(AlignTok)`), call at
  `src/command.c:6251`.
- **Core algorithm:** `src/transfor.c:5334` (`AlignToMolecule`), `CQRHLERP`
  calls at `transfor.c:5859,5868,5874`.
- **Reconstruction:** `src/transfor.c:244-267`.
- **Mode constants:** `src/transfor.h:243-251`.
- **Multi-molecule support:** `src/multiple.c` (`InitialiseMultiple`,
  `SwitchMolecule`, `StoreMoleculeData`, `Molecules[]`). The 2nd-load crash fix
  is commit `c7a51f0`.
- **Public CQRlib:** 1.0.3 (SourceForge `cqrlib`) — no interpolation functions.
  Vendored at `third_party/CQRlib/`.
- **NearTree:** used for correspondence search; RasMol 2.7.5.2 shipped 3.1, we
  vendor 5.1 at `third_party/NearTree/`. HJB mentioned a needed CNearTree patch.
- **Help file:** `doc/rasmol.hlp` — `align` appears 0 times.
- **Not reachable as a viewer feature:** only via the explicit, undocumented
  `align` command after loading multiple molecules.
