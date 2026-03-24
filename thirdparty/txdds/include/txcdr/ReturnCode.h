/*
 * @Author       : yanli yanli563730@baosight.com
 * @Date         : 2025-02-17 13:19:52
 * @FilePath     : /txcdr/include/ReturnCode.h
 * @Copyright (c) 2025 by BAOSIGHT, All Rights Reserved.
 * @Description  :
 */
#ifndef TXCDR_RETURNCODE_H
#define TXCDR_RETURNCODE_H

#include <cstdint>

namespace BaoSky::Cdr
{
    typedef int32_t ReturnCode;

    const ReturnCode RETCODE_OK = 0;
    const ReturnCode RETCODE_ERROR = 1;
    const ReturnCode RETCODE_UNSUPPORTED = 2;
    const ReturnCode RETCODE_NOT_ENOUGH_MEMORY = 3;
    const ReturnCode RETCODE_BAD_PARAM = 4;
    const ReturnCode RETCODE_EXCEED_MAX_SIZE = 5;
}

#endif