{ pkgs ? import <nixpkgs> {} }:

with pkgs;

mkShell {
  packages = with pkgs; [ nix-prefetch-github ];
  inputsFrom = [ (import ./default.nix {}).m8c-dev ];
}
