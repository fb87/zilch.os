{
  description = "Zilch microkernel development shell";

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
        in {
          default = pkgs.mkShell {
            packages = [
              pkgs.coreutils
              pkgs.cmake
              pkgs.dtc
              pkgs.findutils
              pkgs.gawk
              pkgs.git
              pkgs.gnugrep
              pkgs.gnumake
              pkgs.gnused
              pkgs.gnutar
              pkgs.python3
              pkgs.qemu
              pkgs.ninja
              pkgs.python3Packages.west
              llvm.clang-unwrapped
              llvm.lld
              llvm.llvm
            ];

            shellHook = ''
              export LLVM=1
            '';
          };
        });
    };
}
