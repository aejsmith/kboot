#!/bin/bash -ex

builddir=build/efi-amd64
fsdir=${builddir}/testfs

mkdir ${fsdir}
mkdir -p ${fsdir}/efi/boot

cp ${builddir}/bin/bootx64.efi ${fsdir}/efi/boot/
cp ${builddir}/test/test-ia32.elf ${builddir}/test/test-amd64.elf ${fsdir}/
cp ../shellx64.efi ${fsdir}/

cat > ${fsdir}/kboot.cfg << EOF
set "timeout" 5

entry "Test (32-bit)" {
    kboot "test-ia32.elf" ["test-ia32.elf"]
}

entry "Test (64-bit)" {
    kboot "test-amd64.elf" ["test-amd64.elf"]
}

entry "EFI Shell" {
    efi "shellx64.efi"
}
EOF

if [ ! -e ".ovmf-x86_64.bin" ]; then
    cp utilities/test/efi/ovmf-x86_64.bin .ovmf-x86_64.bin
fi

qemu-system-x86_64 -pflash .ovmf-x86_64.bin -hda fat:${fsdir} -serial stdio -m 512 -monitor vc:1024x768 -s

rm -rf ${fsdir}
