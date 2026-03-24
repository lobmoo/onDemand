/*
 * @Author       : wengjiqing wengjiqing@baosight.com
 * @Date         : 2025-07-01 13:38:51
 * @FilePath     : /TXDDS/include/txdds/RTPS/utils/InsertByOrder.h
 * Copyright (c) 2024 by BAOSIGHT, All Rights Reserved.
 * @Description  :
 */
#ifndef SRC_CPP_UTILS_COLLECTIONS_SORTED_VECTOR_INSERT_H_
#define SRC_CPP_UTILS_COLLECTIONS_SORTED_VECTOR_INSERT_H_

#include <algorithm>
#include <functional>

namespace BaoSky
{
    namespace rtps
    {
        template <typename CollectionType, typename ValueType, typename LessThanPredicate = std::less<ValueType>>
        void InsertByOrder(CollectionType &collection, const ValueType &item, const LessThanPredicate &pred = LessThanPredicate())
        {
            auto it = collection.end();
            if (!collection.empty() && pred(item, *collection.rbegin()))
            {
                it = std::lower_bound(collection.begin(), collection.end(), item, pred);
            }
            collection.insert(it, item);
        }
    }
}

#endif
