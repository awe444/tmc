/**
 * @file asm_stubs.c
 * @brief Legacy anchor after all asm trap stubs were ported.
 *
 * The original trap stubs in this file have been fully replaced by C
 * implementations in other translation units. We keep this tiny file so
 * references from docs / build notes to `Port_AsmStubCount()` remain valid.
 */
#include <stddef.h>

size_t Port_AsmStubCount(void);
size_t Port_AsmStubCount(void) {
    return 0;
}
