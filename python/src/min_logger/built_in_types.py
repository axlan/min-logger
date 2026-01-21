def get_struct_format(c_type: str) -> str:
    """
    Get the Python struct format string for a C type.

    Args:
        c_type: C type name (e.g., 'int', 'unsigned long', 'float', 'uint64_t')

    Returns:
        Format string for use with Python's struct module

    Raises:
        ValueError: If c_type is not recognized or bits is invalid
    """
    # Normalize the type name
    c_type = c_type.strip()

    # Fixed-size integer types from stdint.h
    # NOTE: Fastest minimum-width integers (int_fast8_t - uint_fast64_t) require custom declarations
    stdint_types = {
        # Signed fixed-width integers
        "int8_t": "b",
        "int16_t": "h",
        "int32_t": "i",
        "int64_t": "q",
        # Unsigned fixed-width integers
        "uint8_t": "B",
        "uint16_t": "H",
        "uint32_t": "I",
        "uint64_t": "Q",
        # Minimum-width integers
        "int_least8_t": "b",
        "int_least16_t": "h",
        "int_least32_t": "i",
        "int_least64_t": "q",
        "uint_least8_t": "B",
        "uint_least16_t": "H",
        "uint_least32_t": "I",
        "uint_least64_t": "Q",
        # Max size integers
        "intmax_t": "q",
        "uintmax_t": "Q",
    }

    # Check stdint types first
    if c_type in stdint_types:
        return stdint_types[c_type]

    # Define format characters for traditional C types
    # These sizes are generally fixed, but if they are not correct (e.x. AVR double is 32bit), you can overwrite them with type_def json.
    fixed_size_types = {
        # Integer types - signed
        "char": "b",
        "signed char": "b",
        "short": "h",
        "short int": "h",
        "long long": "q",
        "long long int": "q",
        # Integer types - unsigned
        "unsigned char": "B",
        "unsigned short": "H",
        "unsigned short int": "H",
        "unsigned long long": "Q",
        "unsigned long long int": "Q",
        # Floating point types
        "float": "f",
        "double": "d",
        "long double": "d",  # Note: Python struct doesn't have true long double support
    }

    # Check fixed-size types
    if c_type in fixed_size_types:
        return fixed_size_types[c_type]

    # Architecture-dependent types.
    # This could be inferred by the memory models used (see https://www.ibm.com/docs/en/ent-metalc-zos/3.1.0?topic=options-lp64-ilp32), but this adds a strong possibility of getting the sizes wrong.
    # To use one of these types, define it in type_def json.

    # ILP32
    # arch_types = {
    #     "int": "i",
    #     "long": "l",
    #     "long int": "l",
    #     "unsigned int": "I",
    #     "unsigned long": "L",
    #     "unsigned long int": "L",
    #     "size_t": "I",
    #     "ssize_t": "i",
    #     "ptrdiff_t": "i",
    # }

    # LP64
    # arch_types = {
    #     "int": "i",
    #     "long": "q",  # long is 8 bytes on 64-bit (Linux/Mac)
    #     "long int": "q",
    #     "unsigned int": "I",
    #     "unsigned long": "Q",
    #     "unsigned long int": "Q",
    #     "size_t": "Q",
    #     "ssize_t": "q",
    #     "ptrdiff_t": "q",
    # }

    arch_types = {
        "int",
        "long",
        "long int",
        "unsigned int",
        "unsigned long",
        "unsigned long int",
        "size_t",
        "ssize_t",
        "ptrdiff_t",
    }

    if c_type in arch_types:
        raise ValueError(f"Architecture specific C/C++ type: {c_type}. Define it in type_def json.")

    raise ValueError(f"Unknown C/C++ type: {c_type}. Define it in type_def json.")
