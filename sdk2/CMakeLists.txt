# The same reproducer on KasperskyOS Community Edition 2.0.0.179, QEMU (aarch64). SDK 2.0 builds no Einit-only image:
# a prebuilt MinimalKOS boots, programs are KPA packages started by ExecutionManager, and system programs a solution
# adds run from a product ROMFS. Here repro.Stress is the KPA package Stress, started at boot from
# system_services.json; VfsRamFs runs from the product ROMFS with the SDK's default environment and arguments, as in
# the 1.4 image (no workaround). Build and run with the SDK's cmake:
#   $SDK/toolchain/bin/cmake -S sdk2 -B build -D CMAKE_TOOLCHAIN_FILE=$SDK/toolchain/share/toolchain-aarch64-kos.cmake -D THREADS=4
#   $SDK/toolchain/bin/cmake --build build --target sim

cmake_minimum_required (VERSION 3.29)

project (vfsramfs_repro)

include (platform)
initialize_platform (FORCE_STATIC)
project_header_default ("STANDARD_GNU_17:YES" "EXTRA_STRICT_WARNINGS:YES")
include (platform/nk)
include (platform/options)
include (platform/kpa)

find_package (minimal_kos REQUIRED)
find_package (vfs REQUIRED)
find_package (precompiled_vfs REQUIRED)
include (${KL_SDK_ROOT_PATH}/share/vendor_rom/build_vendor_rom.cmake)

set (THREADS "1" CACHE STRING "Directories stressed at once (3 threads each)")

set (REPO ${CMAKE_SOURCE_DIR}/..)
set (NK_RESOURCES ${REPO}/resources/xdl)
nk_include_directories (${NK_RESOURCES})
nk_build_edl_files (stress_edl_files
                    NK_MODULE "repro"
                    EDL       "${NK_RESOURCES}/repro/Stress.edl")

add_executable (Stress "${REPO}/src/stress.c")
add_dependencies (Stress stress_edl_files)
target_compile_definitions (Stress PRIVATE DIRS=${THREADS})
target_link_libraries (Stress vfs::client)
set_target_properties (Stress PROPERTIES vfs::entity_REPLACEMENT "")

add_kpa_package (Stress.kpa ID Stress MANIFEST_V "2")
add_kpa_component (Stress.kpa RESOURCE Stress TYPE "bin")
# The client form that waits for the server, as the SDK examples use for programs started at boot.
add_kpa_component (Stress.kpa
                   RUN_CONFIGURATION "default"
                   TYPE              "application"
                   NAME              "Stress"
                   EIID              "repro.Stress"
                   PATH              "bin/Stress"
                   ENV               "VFS_FILESYSTEM_BACKEND=client:task:kl.VfsRamFs:timeout_dcm_ms:600000"
                   NO_AUTO_VFS_CONNECTION)

set (EINIT_SRC ${CMAKE_SOURCE_DIR}/einit)
set (ROOTFS_DIR ${CMAKE_BINARY_DIR}/rootfs_img)
set (PRODUCT_ROM_PROGRAMS precompiled_vfs::VfsRamFs)
find_program (TAR_COMMAND tar REQUIRED)

build_vendor_romfs (vendor_romfs
                    WITH_PRODUCT_ROMFS
                    SECURITY_PSL_FILE ${EINIT_SRC}/security.psl
                    EDL_TARGETS       stress_edl_files)
build_einit (product_einit ${EINIT_SRC}/product_init.yaml DEPENDS ${PRODUCT_ROM_PROGRAMS})
build_romfs (product_romfs product_einit FILES ${PRODUCT_ROM_PROGRAMS})
add_custom_target (rootfs_part_src
                   COMMAND ${CMAKE_COMMAND} -E make_directory ${ROOTFS_DIR}
                   COMMAND ${TAR_COMMAND} --extract --keep-directory-symlink
                                          --file ${MINIMAL_KOS_ROOTFS_ARCHIVE} --directory ${ROOTFS_DIR}
                   COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_PROPERTY:product_romfs,ROMFS_LOCATION>
                                                    ${ROOTFS_DIR}/product_romfs
                   COMMAND ${CMAKE_COMMAND} -E copy ${EINIT_SRC}/initial_install.json
                                                    ${EINIT_SRC}/system_services.json
                                                    ${ROOTFS_DIR}${MINIMAL_KOS_INITIAL_SETTINGS_DIR}/
                   COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_PROPERTY:Stress.kpa,KPA_OUTPUT_DIR>/Stress.kpa
                                                    ${ROOTFS_DIR}${MINIMAL_KOS_INITIAL_PACKAGES_DIR}
                   DEPENDS product_romfs Stress.kpa)

build_rootfs (rootfs DIR ${ROOTFS_DIR} DEPENDS rootfs_part_src)
generate_qemu_target (qemu
                      KERNEL_IMAGE ${MINIMAL_KOS_IMAGE}
                      VENDOR_IMAGE vendor_romfs
                      ROOTFS_IMAGE rootfs)
