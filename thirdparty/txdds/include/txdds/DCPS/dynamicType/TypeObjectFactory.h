/*
 * @Author       : tangfuqi tangfuqi@baosight.com
 * @Date         : 2025-05-13 10:46:05
 * @FilePath     : /TXDDS/include/txdds/DCPS/dynamicType/TypeObjectFactory.h
 * @Copyright (c) 2023 by BAOSIGHT, All Rights Reserved.
 * @Description  :
 */
#pragma once
#include "txdds/DCPS/dynamicType/TypeObject.h"
#include <map>
#include <string>
#include <mutex>
namespace BaoSky::dds
{

    class TypeInformation;
    class DynamicType;
    class TypeDescriptor;
    std::string GetKindName(TypeKind kind);
    class TypeObjectFactory
    {
    public:
        TypeObjectFactory();
        mutable std::map<std::string, TypeIdentifier *> identifiers_;  // Basic, builtin and EquivalenceKind_MINIMAL
        std::map<std::string, TypeIdentifier *> complete_identifiers_; // Only EquivalenceKind_COMPLETE
        std::map<TypeIdentifier *, TypeObject *> objects_;             // EquivalenceKind_MINIMAL
        std::map<TypeIdentifier *, TypeObject *> complete_objects_;    // EquivalenceKind_COMPLETE
        mutable std::vector<TypeIdentifier *> identifiers_created_;
        mutable std::map<TypeIdentifier *, TypeInformation *> informations_;
        mutable std::vector<TypeInformation *> informations_created_;
        virtual ~TypeObjectFactory() = default;
        static std::mutex sMutex;
        std::mutex mMutex;

    public:
        TypeKind GetTypeKindFromIdentifier(const TypeIdentifier *identifier);
        static TypeObjectFactory *get_instance()
        {
            std::unique_lock<std::mutex> lck(sMutex);
            if (sInstance == nullptr)
            {
                sInstance = new TypeObjectFactory();
            }
            return sInstance;
        }
        TypeIdentifier *get_type_identifier(const std::string &type_name, bool generator = true);
        void add_alias(std::string name, std::string alisanme)
        {
            (void)name;
            (void)alisanme;
            return;
        }
        void add_type_object(const std::string &type_name, const TypeIdentifier *identifier, const TypeObject *object);

        TypeObject *get_type_object(std::string type_name, bool generator);

        const TypeObject *get_type_object(
            const TypeIdentifier *identifier);

        // DynamicType BuildDynamicType(
        //     const std::string &name,
        //     const TypeIdentifier *identifier,
        //     const TypeObject *object);
        static inline TypeObjectFactory *sInstance{};
        const TypeIdentifier *get_stored_type_identifier(
            const TypeIdentifier *identifier);
        std::string get_type_name(
            const TypeIdentifier *identifier) const;
        std::shared_ptr<BaoSky::dds::DynamicType> BuildDynamicType(const std::string &name, const TypeIdentifier *identifier, const TypeObject *object);

        std::shared_ptr<BaoSky::dds::DynamicType> BuildDynamicType(TypeDescriptor &descriptor, const TypeObject *object, const DynamicType *annotation_member_type);
        TypeIdentifier *CreateComplexType(const TypeIdentifier *identifier);
        TypeIdentifier *get_array_identifier(const std::string &type_name, const std::vector<uint32_t> &bound);

        inline bool add_type_identifier(const std::string &type_name, const TypeIdentifier *identifier)
        {
            std::unique_lock<std::mutex> lck(mMutex);
            if (identifier->m__d == EquivalenceKind_COMPLETE)
            {
                if (!complete_identifiers_.count(type_name))
                {
                    TypeIdentifier *id = new TypeIdentifier();
                    identifiers_created_.push_back(id);
                    *id = *identifier;
                    complete_identifiers_[type_name] = id;
                }
            }
            else
            {
                if (!identifiers_.count(type_name))
                {
                    TypeIdentifier *id = new TypeIdentifier();
                    identifiers_created_.push_back(id);
                    *id = *identifier;
                    identifiers_[type_name] = id;
                }
            }
            return true;
        }

        std::string get_array_type_name(const std::string &type_name, const std::vector<uint32_t> &bound, uint32_t &ret_size);

        TypeIdentifier *get_sequence_identifier(const std::string &type_name, uint32_t bound);

        std::string get_sequence_type_name(const std::string &type_name, uint32_t bound);

        TypeIdentifier *get_map_identifier(const std::string &key_type_name, const std::string &value_type_name, uint32_t bound);

        std::string get_map_type_name(const std::string &key_type_name, const std::string &value_type_name, uint32_t bound);
        std::string get_string_type_name(uint32_t bound, bool wide);

        TypeIdentifier *get_string_identifier(uint32_t bound, bool wide);
    };

}