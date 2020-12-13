cd $BUILDROOT_ROOT
make BR2_EXTERNAL=../linux-on-litex-vexriscv/buildroot/ litex_vexriscv_defconfig && make
cp output/images/Image $LINUX_ON_LITEX_ROOT/buildroot
cp output/images/rootfs.cpio $LINUX_ON_LITEX_ROOT/buildroot
