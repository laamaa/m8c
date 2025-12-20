{
  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixpkgs-unstable";
    systems.url = "github:nix-systems/default";
    treefmt-nix = {
      url = "github:numtide/treefmt-nix";
      inputs.nixpkgs.follows = "nixpkgs";
    };
    flake-compat = {
      url = "github:edolstra/flake-compat";
      flake = false;
    };
  };

  outputs = { self, nixpkgs, systems, treefmt-nix, ... }:
    let
      pname = "m8c";
      version = "2.2.1";
      m8c-package =
        { stdenv
        , cmake
        , copyDesktopItems
        , pkg-config
        , sdl3
        , libserialport
        }:
        stdenv.mkDerivation {
          inherit pname version;
          src = ./.;

          nativeBuildInputs = [ cmake copyDesktopItems pkg-config ];
          buildInputs = [ sdl3 libserialport ];
        };
      eachSystem = f: nixpkgs.lib.genAttrs (import systems) (system: f
        (import nixpkgs { inherit system; })
      );
      treefmtEval = eachSystem (pkgs: treefmt-nix.lib.evalModule pkgs (_: {
        projectRootFile = "flake.nix";
        programs = {
          clang-format.enable = false; # TODO(pope): Enable and format C code
          deadnix.enable = true;
          nixpkgs-fmt.enable = true;
          statix.enable = true;
        };
      }));
    in
    {
      packages = eachSystem (pkgs: rec {
        m8c = pkgs.callPackage m8c-package { };
        default = m8c;
      });

      overlays.default = final: _prev: {
        inherit (self.packages.${final.system}) m8c;
      };

      apps = eachSystem (pkgs: rec {
        m8c = {
          type = "app";
          program = "${self.packages.${pkgs.system}.m8c}/bin/m8c";
        };
        default = m8c;
      });

      devShells = eachSystem (pkgs: with pkgs; {
        default = mkShell {
          packages = [
            cmake
            gnumake
            nix-prefetch-github
            treefmtEval.${system}.config.build.wrapper
          ];
          inputsFrom = [ self.packages.${system}.m8c ];
        };
      });

      formatter = eachSystem (pkgs: treefmtEval.${pkgs.system}.config.build.wrapper);
    };
}
