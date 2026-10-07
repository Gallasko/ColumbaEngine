# Writes the published page from its template when the game is packed (make ChronicleWeb):
#
#   cmake -DTEMPLATE=<index.html.in> -DOUT=<index.html> -DFILES=<folder of the three built files>
#         -DNEEDS_THREADS=<true|false> -P packpage.cmake
#
# The page asks for Chronicle.js, Chronicle.wasm and Chronicle.data under a stamp made from the
# three of them. A host may let a browser keep one of them for hours and not the others: without
# the stamp a new .wasm meets the .js of the build before, and they do not link.

set(STAMP_SOURCE "")

foreach(BUILT Chronicle.js Chronicle.wasm Chronicle.data)
    file(MD5 "${FILES}/${BUILT}" BUILT_HASH)
    string(APPEND STAMP_SOURCE "${BUILT_HASH}")
endforeach()

string(MD5 STAMP "${STAMP_SOURCE}")
string(SUBSTRING "${STAMP}" 0 12 CHRONICLE_WEB_STAMP)

set(CHRONICLE_WEB_NEEDS_THREADS "${NEEDS_THREADS}")

configure_file("${TEMPLATE}" "${OUT}" @ONLY)
