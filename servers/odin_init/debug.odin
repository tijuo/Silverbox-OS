package init_server

import "core:io"
import core_fmt "core:fmt"

USE_SOFT_FLOAT :: false

DEBUG_PORT : u16 : 0xE9

foreign import llio "io.asm"

foreign llio {
    outport8 :: proc(port: u16, data: u8) ---
    outport16 :: proc(port: u16, data: u16) ---
    outport32 :: proc(port: u16, data: u32) ---
    inport8 :: proc(port: u16) -> u8 ---
    inport16 :: proc(port: u16) -> u16 ---
    inport32 :: proc(port: u16) -> u32 ---
}

putc :: #force_inline proc "contextless" (c: byte) {
    outport8(DEBUG_PORT, c)
}

print_str :: proc "contextless" (args: ..string, sep: string = " ") -> (n: int) {
    use_sep := false

    for s in args {
        if use_sep {
            for i := 0; i < len(sep); i+= 1 {
                putc(sep[i])
                n += 1
            }
        }

        for i := 0; i < len(s); i+= 1 {
            putc(s[i])
            n += 1
        }

        use_sep = true
    }

    return
}

eprint_streamer :: proc(stream_data: rawptr, mode: io.Stream_Mode, p: []byte, offset: i64, whence: io.Seek_From) -> (n: i64, err: io.Error) {
    #partial switch mode {
        case .Write, .Write_At:
            n = 0

            for c in p {
                outport8(0xE9, c)
                n += 1
            }
        case .Flush:
            n = 0
        case .Query:
            set : io.Stream_Mode_Set = { .Write, .Write_At, .Flush, .Query }
            n = transmute(i64)set
        case:
            err = .EOF
    }

    return
}

kwriter := io.Writer {
    procedure = eprint_streamer,
    data = nil
}

eprintf :: proc(fmt: string, args: ..any, flush := true, newline := false) -> int {
    if len(args) == 0 {
        return core_fmt.wprint(kwriter, fmt, newline ? "\n" : "", sep="", flush = flush)
    } else {
        return core_fmt.wprintf(kwriter, fmt, ..args, flush = flush, newline = newline)
    }
}

eprintfln :: proc(fmt: string, args: ..any, flush := true) -> int {
    if len(args) == 0 {
        return core_fmt.wprintln(kwriter, fmt, flush = flush)
    } else {
        return core_fmt.wprintfln(kwriter, fmt, ..args, flush=flush)
    }
}