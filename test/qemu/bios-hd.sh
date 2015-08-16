#!/bin/bash -ex

builddir=build/bios
imagedir=${builddir}/image
image=${imagedir}/hd.img

mkdir ${imagedir}

cat > ${imagedir}/kboot.cfg << EOF
set "timeout" 5

entry "Test (32-bit)" {
    kboot "/test-ia32.elf" ["/test-ia32.elf"]
}

entry "Test (64-bit)" {
    kboot "/test-amd64.elf" ["/test-amd64.elf"]
}
EOF

# Write a debugfs script to create the image.
cat > ${imagedir}/create.txt << EOF
mkdir boot
cd boot
write ${imagedir}/kboot.cfg kboot.cfg
write ${builddir}/bin/kboot.bin kboot.bin
cd /
write ${builddir}/test/test-amd64.elf test-amd64.elf
write ${builddir}/test/test-ia32.elf test-ia32.elf
EOF

# Create the image.
dd if=/dev/zero of=${image} bs=1048576 count=50
mke2fs -t ext2 -L Test -F ${image}
debugfs -w -f ${imagedir}/create.txt ${image}

# Write boot sector.
build/host/bin/kboot-install --target=bios --image=${image} --offset=0 --path=boot/kboot.bin --verbose

qemu-system-x86_64 -hda ${image} -serial stdio -vga std -m 512 -monitor vc:1024x768 -s
rm -rf ${imagedir}
