#!/bin/bash -ex

platform=$(basename "$0" ".sh")
builddir="build/${platform}"
imagedir=${builddir}/image

mkdir ${imagedir}

cp ${builddir}/test/test-arm64.elf ${imagedir}/

cat > ${imagedir}/kboot.cfg << EOF
kboot "/test-arm64.elf" ["/test-arm64.elf"]
EOF

tar -cvf ${builddir}/initrd.tar -C ${imagedir} .
rm -rf ${imagedir}

qemu-system-aarch64 -M virt -cpu cortex-a53 -serial stdio -m 512 -kernel "${builddir}/bin/kboot.bin" -initrd "${builddir}/initrd.tar"
