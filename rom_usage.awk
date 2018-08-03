# Pipe main.map into this to get ROM size info.

# https://stackoverflow.com/a/32437561/211234
function parse_hex(hex_string) {
    if (hex_string ~ /^0x/) {
        hex_string = substr(hex_string, 3)
    }

    value = 0
    for (i = 1; i <= length(hex_string); i++) {
        value = value*16 + H[substr(hex_string, i, 1)]
    }

    return value
}

BEGIN {
    # Build map from hex digit to value.
    for (i = 0; i < 16; i++) {
        H[sprintf("%x",i)] = i
        H[sprintf("%X",i)] = i
    }
}

/^CODE/ { code_start = parse_hex($2) }

/^RODATA/ { rodata_end = parse_hex($3) }

/^VECTORS/ { vectors_start = parse_hex($2) }

END {
    code = rodata_end - code_start + 1
    all = vectors_start - code_start
    printf "%d of %d ROM bytes (%d%%) used\n", code, all, code*100/all
}

