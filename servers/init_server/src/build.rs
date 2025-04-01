fn main() {
    println!(r"cargo:rustc-link-search=.");
    println!(r"cargo:rustc-link-search=../../lib/libc");
    println!(r"cargo:rustc-link-search=../../lib/libos");
    println!(r"cargo:rustc-link-search=../../lib");
    println!(r"cargo:rustc-link-search=/opt/cross-compiler/lib/gcc/i686-elf/15.0.0/");
    println!(r"cargo:rustc-link-lib=c");
    println!(r"cargo:rustc-link-lib=os_init");
    println!(r"cargo:rustc-link-lib=gcc");
}
