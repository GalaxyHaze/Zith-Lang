# Audit reference formula for GalaxyHaze/homebrew-zithc.
#
# This file documents the release contract that update-package.yml should
# keep in sync with a live Homebrew tap. It is not a copy of an official
# tap formula; use it as the contract to compare against the tap.
#
# Homebrew does not currently offer a stable prebuilt zithc macOS asset, so
# the recommended formula builds from the tagged source archive. Build from
# source also matches local CMake expectations: zithc is a CMake project that
# can be configured with -DZITH_HAS_LLVM=OFF and ZITH_ENABLE_FFI=OFF.
#
# The stdlib is installed by Homebrew under share/zith/stdlib. findStdlibRoots
# discovers <binary_dir>/../share/zith/stdlib, which is exactly the layout
# produced by installing the compiler to Homebrew's prefix.
class Zithc < Formula
  desc "Zith programming language toolchain"
  homepage "https://github.com/GalaxyHaze/Zith"
  license "MIT"

  # Placeholder: REPLACE with the next tagged release and its verified archive
  # SHA-256. The archive URL and checksum are what update-package.yml sends.
  version "0.0.0"
  url "https://github.com/GalaxyHaze/Zith/archive/refs/tags/v0.0.0.tar.gz"
  sha256 "0000000000000000000000000000000000000000000000000000000000000000"

  depends_on "cmake" => :build
  depends_on "ninja" => :build
  depends_on "llvm" => :optional

  def install
    system "cmake", "-S", ".", "-B", "build", "-G", "Ninja",
           "-DCMAKE_BUILD_TYPE=Release",
           "-DZITH_HAS_LLVM=OFF",
           "-DZITH_ENABLE_FFI=OFF",
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
