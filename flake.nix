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
      # HOWTO keep this package definition up-to-date:
      #
      # 1. NEW VERSION:
      # After the new version is tagged and pushed, update the `version` var to the
      # proper value and update the hash. You can use the following command to find
      # out the sha256, for example, with version 1.0.3:
      # `nix-prefetch-github --rev v1.0.3 laamaa m8c`
      #
      # 2. NEW DEPENDENCIES:
      # Make sure new dependencies are added. Runtime deps go to buildInputs and
      # compile-time deps go to nativeBuildInputs. Use the nixpkgs manual for help
      pname = "m8c";
      version = "1.7.10";
      m8c-package =
        { stdenv
        , gnumake
        , pkg-config
        , sdl3
        , libserialport
        , fetchFromGitHub
        }:
        stdenv.mkDerivation {
          inherit pname version;

          src = fetchFromGitHub {
            owner = "laamaa";
            repo = pname;
            rev = "v${version}";
            hash = "sha256:18bx6jf0jbgnd6cfydh4iknh25rrpyc8awma4a1hkia57fyjy2gi";
          };

          installFlags = [ "PREFIX=$(out)" ];
          nativeBuildInputs = [ gnumake pkg-config ];
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
        m8c-stable = pkgs.callPackage m8c-package { };
        m8c-dev = (pkgs.callPackage m8c-package { }).overrideAttrs (_oldAttrs: {
          src = ./.;
        });
        default = m8c-stable;
      });

      overlays.default = final: _prev: {
        inherit (self.packages.${final.system}) m8c-stable m8c-dev;
      };

      apps = eachSystem (pkgs: rec {
        m8c = {
          type = "app";
          program = "${self.packages.${pkgs.system}.m8c-stable}/bin/m8c";
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
          inputsFrom = [ self.packages.${system}.m8c-dev ];
        };
      });

      formatter = eachSystem (pkgs: treefmtEval.${pkgs.system}.config.build.wrapper);
    };
}
