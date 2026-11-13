#!/usr/bin/env perl
# make_meshes.pl -- test inputs for "load mesh".
#
# Writes, in the current directory, a 7-atom molecule (mol.pdb) and a sphere
# of radius 6 A around it, triangulated as a twice-subdivided icosahedron
# (162 vertices, 480 edges, 320 triangles), in every format RasMol reads:
#
#   ico.best          BEST
#   ico_rev.best      BEST, every triangle turned the other way
#   ico.c3p, ico.c3v  msroll
#   ico.vert/.face    MSMS
#   ico.bod           ZENO, UNITS A, plus one SPHERE body (not shown)
#   ico_nm.bod        ZENO, UNITS nm
#   open.best         the sphere with its top cap removed (not closed)
#   bad.best          a BEST file that ends early
#
# and expected.txt: the polyhedral area and volume, and the number of edges on
# only one triangle of open.best.
#
# The centre is not at the origin, and differs in x, y and z, so that a
# mistake in placing the mesh relative to the molecule shows.

use strict;
use warnings;

my @C = ( 10.0, -5.0, 3.0 );
my $R = 6.0;

# Icosahedron, faces turning anticlockwise seen from outside.
my $t = ( 1 + sqrt(5) ) / 2;
my @v = ( [-1, $t, 0], [1, $t, 0], [-1, -$t, 0], [1, -$t, 0],
          [0, -1, $t], [0, 1, $t], [0, -1, -$t], [0, 1, -$t],
          [$t, 0, -1], [$t, 0, 1], [-$t, 0, -1], [-$t, 0, 1] );
my @f = ( [0,11,5], [0,5,1], [0,1,7], [0,7,10], [0,10,11],
          [1,5,9], [5,11,4], [11,10,2], [10,7,6], [7,1,8],
          [3,9,4], [3,4,2], [3,2,6], [3,6,8], [3,8,9],
          [4,9,5], [2,4,11], [6,2,10], [8,6,7], [9,8,1] );

sub unit { my ($a) = @_; my $l = sqrt($a->[0]**2 + $a->[1]**2 + $a->[2]**2);
           return [ map { $_ / $l } @$a ]; }
@v = map { unit($_) } @v;

for my $level ( 1, 2 ) {
    my %mid;
    my @nf;
    my $midpoint = sub {
        my ( $a, $b ) = @_;
        my $key = $a < $b ? "$a,$b" : "$b,$a";
        return $mid{$key} if exists $mid{$key};
        push @v, unit( [ map { ( $v[$a][$_] + $v[$b][$_] ) / 2 } 0 .. 2 ] );
        return $mid{$key} = $#v;
    };
    for my $tri (@f) {
        my ( $a, $b, $c ) = @$tri;
        my $ab = $midpoint->( $a, $b );
        my $bc = $midpoint->( $b, $c );
        my $ca = $midpoint->( $c, $a );
        push @nf, [ $a, $ab, $ca ], [ $b, $bc, $ab ], [ $c, $ca, $bc ], [ $ab, $bc, $ca ];
    }
    @f = @nf;
}

# Coordinates as written (8 decimals), so that every format holds the same numbers.
my @x = map { my $u = $_; [ map { sprintf( "%.8f", $C[$_] + $R * $u->[$_] ) + 0 } 0 .. 2 ] } @v;
my @n = map { my $u = $_; [ map { sprintf( "%.6f", $u->[$_] ) + 0 } 0 .. 2 ] } @v;

sub cross { my ( $a, $b ) = @_;
            return [ $a->[1]*$b->[2] - $a->[2]*$b->[1],
                     $a->[2]*$b->[0] - $a->[0]*$b->[2],
                     $a->[0]*$b->[1] - $a->[1]*$b->[0] ]; }
sub sub3 { my ( $a, $b ) = @_; return [ map { $a->[$_] - $b->[$_] } 0 .. 2 ]; }
sub dot { my ( $a, $b ) = @_; return $a->[0]*$b->[0] + $a->[1]*$b->[1] + $a->[2]*$b->[2]; }

my ( $area, $volume ) = ( 0, 0 );
for my $tri (@f) {
    my ( $a, $b, $c ) = map { $x[$_] } @$tri;
    my $fn = cross( sub3( $b, $a ), sub3( $c, $a ) );
    $area += 0.5 * sqrt( dot( $fn, $fn ) );
    $volume += dot( $a, cross( $b, $c ) ) / 6;
}
die "faces turn inwards\n" if $volume <= 0;

# Edges, numbered from 1.
my ( %edge, @edges );
for my $tri (@f) {
    for my $k ( 0 .. 2 ) {
        my ( $a, $b ) = ( $tri->[$k], $tri->[ ( $k + 1 ) % 3 ] );
        my $key = $a < $b ? "$a,$b" : "$b,$a";
        next if $edge{$key};
        push @edges, [ $a, $b ];
        $edge{$key} = scalar @edges;
    }
}

sub out { my ($name) = @_; open( my $fh, '>', $name ) or die "$name: $!\n"; return $fh; }
sub xyz { my ($i) = @_; return join( ' ', map { sprintf( "%.8f", $_ ) } @{ $x[$i] } ); }

sub best {
    my ( $name, $faces, $rev ) = @_;
    my $fh = out($name);
    printf $fh "%d %.6f %.6f\n", scalar @$faces, $area, $volume;
    for my $tri (@$faces) {
        my @t = $rev ? ( $tri->[0], $tri->[2], $tri->[1] ) : @$tri;
        print $fh join( ' ', map { xyz($_) } @t ), "\n";
    }
    close $fh;
}

best( "ico.best", \@f, 0 );
best( "ico_rev.best", \@f, 1 );

{   # msroll .c3p and .c3v
    my $fh = out("ico.c3p");
    printf $fh "%d %d %d\n", scalar @v, scalar @edges, scalar @f;
    for my $i ( 0 .. $#v ) {
        printf $fh "%15.8f %15.8f %15.8f %9.6f %9.6f %9.6f   0.000000   0.000000   0.000000   1 %6d  1\n",
            @{ $x[$i] }, @{ $n[$i] }, 1 + $i % 7;
    }
    printf $fh "%7d %7d   1     0   0\n", $_->[0] + 1, $_->[1] + 1 for @edges;
    for my $tri (@f) {
        my @e = map { my ( $a, $b ) = ( $tri->[$_], $tri->[ ( $_ + 1 ) % 3 ] );
                      $edge{ $a < $b ? "$a,$b" : "$b,$a" } } 0 .. 2;
        printf $fh "%8d %8d %8d %7d %7d %7d   1     1   0\n", @e, map { $_ + 1 } @$tri;
    }
    close $fh;
    $fh = out("ico.c3v");
    print $fh "molecule name           = ico\n";
    printf $fh "number of atoms         = %10d\n", 7;
    printf $fh "probe radius            = %14.3f\n", 1.5;
    printf $fh "molecular area          = %14.3f\n", 4 * 3.14159265358979 * $R * $R;
    printf $fh "solvent-excluded volume = %14.3f\n", 4 / 3 * 3.14159265358979 * $R**3;
    close $fh;
}

{   # MSMS .vert and .face
    my $fh = out("ico.vert");
    print $fh "# MSMS solvent excluded surface vertices for ico\n#vertex #sphere density probe_r\n";
    printf $fh "%7d %7d %6.2f %5.2f\n", scalar @v, 7, 1.0, 1.5;
    printf $fh "%14.8f %14.8f %14.8f %10.6f %10.6f %10.6f %7d %7d %2d\n",
        @{ $x[$_] }, @{ $n[$_] }, 0, 1 + $_ % 7, 1 for 0 .. $#v;
    close $fh;
    $fh = out("ico.face");
    print $fh "# MSMS solvent excluded surface faces for ico\n#faces  #sphere density probe_r\n";
    printf $fh "%7d %7d %6.2f %5.2f\n", scalar @f, 7, 1.0, 1.5;
    printf $fh "%7d %7d %7d %2d %6d\n", ( map { $_ + 1 } @$_ ), 1, 0 for @f;
    close $fh;
}

{   # ZENO .bod, in A and in nm
    my $fh = out("ico.bod");
    print $fh "UNITS A\nTEMP 20 C\nVISCOSITY 1.002 cp\n";
    printf $fh "SPHERE %.8f %.8f %.8f 1.0\n", @C;
    print $fh "TRIANGLE ", join( ' ', map { xyz($_) } @$_ ), "\n" for @f;
    close $fh;
    $fh = out("ico_nm.bod");
    print $fh "UNITS nm\nTEMP 20 C\n";
    for my $tri (@f) {
        print $fh "TRIANGLE ", join( ' ', map { my $i = $_; map { sprintf( "%.9f", $_ / 10 ) } @{ $x[$i] } } @$tri ), "\n";
    }
    close $fh;
}

# The sphere without the triangles whose centre is in its top quarter.
my @open = grep { my $tri = $_; ( $x[ $tri->[0] ][2] + $x[ $tri->[1] ][2] + $x[ $tri->[2] ][2] ) / 3 < $C[2] + 0.5 * $R } @f;
my %count;
for my $tri (@open) {
    for my $k ( 0 .. 2 ) {
        my ( $a, $b ) = ( $tri->[$k], $tri->[ ( $k + 1 ) % 3 ] );
        $count{ $a < $b ? "$a,$b" : "$b,$a" }++;
    }
}
my $boundary = grep { $_ == 1 } values %count;
best( "open.best", \@open, 0 );

{   my $fh = out("bad.best");
    printf $fh "%d %.6f %.6f\n", scalar @f, $area, $volume;
    print $fh join( ' ', map { xyz($_) } @{ $f[$_] } ), "\n" for 0 .. 9;
    close $fh;
}

{   # Seven atoms around the centre, 2 A apart, of four elements, so that
    # "map colour atom" gives triangles whose corners differ in colour.
    my $fh = out("mol.pdb");
    my @d = ( [0,0,0], [2,0,0], [-2,0,0], [0,2,0], [0,-2,0], [0,0,2], [0,0,-2] );
    my @e = qw( C N N O O S S );
    for my $i ( 0 .. $#d ) {
        printf $fh "HETATM%5d  %-3s MSH A   1    %8.3f%8.3f%8.3f  1.00  0.00          %2s\n",
            $i + 1, $e[$i], ( map { $C[$_] + $d[$i][$_] } 0 .. 2 ), $e[$i];
    }
    print $fh "END\n";
    close $fh;
}

{   my $fh = out("expected.txt");
    printf $fh "triangles %d\nvertices %d\nedges %d\narea %.3f\nvolume %.3f\nopen_triangles %d\nopen_boundary %d\n",
        scalar @f, scalar @v, scalar @edges, $area, $volume, scalar @open, $boundary;
    close $fh;
}
