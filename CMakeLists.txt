# Reproducer: VfsRamFs faults (NULL dereference in inode_destructor) under concurrent stat/readdir/unlink.
# KasperskyOS Community Edition 1.4.0.102, QEMU (aarch64). One program, Stress, whose file system is the SDK's
# prebuilt VfsRamFs. Build and run with the SDK's cmake:
#   $SDK/toolchain/bin/cmake -B build -D CMAKE_TOOLCHAIN_FILE=$SDK/toolchain/share/toolchain-aarch64-kos.cmake
#   $SDK/toolchain/bin/cmake --build build --target sim
# THREADS (1-4, default 1) sets how many directories are stressed at once; each adds 3 threads (stat, readdir,
# unlink), so 1 keeps the process at 4 threads, within the documented 5 threads per VFS client.

cmake_minimum_required (VERSION 3.25)

project (vfsramfs_repro)

include (platform)
initialize_platform (FORCE_STATIC)
project_header_default ("STANDARD_GNU_17:YES" "EXTRA_STRICT_WARNINGS:YES")
include (platform/nk)
include (platform/image)

find_package (precompiled_vfs REQUIRED)
find_package (vfs REQUIRED)

set (THREADS "1" CACHE STRING "Directories stressed at once (3 threads each)")

set (NK_RESOURCES ${CMAKE_SOURCE_DIR}/resources/xdl)
nk_include_directories (${NK_RESOURCES})
nk_build_edl_files (stress_edl_files
                    NK_MODULE "repro"
                    EDL       "${NK_RESOURCES}/repro/Stress.edl")

add_executable (Stress "src/stress.c")
add_dependencies (Stress stress_edl_files)
target_compile_definitions (Stress PRIVATE DIRS=${THREADS})
target_link_libraries (Stress vfs::client)
# vfs::client would add the SDK's default VFS program; VfsRamFs serves Stress instead (init.yaml.in).
set_target_properties (Stress PROPERTIES vfs::entity_REPLACEMENT "")

build_kos_qemu_image (kos-qemu-image
                      EINIT_ENTITY EinitQemu
                      CONNECTIONS_CFG "einit/init.yaml.in"
                      SECURITY_PSL "einit/security.psl"
                      IMAGE_FILES Stress precompiled_vfs::VfsRamFs)
