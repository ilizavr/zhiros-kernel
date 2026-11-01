load_mod(ramdisk_letter,"./mouse.mod"); // NEUROSLOP WARNING
load_mod(ramdisk_letter,"./clitools.mod");
load_mod(ramdisk_letter,"./cli_NS.mod"); // NEUROSLOP WARNING
load_mod(ramdisk_letter,"./mbr.mod");
load_mod(ramdisk_letter,"./bios_disk_driver.mod");
load_mod(ramdisk_letter,"./tmpfs.mod");

//graphics app
printf("select graphics mode(M - mac(NEUROSLOP), T - tinerkotl wm, C - console): ");
while(true){
    char mode = getchar();
    if(mode == 'M') {
        load_mod(ramdisk_letter,"./mac.mod"); // NEUROSLOP WARNING
        load_mod(ramdisk_letter,"./term.mod");
        load_mod(ramdisk_letter,"./img_view.mod");
        break;
    }
    if(mode == 'T') {
        load_mod(ramdisk_letter,"./win.mod");
        load_mod(ramdisk_letter,"./term.mod");
        load_mod(ramdisk_letter,"./img_view.mod");
        break;
    }
    if(mode == 'C') {
        break;
    }
}
printf("\n");
yield();

start_shell();
