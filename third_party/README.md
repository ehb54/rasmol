# Vendored third-party libraries

RasMol's core depends on three small libraries by Herbert J. Bernstein and
Lawrence C. Andrews. They are vendored here so the build is self-contained.

| Dir       | Package        | Upstream                                      | License |
|-----------|----------------|-----------------------------------------------|---------|
| CVector   | CVector 1.0.3  | https://sourceforge.net/projects/cvector/     | LGPL    |
| NearTree  | NearTree 5.1   | https://sourceforge.net/projects/neartree/    | LGPL    |
| CQRlib    | CQRlib 1.0.3   | https://sourceforge.net/projects/cqrlib/      | LGPL    |

Note: RasMol 2.7.5.2 originally shipped NearTree 3.1; 5.1 is a later release
and its API is compatible with RasMol's usage as built here.
