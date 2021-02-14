#!/bin/bash -ex

builddir=build/dt-arm64

qemu-system-aarch64 -M virt -cpu cortex-a53 -serial stdio -m 512 -kernel "${builddir}/bin/kboot.bin"
