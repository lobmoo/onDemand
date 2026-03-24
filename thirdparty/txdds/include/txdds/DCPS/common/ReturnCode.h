/*
 * @Author      : wanglin wanglin@baosight.com
 * @Date        : 2025-02-14 16:51:26
 * @FilePath    : /TXDDS/include/DCPS/common/ReturnCode.h
 * Copyright (c) 2025 by BAOSIGHT, All Rights Reserved.
 * @Description :
 */
#ifndef TXDDS_DCPS_RETURNCODE_H
#define TXDDS_DCPS_RETURNCODE_H

#include <cstdint>

namespace BaoSky::dds
{
    typedef int32_t ReturnCode;

    const ReturnCode RETCODE_OK = 0;
    const ReturnCode RETCODE_ERROR = 1;
    const ReturnCode RETCODE_UNSUPPORTED = 2;
    const ReturnCode RETCODE_BAD_PARAMETER = 3;
    const ReturnCode RETCODE_PRECONDITION_NOT_MET = 4;
    const ReturnCode RETCODE_OUT_OF_RESOURCES = 5;
    const ReturnCode RETCODE_NOT_ENABLED = 6;
    const ReturnCode RETCODE_IMMUTABLE_POLICY = 7;
    const ReturnCode RETCODE_INCONSISTENT_POLICY = 8;
    const ReturnCode RETCODE_ALREADY_DELETED = 9;
    const ReturnCode RETCODE_TIMEOUT = 10;
    const ReturnCode RETCODE_NO_DATA = 11;
    const ReturnCode RETCODE_ILLEGAL_OPERATION = 12;
}

#endif