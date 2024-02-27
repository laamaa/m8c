{ pkgs ? import <nixpkgs> {} }:

with pkgs;

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
#
let m8c-package =
  { stdenv
  , gnumake
  , pkg-config
  , SDL2
  , libserialport
  , fetchFromGitHub
  }:

  let
    pname = "m8c";
    version = "1.6.0";
  in
    stdenv.mkDerivation {
      inherit pname version;

      src = fetchFromGitHub {
        owner = "laamaa";
        repo = pname;
        rev = "v${version}";
        hash = "sha256:1sv1d419mgymh2xcrisnk26qqr77ldb3xp7k1qm3s6cvlil42n0n";
      };

      installFlags = [ "PREFIX=$(out)" ];
      nativeBuildInputs = [ gnumake pkg-config ];
      buildInputs = [ SDL2 libserialport ];
    };
in {
  m8c-stable = pkgs.callPackage m8c-package {};
  m8c-dev = (pkgs.callPackage m8c-package {}).overrideAttrs (oldAttrs: {src = ./.;});
}
