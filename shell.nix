{ pkgs ? import <nixpkgs> {} }:

with pkgs;

mkShell {
  inputsFrom = [ (import ./default.nix {}).m8c-dev ];
}
