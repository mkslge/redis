#ifndef BYTEESCAPING_H
#define BYTEESCAPING_H

#include "Core/Bytes.h"

#include <string_view>

// Escapes bytes for display inside double quotes, as redis-cli does. The result
// never contains a newline, quote, or non-printable byte, and ArgumentSplitter
// decodes it back to the original bytes.
Bytes escape_bytes(std::string_view bytes);

#endif //BYTEESCAPING_H
