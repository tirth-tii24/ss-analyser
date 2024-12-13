# Copyright 2024 TII (SSRC) and the Ghaf contributors
# SPDX-License-Identifier: Apache-2.0

#with import <nixpkgs> {};
#overlay for ss-analyser
{
  config,
  lib,
  pkgs,
  ...
}: let
  ss-analyser-pkg = pkgs.stdenv.mkDerivation rec {
    pname = "ss-analyser";
    version = "v1.0";

    src = ./src;

    buildInputs = [ pkgs.pkg-config 
                    pkgs.pkgconf 
                  ];
    
    nativeBuildInputs = with pkgs; [
          pkg-config
    ];

    buildPhase = ''
                  echo "ss-analyser build started..." 
                  make  
                  echo "ss-analyser build completed..." 
                  #touch $out
                  '';
    installPhase = ''
      mkdir -p $out/bin
      
      cp ss-analyser $out/bin/

    '';
  }; in
  with lib; {
    config = {

      environment.systemPackages = with pkgs; [
          #ss analyser
          ss-analyser-pkg
      ];
    };
}
 
