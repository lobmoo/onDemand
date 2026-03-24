/*
 * @Author       : tangfuqi tangfuqi@baosight.com
 * @Date         : 2025-04-28 15:18:55
 * @FilePath     : /TXDDS/include/DCPS/dynamicType/TypeInformation.h
 * @Copyright (c) 2023 by BAOSIGHT, All Rights Reserved.
 * @Description  :
 */
#pragma once
#include "TypeIdentifier.h"
namespace BaoSky::dds
{

    class TypeIdentifierWithSize
    {
    private:
        TypeIdentifier m_type_id;
        uint32_t m_typeobject_serialized_size;
    };
    typedef std::vector<TypeIdentifierWithSize> TypeIdentifierWithSizeSeq;
    class TypeIdentifierWithDependencies
    {

    private:
        TypeIdentifierWithSize m_typeid_with_size;
        int32_t m_dependent_typeid_count;
        TypeIdentifierWithSizeSeq m_dependent_typeids;
    };

    class TypeInformation
    {
    public:
        TypeIdentifierWithDependencies m_minimal;
        TypeIdentifierWithDependencies m_complete;
    };

}
