#!/bin/bash -ex

platform=$(basename "$0" ".sh")
builddir="build/${platform}"

qemu-system-aarch64 -M virt -cpu cortex-a53 -serial stdio -m 512 -kernel "${builddir}/bin/kboot.bin"
