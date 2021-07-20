{ pkgs ? import <nixpkgs> {} }:

with pkgs;

let m8c-package =
  { stdenv
  , gnumake
  , SDL2
  , libserialport
  , fetchFromGitHub
  }:

  let
    pname = "m8c";
    version = "1.0.3";
  in
    stdenv.mkDerivation {
      inherit pname version;

      src = fetchFromGitHub {
        owner = "laamaa";
        repo = pname;
        rev = "v${version}";
        hash = "sha256:0yrd6lnb2chgafhw1cz4awx2s1sws6mch5irvgyddgnsa8ishcr5";
      };

      installFlags = [ "PREFIX=$(out)" ];
      nativeBuildInputs = [ gnumake ];
      buildInputs = [ SDL2 libserialport ];
    };
in {
  m8c-stable = pkgs.callPackage m8c-package {};
  m8c-dev = (pkgs.callPackage m8c-package {}).overrideAttrs (oldAttrs: {src = ./.;});
}
