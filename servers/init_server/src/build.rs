fn main() {
    println!(r"cargo:rustc-link-search=.");
    println!(r"cargo:rustc-link-search=/media/cb_Silverbox-OS/lib/libc");
    println!(r"cargo:rustc-link-search=/media/cb_Silverbox-OS/lib/libos");
    println!(r"cargo:rustc-link-lib=c");
    println!(r"cargo:rustc-link-lib=os_init");
}
