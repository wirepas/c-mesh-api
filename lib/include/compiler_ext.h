// Compiler extensions used by Wirepas C Mesh API

#ifdef __GNUC__

// GNU C compiler extensions

#    define PACKED_STRUCT_START  // Not needed

#    define PACKED_STRUCT_END  // Not needed

#    define PACKED_STRUCT struct __attribute__((__packed__))

#elif defined(_MSC_EXTENSIONS)

// Microsoft Visual Studio C compiler extensions

#    define PACKED_STRUCT_START __pragma(pack(push, 1))

#    define PACKED_STRUCT_END __pragma(pack(pop))

#    define PACKED_STRUCT struct

#else

// No compiler extensions supported

#    define PACKED_STRUCT_START

#    define PACKED_STRUCT_END

#    define PACKED_STRUCT struct

#endif
