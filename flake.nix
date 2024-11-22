{
  description = "C compiler, but compiled and optimized by AI";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs";
    systems.url = "github:nix-systems/default-linux";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = {
    self,
    nixpkgs,
    systems,
    flake-utils,
  }:
    let
      inherit (nixpkgs) lib;
    in flake-utils.lib.eachSystem (import systems) (system:
      let
        pkgs = nixpkgs.legacyPackages.${system};
        pkgsLLVM = import nixpkgs {
          localSystem = {
            inherit system;
          };

          crossSystem = {
            inherit system;
            useLLVM = true;
            linker = "lld";
          };
        };
      in {
        packages = {
          default = pkgs.callPackage ./nix/package.nix {
            src = self;
          };

          llvm = pkgsLLVM.callPackage ./nix/package.nix {
            src = self;
          };
        };

        legacyPackages = pkgs;

        devShells = {
          default = pkgs.callPackage ./nix/package.nix {
            src = self;
          };

          llvm = pkgsLLVM.callPackage ./nix/package.nix {
            src = self;
          };
        };
      });
}
