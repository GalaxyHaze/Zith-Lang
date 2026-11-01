# Audit reference formula for GalaxyHaze/homebrew-zithc.
#
# This file documents the release contract that build-artifact.yml should
# keep in sync with a live Homebrew tap. It is not a copy of an official
# tap formula; use it as the contract to compare against the tap.
#
# Homebrew does not currently offer a stable prebuilt zithc macOS asset, so
# the recommended formula builds from the tagged source archive. Build from
# source also matches local CMake expectations and requires the LLVM codegen
# backend.
#
# The stdlib is installed by Homebrew under share/zith/stdlib. findStdlibRoots
# discovers <binary_dir>/../share/zith/stdlib, which is exactly the layout
# produced by installing the compiler to Homebrew's prefix.
class Zithc < Formula
  desc "Zith programming language toolchain"
  homepage "https://github.com/GalaxyHaze/Zith-Lang"
  license "MIT"

  # build-artifact.yml updates the tagged archive URL and verified SHA-256.
  version "0.6.4.2"
  url "https://github.com/GalaxyHaze/Zith-Lang/archive/4d69a732527aa5dfd0e71f4ea68e3de7c773ebda.tar.gz"
  sha256 "8b17253ddeb93168db48616edb079d4ca694f1be5c39e99e621446fda0bcf3f4"

  depends_on "cmake" => :build
  depends_on "ninja" => :build
  depends_on "llvm"

  def install
    llvm_dir = Formula["llvm"].opt_lib/"cmake"/"llvm"
    system "cmake", "-S", ".", "-B", "build", "-G", "Ninja",
           "-DCMAKE_BUILD_TYPE=Release",
           "-DZITH_HAS_LLVM=ON",
           "-DZITH_REQUIRE_LLVM=ON",
           "-DZITH_ENABLE_C_COMPILE=OFF",
           "-DLLVM_DIR=#{llvm_dir}",
           "-DCMAKE_INSTALL_PREFIX=#{prefix}"
    system "cmake", "--build", "build"
    system "cmake", "--install", "build"
  end

  test do
    assert_predicate bin/"zithc", :exist?
    assert_predicate share/"zith/stdlib", :directory?
    assert_match(/#{version}/, shell_output("#{bin}/zithc info"))
  end
end
