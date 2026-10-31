load_mod(ramdisk_letter,"./mouse.mod"); // NEUROSLOP WARNING
load_mod(ramdisk_letter,"./clitools.mod");
load_mod(ramdisk_letter,"./cli_NS.mod"); // NEUROSLOP WARNING
load_mod(ramdisk_letter,"./mbr.mod");
load_mod(ramdisk_letter,"./bios_disk_driver.mod");
load_mod(ramdisk_letter,"./tmpfs.mod");

//graphics app
load_mod(ramdisk_letter,"./win.mod");
load_mod(ramdisk_letter,"./term.mod");
yield();

start_shell();
