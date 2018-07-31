# Pipe main.map into this to get ROM size info.

/^CODE/ { code_start = ("0x" $2) + 0 }

/^RODATA/ { rodata_end = ("0x" $3) + 0 }

/^VECTORS/ { vectors_start = ("0x" $2) + 0 }

END {
    code = rodata_end - code_start + 1
    all = vectors_start - code_start
    printf "%d of %d ROM bytes (%d%%) used\n", code, all, code*100/all
}

