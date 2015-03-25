#!/bin/bash -ex

builddir=build/efi-amd64
pxedir=${builddir}/pxe

mkdir ${pxedir}
mkdir -p ${pxedir}/efi/boot

cp ${builddir}/bin/kbootx64.efi ${pxedir}/efi/boot/bootx64.efi
cp ${builddir}/test/test-ia32.elf ${builddir}/test/test-amd64.elf ${pxedir}/

cat > ${pxedir}/kboot.cfg << EOF
set "timeout" 5

entry "Test (32-bit)" {
    kboot "test-ia32.elf" ["test-ia32.elf"]
}

entry "Test (64-bit)" {
    kboot "test-amd64.elf" ["test-amd64.elf"]
}
EOF

if [ ! -e ".ovmf-amd64-pxe.bin" ]; then
    cp test/qemu/efi/ovmf-amd64-pxe.bin .ovmf-amd64-pxe.bin
fi

qemu-system-x86_64 \
    -pflash .ovmf-amd64-pxe.bin -L test/qemu/efi \
    -tftp ${pxedir} -bootp efi/boot/bootx64.efi \
    -m 512 -serial stdio -monitor vc:1024x768 -s

rm -rf ${pxedir}
