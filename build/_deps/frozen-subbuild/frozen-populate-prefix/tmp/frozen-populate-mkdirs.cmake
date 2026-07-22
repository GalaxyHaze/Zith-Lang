# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file LICENSE.rst or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION ${CMAKE_VERSION}) # this file comes with cmake

# If CMAKE_DISABLE_SOURCE_CHANGES is set to true and the source directory is an
# existing directory in our source tree, calling file(MAKE_DIRECTORY) on it
# would cause a fatal error, even though it would be a no-op.
if(NOT EXISTS "/home/diogo/Zith/build/_deps/frozen-src")
  file(MAKE_DIRECTORY "/home/diogo/Zith/build/_deps/frozen-src")
endif()
file(MAKE_DIRECTORY
  "/home/diogo/Zith/build/_deps/frozen-build"
  "/home/diogo/Zith/build/_deps/frozen-subbuild/frozen-populate-prefix"
  "/home/diogo/Zith/build/_deps/frozen-subbuild/frozen-populate-prefix/tmp"
  "/home/diogo/Zith/build/_deps/frozen-subbuild/frozen-populate-prefix/src/frozen-populate-stamp"
  "/home/diogo/Zith/build/_deps/frozen-subbuild/frozen-populate-prefix/src"
  "/home/diogo/Zith/build/_deps/frozen-subbuild/frozen-populate-prefix/src/frozen-populate-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/home/diogo/Zith/build/_deps/frozen-subbuild/frozen-populate-prefix/src/frozen-populate-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/home/diogo/Zith/build/_deps/frozen-subbuild/frozen-populate-prefix/src/frozen-populate-stamp${cfgdir}") # cfgdir has leading slash
endif()
