#include "iqmfile.h"
#include <stdlib.h>
#include <string.h>

int iqm_read_header(struct iqm_file* iqm)
{
    if (memcmp(iqm->base, IQM_MAGIC, sizeof(IQM_MAGIC)) == 0) {
        memcpy(&iqm->header, iqm->base, sizeof(struct iqm_header));
        return 1;
    }
    return 0;
}

size_t iqm_va_fmt_size(int va_fmt)
{
    switch(va_fmt) {
        case IQM_BYTE:   return 1;
        case IQM_UBYTE:  return 1;
        case IQM_SHORT:  return 2;
        case IQM_USHORT: return 2;
        case IQM_INT:    return 4;
        case IQM_UINT:   return 4;
        case IQM_HALF:   return 4;
        case IQM_FLOAT:  return 4;
        case IQM_DOUBLE: return 8;
        default: return 0;
    }
}
