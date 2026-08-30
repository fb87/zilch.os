{
  description = "Zilch Zephyr guest sample";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

  outputs = { self, nixpkgs }:
    let
      systems = [ "x86_64-linux" "aarch64-linux" ];
      forEachSystem = nixpkgs.lib.genAttrs systems;
    in {
      devShells = forEachSystem (system:
        let
          pkgs = import nixpkgs { inherit system; };
          llvm = pkgs.llvmPackages;
          python = pkgs.python3.withPackages (ps: [
            ps.kconfiglib
            ps.packaging
            ps.pyelftools
            ps.pykwalify
            ps.pyyaml
            ps.west
          ]);
        in {
          default = pkgs.mkShell {
            packages = [
              pkgs.cmake
              pkgs.dtc
              pkgs.git
              pkgs.gnumake
              pkgs.ninja
              python
              llvm.clang-unwrapped
              llvm.lld
              llvm.llvm
            ];
          };
        });
    };
}
