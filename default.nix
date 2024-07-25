let
  pkgs = (import
    (
      let
        lock = builtins.fromJSON (builtins.readFile ./flake.lock);
      in
      fetchTarball {
        url = "https://github.com/edolstra/flake-compat/archive/${lock.nodes.flake-compat.locked.rev}.tar.gz";
        sha256 = lock.nodes.flake-compat.locked.narHash;
      }
    )
    {
      src = ./.;
    }).defaultNix.packages.${builtins.currentSystem};
in
{
  inherit (pkgs) m8c-stable m8c-dev;
}
