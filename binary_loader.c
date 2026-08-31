/* 8051 emulator core
 * Copyright 2006 Jari Komppa
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 * (i.e. the MIT License)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "emu8051.h"

#define EM8051_RAW_CODE_SIZE 65536u

int em8051_load_binary(struct em8051 *aCPU, const char *aFilename)
{
    FILE *input;
    unsigned char *image;
    size_t bytes_read;
    int trailing;
    int result = EM8051_LOAD_OK;

    if (!aFilename)
        return EM8051_LOAD_IO_ERROR;
    if (!aCPU || !aCPU->mCodeMem || aCPU->mCodeMemMaxIdx != 0xffffu)
        return EM8051_LOAD_CONFIGURATION_ERROR;

    input = fopen(aFilename, "rb");
    if (!input)
        return EM8051_LOAD_IO_ERROR;

    image = (unsigned char *)malloc(EM8051_RAW_CODE_SIZE);
    if (!image)
    {
        fclose(input);
        return EM8051_LOAD_IO_ERROR;
    }

    bytes_read = fread(image, 1, EM8051_RAW_CODE_SIZE, input);
    trailing = fgetc(input);
    if (ferror(input))
        result = EM8051_LOAD_IO_ERROR;
    else if (bytes_read != EM8051_RAW_CODE_SIZE || trailing != EOF)
        result = EM8051_LOAD_SIZE_ERROR;
    else
        memcpy(aCPU->mCodeMem, image, EM8051_RAW_CODE_SIZE);

    free(image);
    if (fclose(input) != 0 && result == EM8051_LOAD_OK)
        result = EM8051_LOAD_IO_ERROR;
    return result;
}
