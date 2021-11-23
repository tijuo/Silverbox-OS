fn main() {
    println!(r"cargo:rustc-link-search=.");
    println!(r"cargo:rustc-link-search=../../lib/libc");
    println!(r"cargo:rustc-link-search=../../lib/libos");
    println!(r"cargo:rustc-link-lib=c");
    println!(r"cargo:rustc-link-lib=os_init");
}
