#!/bin/bash -ex

builddir=build/efi-amd64
fsdir=${builddir}/testfs

mkdir -p ${fsdir}
mkdir -p ${fsdir}/efi/boot

cp ${builddir}/bin/kbootx64.efi ${fsdir}/efi/boot/bootx64.efi
cp ${builddir}/test/test-ia32.elf ${builddir}/test/test-amd64.elf ${fsdir}/

cat > ${fsdir}/efi/boot/kboot.cfg << EOF
set "timeout" 5

entry "Test (32-bit)" {
    kboot "/test-ia32.elf" ["/test-ia32.elf"]
}

entry "Test (64-bit)" {
    kboot "/test-amd64.elf" ["/test-amd64.elf"]
}
EOF

if [ ! -e ".ovmf-amd64.bin" ]; then
    cp test/qemu/efi/ovmf-amd64.bin .ovmf-amd64.bin
fi

qemu-system-x86_64 -pflash .ovmf-amd64.bin -drive file=fat:rw:${fsdir} -serial stdio -m 512 -monitor vc:1024x768 -s

rm -rf ${fsdir}
