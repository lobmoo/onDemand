/*
 * @Author       : yanli yanli563730@baosight.com
 * @Date         : 2025-11-25 13:07:54
 * @FilePath     : /TXDDS/thirdparty/txcdr/include/txcdr/optional.h
 * @Copyright (c) 2025 by BAOSIGHT, All Rights Reserved.
 * @Description  :
 */
#ifndef OPTIONAL_H
#define OPTIONAL_H


#include <new>
#include <utility>
#include <type_traits>

namespace BaoSky::Cdr
{

    template <class T, typename = void>
    struct OptionalStorage
    {
        union
        {
            char dummy;
            T val;
        };

        bool engaged{false};

        OptionalStorage()
        {
        }

        ~OptionalStorage()
        {
            if (engaged)
            {
                val.~T();
            }
        }
    };

    /* *INDENT-OFF* */
    template <class T>
    struct OptionalStorage<T, typename std::enable_if<std::is_trivially_destructible<T>{}>::type>
    {
        union
        {
            char dummy;
            T val;
        };

        bool engaged{false};

        OptionalStorage()
        {
        }

        ~OptionalStorage() = default;
    };
    //! An empty class type used to indicate optional type with uninitialized state.
    struct nullopt_t
    {
        constexpr explicit nullopt_t(
            int)
        {
        }
    };

    /*!
     * @brief nullopt is a constant of type nullopt_t that is used to indicate optional type with uninitialized state.
     */
    static constexpr nullopt_t nullopt{0};

    /*!
     * @brief This class template manages an optional contained value, i.e. a value that may or may not be present.
     */
    template <class T>
    class optional
    {
    public:
        using type = T;

        //! Default constructor
        optional() = default;

        //! Copy constructor from an instance of the templated class.
        optional(
            const T &val) noexcept
        {
            ::new (&storage.val) T(val);
            storage.engaged = true;
        }

        //! Move constructor from an instance of the templated class.
        optional(
            T &&val) noexcept
        {
            ::new (&storage.val) T(std::move(val));
            storage.engaged = true;
        }

        //! Copy constructor.
        optional(
            const optional<T> &val) noexcept
        {
            ::new (&storage.val) T(val.storage.val);
            storage.engaged = val.storage.engaged;
        }

        //! Move constructor.
        optional(
            optional<T> &&val) noexcept
        {
            ::new (&storage.val) T(std::move(val.storage.val));
            storage.engaged = val.storage.engaged;
        }

        //! Destructor
        ~optional() = default;

        /*!
         * @brief Constructs the contained value in-place
         *
         * @param[in] _args The arguments to pass to the constructor.
         */
        template <class... Args>
        void emplace(
            Args &&..._args)
        {
            reset();
            storage.val.T(std::forward<Args>(_args)...);
            storage.engaged = true;
        }

        /*!
         * @brief Reset the state of the optional
         *
         * @param[in] initial_engaged True value initializes the state with a default instance of the templated class.
         * False value leaves the optional in a uninitialized state.
         */
        void reset(
            bool initial_engaged = false)
        {
            if (storage.engaged)
            {
                storage.val.~T();
            }
            storage.engaged = initial_engaged;
            if (storage.engaged)
            {
                ::new (&storage.val) T();
            }
        }

        /*!
         * @brief Returns the contained value.
         *
         * @return The contained value.
         * @exception exception::BadOptionalAccessException This exception is thrown when the optional is uninitialized.
         */
        T &value() &
        {
            if (!storage.engaged)
            {
                // throw exception::BadOptionalAccessException(
                //     exception::BadOptionalAccessException::BAD_OPTIONAL_ACCESS_MESSAGE_DEFAULT);
            }

            return storage.val;
        }

        /*!
         * @brief Returns the contained value.
         *
         * @return The contained value.
         * @exception exception::BadOptionalAccessException This exception is thrown when the optional is uninitialized.
         */
        const T &value() const &
        {
            if (!storage.engaged)
            {
                // throw exception::BadOptionalAccessException(
                //     exception::BadOptionalAccessException::BAD_OPTIONAL_ACCESS_MESSAGE_DEFAULT);
            }

            return storage.val;
        }

        T &&value() &&
        {
            if (!storage.engaged)
            {
                // throw exception::BadOptionalAccessException(
                //     exception::BadOptionalAccessException::BAD_OPTIONAL_ACCESS_MESSAGE_DEFAULT);
            }

            return std::move(storage.val);
        }

        const T &&value() const &&
        {
            if (!storage.engaged)
            {
                // throw exception::BadOptionalAccessException(
                //     exception::BadOptionalAccessException::BAD_OPTIONAL_ACCESS_MESSAGE_DEFAULT);
            }

            return std::move(storage.val);
        }

        bool has_value() const
        {
            return storage.engaged;
        }

        optional &operator=(
            const optional &opt)
        {
            reset();
            storage.engaged = opt.storage.engaged;
            if (opt.storage.engaged)
            {
                ::new (&storage.val) T(opt.storage.val);
            }
            return *this;
        }

        optional &operator=(
            optional &&opt)
        {
            reset();
            storage.engaged = opt.storage.engaged;
            if (opt.storage.engaged)
            {
                ::new (&storage.val) T(std::move(opt.storage.val));
            }
            return *this;
        }

        optional &operator=(
            const T &val)
        {
            reset();
            ::new (&storage.val) T(val);
            storage.engaged = true;
            return *this;
        }

        optional &operator=(
            T &&val)
        {
            reset();
            ::new (&storage.val) T(std::move(val));
            storage.engaged = true;
            return *this;
        }

        optional &operator=(
            nullopt_t) noexcept
        {
            reset();
            return *this;
        }

        bool operator==(
            const optional &opt_val) const
        {
            return opt_val.storage.engaged == storage.engaged &&
                   (storage.engaged ? opt_val.storage.val == storage.val : true);
        }

        bool operator!=(
            const optional &opt_val) const
        {
            return !operator==(opt_val);
        }

        T &operator*() & noexcept
        {
            return storage.val;
        }

        const T &operator*() const & noexcept
        {
            return storage.val;
        }

        T &&operator*() && noexcept
        {
            return std::move(storage.val);
        }

        const T &&operator*() const && noexcept
        {
            return std::move(storage.val);
        }

        T *operator->() noexcept
        {
            return std::addressof(storage.val);
        }

        const T *operator->() const noexcept
        {
            return std::addressof(storage.val);
        }

        //! Checks whether the optional contains a value.
        explicit operator bool() const noexcept
        {
            return storage.engaged;
        }

    private:
        OptionalStorage<T> storage;
    };

}
#endif
