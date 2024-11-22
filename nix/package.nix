{
  lib,
  stdenv,
  cmake,
  ninja,
  llvmPackages,
  curl,
  src,
  OpenAIAPIKey ? builtins.getEnv "OPENAI_API_KEY",
}:
stdenv.mkDerivation {
  pname = "aicc";
  version = "0-unstable-git+${src.shortRev or "dirty"}";

  inherit src;

  nativeBuildInputs = [
    cmake
    ninja
  ];

  buildInputs = [
    llvmPackages.llvm
    curl
  ];

  cmakeFlags = [
    (lib.cmakeFeature "OPENAI_API_KEY" OpenAIAPIKey)
  ];
}
