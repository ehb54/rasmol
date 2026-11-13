#!/usr/bin/env bash
#
# Headless tests for "load mesh": every format, the map commands on a mesh,
# where the mesh is drawn relative to the molecule, and "write script".
#
# Usage:   tests/mesh/run_tests.sh path/to/rasmol
#
# Needs bash and perl.  Works in $TEST_WORKDIR (default ./mesh-test-work),
# which is removed when every test passes and kept otherwise.
#
set -u

rasmol=${1:?usage: $0 path/to/rasmol}
case $rasmol in /*) ;; *) rasmol=$PWD/$rasmol ;; esac
here=$(cd "$(dirname "$0")" && pwd)
work=${TEST_WORKDIR:-$PWD/mesh-test-work}

rm -rf "$work"
mkdir -p "$work" && cd "$work" || exit 1
perl "$here/make_meshes.pl" || { echo "FAIL: make_meshes.pl"; exit 1; }

expect() { awk -v k="$1" '$1 == k { print $2 }' expected.txt; }
ntri=$(expect triangles)
nvert=$(expect vertices)
nedge=$(expect edges)
area=$(expect area)
volume=$(expect volume)

pass=0
fail=0
ok()  { pass=$((pass+1)); echo "ok    $1"; }
bad() { fail=$((fail+1)); echo "FAIL  $1"; }

# run NAME: RasMol reads NAME.cmd on stdin (so it reports, as typed
# commands do); its output goes to NAME.out.
run() {
    "$rasmol" -nodisplay < "$1.cmd" > "$1.out" 2>&1
}

# The first commands of most tests: the molecule, drawn as spheres.
mol() {
    printf 'load pdb mol.pdb\nwireframe off\nspacefill\n'
}

# The bounding box of the pixels that are not background, "x0 y0 x1 y1".
bbox() {
    perl -e '
        open( my $fh, "<", $ARGV[0] ) or die "$ARGV[0]: $!\n";
        binmode $fh;
        local $/;
        my $d = <$fh>;
        $d =~ s/\AP6\s+(\d+)\s+(\d+)\s+(\d+)\s//s or die "$ARGV[0]: not a PPM\n";
        my ( $w, $h ) = ( $1, $2 );
        my ( $x0, $y0, $x1, $y1 ) = ( $w, $h, -1, -1 );
        for my $y ( 0 .. $h - 1 ) {
            for my $x ( 0 .. $w - 1 ) {
                next if substr( $d, 3 * ( $y * $w + $x ), 3 ) eq "\0\0\0";
                $x0 = $x if $x < $x0;  $x1 = $x if $x > $x1;
                $y0 = $y if $y < $y0;  $y1 = $y if $y > $y1;
            }
        }
        die "$ARGV[0]: empty\n" if $x1 < 0;
        print "$x0 $y0 $x1 $y1\n";
    ' "$1"
}

# The middle of that box, "x y".
centre() {
    bbox "$1" | awk '{ printf "%.1f %.1f\n", ($1+$3)/2, ($2+$4)/2 }'
}

same_image() {   # NAME A B
    if cmp -s "$2" "$3"; then ok "$1"; else bad "$1 ($2 and $3 differ)"; fi
}

# ---------------------------------------------------------------------------
# 1. Every format: counts, closed, area and volume; and the picture.

for f in ico.best ico_rev.best ico.c3p ico.vert ico.face ico.bod ico_nm.bod; do
    n=fmt_${f//./_}
    { mol; echo "load mesh $f"; echo "map show"; echo "write ppm $n.ppm"; } > $n.cmd
    run $n
    if grep -q "mesh: *$ntri triangles, $nvert vertices, $nedge edges, closed" $n.out; then
        ok "$f: $ntri triangles, $nvert vertices, $nedge edges, closed"
    else bad "$f: counts (see $work/$n.out)"; fi
    if grep -q "mesh: *area $area A^2, volume $volume A^3" $n.out; then
        ok "$f: area $area, volume $volume"
    else bad "$f: area and volume (see $work/$n.out)"; fi
done

grep -q "the file states area 452.389, volume 904.779" fmt_ico_c3p.out \
    && ok "ico.c3p: area and volume from ico.c3v" || bad "ico.c3p: area and volume from ico.c3v"
grep -q "the file states area $area, volume $volume" fmt_ico_best.out \
    && ok "ico.best: area and volume from the first line" || bad "ico.best: area and volume from the first line"
grep -q "1 bodies other than triangles are not shown" fmt_ico_bod.out \
    && ok "ico.bod: the SPHERE is reported" || bad "ico.bod: the SPHERE is reported"

# Formats holding the same triangles draw the same picture: BEST and .bod
# (normals from the triangles, either winding, A or nm), and .c3p and MSMS
# (normals from the file).
same_image "BEST = BEST turned inside out" fmt_ico_best.ppm fmt_ico_rev_best.ppm
same_image "BEST = .bod"                   fmt_ico_best.ppm fmt_ico_bod.ppm
same_image ".bod in A = .bod in nm"        fmt_ico_bod.ppm  fmt_ico_nm_bod.ppm
same_image ".c3p = MSMS .vert"             fmt_ico_c3p.ppm  fmt_ico_vert.ppm
same_image "MSMS .vert = MSMS .face"       fmt_ico_vert.ppm fmt_ico_face.ppm

# ---------------------------------------------------------------------------
# 2. The mesh sits around the molecule: the middle of the drawing is the
#    same with and without it, seen from three sides.

views=( "" "rotate y 90" "rotate x 90" )
for i in 0 1 2; do
    { mol; echo "${views[$i]}"; echo "write ppm mol_$i.ppm"; } > mol_$i.cmd
    { mol; echo "load mesh ico.best"; echo "${views[$i]}"; echo "write ppm mesh_$i.ppm"; } > mesh_$i.cmd
    run mol_$i
    run mesh_$i
    read -r ax ay < <(centre mol_$i.ppm)
    read -r bx by < <(centre mesh_$i.ppm)
    if perl -e 'exit !( abs($ARGV[0]-$ARGV[2]) <= 1.5 && abs($ARGV[1]-$ARGV[3]) <= 1.5 )' \
            "$ax" "$ay" "$bx" "$by"; then
        ok "centred on the molecule, view $i ($bx,$by)"
    else bad "view $i: molecule centred at ($ax,$ay), mesh at ($bx,$by)"; fi
done
if cmp -s mol_0.ppm mesh_0.ppm; then bad "the mesh is not drawn"; else ok "the mesh is drawn"; fi

# ---------------------------------------------------------------------------
# 3. The map commands.

{ mol; echo "load mesh ico.best"; echo "map mesh"; echo "write ppm wire.ppm"
  echo "map mesh 0.2"; echo "write ppm thick.ppm"
  echo "map dots"; echo "write ppm dots.ppm"
  echo "map dots 0.3"; echo "write ppm balls.ppm"
  echo "map surface"; echo "map show"; echo "write ppm surface.ppm"
  echo "map colour atom"; echo "write ppm atom.ppm"
  echo "map zap"; echo "write ppm zap.ppm"; } > maps.cmd
run maps
if cmp -s thick.ppm wire.ppm; then bad "map mesh 0.2 draws thin lines"; else ok "map mesh 0.2 draws cylinders"; fi
if cmp -s balls.ppm dots.ppm; then bad "map dots 0.3 draws points"; else ok "map dots 0.3 draws spheres"; fi
same_image "map mesh, dots, surface: the surface comes back" fmt_ico_best.ppm surface.ppm
grep -q "mesh: *$ntri triangles, $nvert vertices, $nedge edges, closed" maps.out \
    && ok "map mesh, dots, surface: the triangles are kept" || bad "map mesh, dots, surface: the triangles are kept"
for p in wire dots atom; do
    if cmp -s $p.ppm surface.ppm; then bad "map $p draws the surface"; else ok "map $p draws something else"; fi
done
# (After map zap the molecule is drawn a little differently than before the
# map was made -- a generated map does the same -- so compare the extent
# of the drawing, which the mesh makes much larger.)
if perl -e '@a = split " ", $ARGV[0]; @b = split " ", $ARGV[1];
           for (0..3) { exit 1 if abs($a[$_]-$b[$_]) > 3 } exit 0' "$(bbox zap.ppm)" "$(bbox mol_0.ppm)"; then
    ok "map zap removes the mesh"
else bad "map zap removes the mesh: drawing $(bbox zap.ppm), molecule $(bbox mol_0.ppm)"; fi

{ mol; echo "slab 50"; echo "write ppm slab_mol.ppm"; } > slab_mol.cmd
{ mol; echo "load mesh ico.best"; echo "slab 50"; echo "write ppm slab_mesh.ppm"; } > slab_mesh.cmd
run slab_mol
run slab_mesh
if cmp -s slab_mol.ppm slab_mesh.ppm || cmp -s slab_mesh.ppm fmt_ico_best.ppm; then
    bad "slab: the cut mesh and the molecule inside it are not both drawn"
else ok "slab: the mesh is cut open"; fi

# ---------------------------------------------------------------------------
# 4. A mesh that is not closed, and files RasMol must refuse.

{ mol; echo "load mesh open.best"; echo "map show"; } > open.cmd
run open
grep -q "mesh: *$(expect open_triangles) triangles.*not closed ($(expect open_boundary) edges on one triangle, 0 on more than two)" open.out \
    && ok "open.best: not closed, $(expect open_boundary) edges on one triangle" || bad "open.best: not closed (see $work/open.out)"

{ echo "load mesh ico.best"; echo "map show"; } > nomol.cmd
run nomol
grep -q "^map [0-9]*:" nomol.out && bad "load mesh without a molecule is refused" \
    || ok "load mesh without a molecule is refused"

{ mol; echo "load mesh bad.best"; echo "load mesh missing.best"; echo "map show"; } > errors.cmd
run errors
grep -q "Mesh: '.*bad.best' line [0-9]*: expected $ntri triangles, read 10" errors.out \
    && ok "bad.best: reported as ending early" || bad "bad.best (see $work/errors.out)"
grep -q "Mesh: cannot open '.*missing.best'" errors.out \
    && ok "missing.best: reported" || bad "missing.best (see $work/errors.out)"
grep -q "^map [0-9]*:" errors.out && bad "a failed load leaves no map" || ok "a failed load leaves no map"

# ---------------------------------------------------------------------------
# 5. write script, then running that script, draws the same picture.

{ mol; echo "load mesh ico.c3p"; echo "map mesh"; echo "map colour [0,255,0]"
  echo "rotate x 30"; echo "write script saved.spt"; echo "write ppm before.ppm"; } > save.cmd
{ echo "script saved.spt"; echo "write ppm after.ppm"; } > restore.cmd
run save
run restore
grep -q '^load mesh "' saved.spt && ok "write script: load mesh is written" || bad "write script: no load mesh line"
same_image "write script: the script draws the same picture" before.ppm after.ppm

# ---------------------------------------------------------------------------

echo "$pass passed, $fail failed"
if [ $fail -eq 0 ]; then
    cd / && rm -rf "$work"
    exit 0
fi
echo "kept $work"
exit 1
