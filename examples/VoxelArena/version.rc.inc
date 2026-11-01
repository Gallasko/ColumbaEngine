// What Windows shows in an exe's Properties > Details. Each exe's .rc defines VA_DESCRIPTION and VA_FILE.
// Bump the version with each release: both forms below have to say the same.

#define VA_PUBLISHER   "PigeonCodeur"
#define VA_PRODUCT     "Voxel Arena"
#define VA_COPYRIGHT   "Copyright (C) 2026 PigeonCodeur"
#define VA_VERSION     0,1,0,0
#define VA_VERSION_STR "0.1.0.0"

1 VERSIONINFO
FILEVERSION     VA_VERSION
PRODUCTVERSION  VA_VERSION
FILEFLAGSMASK   0x3fL
FILEFLAGS       0x0L
FILEOS          0x40004L
FILETYPE        0x1L
FILESUBTYPE     0x0L
BEGIN
    BLOCK "StringFileInfo"
    BEGIN
        BLOCK "040904B0"
        BEGIN
            VALUE "CompanyName",      VA_PUBLISHER
            VALUE "FileDescription",  VA_DESCRIPTION
            VALUE "FileVersion",      VA_VERSION_STR
            VALUE "InternalName",     VA_FILE
            VALUE "LegalCopyright",   VA_COPYRIGHT
            VALUE "OriginalFilename", VA_FILE
            VALUE "ProductName",      VA_PRODUCT
            VALUE "ProductVersion",   VA_VERSION_STR
        END
    END
    BLOCK "VarFileInfo"
    BEGIN
        VALUE "Translation", 0x409, 1200
    END
END
