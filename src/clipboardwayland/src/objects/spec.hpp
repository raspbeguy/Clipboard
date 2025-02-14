/*  Clipboard - Cut, copy, and paste anything, anywhere, all from the terminal.
    Copyright (C) 2023 Jackson Huff and other contributors on GitHub.com
    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.*/
#pragma once

#include <memory>
#include <concepts>
#include <functional>
#include <wayland-client.h>

#include "../exception.hpp"

/**
 * Base concept for all Wayland object specifications.
 * The specification must have an obj_t type with the Wayland object type,
 * a static interface member pointing to the Wayland interface for the object,
 * and a version member with the Wayland interface version supported by the program.
 */
template<typename T>
concept WlObjectSpec = requires(T::obj_t* obj) {
    typename T::obj_t;
    reinterpret_cast<wl_proxy*>(obj);
    { T::interface } -> std::same_as<wl_interface const&>;
    { T::version } -> std::same_as<std::uint32_t const&>;
};

/**
 * Concept for Wayland object specifications that have a custom deleter.
 * The specification must have static function called deleteObj that accepts a pointer to
 * the wayland object.
 */
template<typename T>
concept WlObjectSpecCustomDeleter = WlObjectSpec<T> && requires(T::obj_t* obj) {
    T::deleter;
    T::deleter(obj);
};

/**
 * Concept for Wayland object specifications that emit events and therefore need a
 * registered listener.
 * The specification must have a type listener_t with the Wayland listener type and
 * a static function listener() that returns the listener for the object.
 */
template<typename T>
concept WlObjectSpecListener = WlObjectSpec<T> && requires {
    T::listener;
    reinterpret_cast<void(**)(void)>(&T::listener);
};

/**
 * Base class for all C++-managed Wayland object types.
 * @tparam spec A Wayland object specification. See the WlObjectSpec concept for more info.
 */
template<WlObjectSpec spec>
class WlObject {
public:
    using spec_t = spec;
    using obj_t = spec::obj_t;

    // Prevent copies and moves if the object has a listener
    // Objects with listeners send the "this" pointer to Wayland, so a move or
    // copy would cause that to be invalidated.
    WlObject(WlObject const&) requires WlObjectSpecListener<spec> = delete;
    WlObject(WlObject&&) noexcept requires WlObjectSpecListener<spec> = delete;
    WlObject& operator=(WlObject const&) requires WlObjectSpecListener<spec> = delete;
    WlObject& operator=(WlObject&&) noexcept requires WlObjectSpecListener<spec> = delete;

    // Explicitly force the generation of default copy/move constructors if the object
    // doesn't have a listener. This is required because "requires" doesn't actually
    // remove the method, just skips it during overload resolution, so the defaults
    // wouldn't be generated by the compiler even if the block above was skipped.
    WlObject(WlObject const&) requires (!WlObjectSpecListener<spec>) = default;
    WlObject(WlObject&&) noexcept requires (!WlObjectSpecListener<spec>) = default;
    WlObject& operator=(WlObject const&) requires (!WlObjectSpecListener<spec>) = default;
    WlObject& operator=(WlObject&&) noexcept requires (!WlObjectSpecListener<spec>) = default;

private:
    static inline constexpr char const* interfaceName() {
        return spec::interface.name;
    }

    static inline constexpr auto deleter() {
        if constexpr (WlObjectSpecCustomDeleter<spec>)
            return spec::deleter;
        else
            return reinterpret_cast<void(*)(obj_t*)>(&wl_proxy_destroy);
    }

    std::unique_ptr<obj_t, decltype(deleter())> m_value;

public:
    explicit WlObject(obj_t* ptr) : m_value { ptr, deleter() } {
        if (!m_value) {
            throw WlException("Failed to initialize ", interfaceName());
        }

        if constexpr (WlObjectSpecListener<spec>) {
            auto result = wl_proxy_add_listener(
                    proxy(),
                    reinterpret_cast<void(**)(void)>(&spec::listener),
                    this
            );
            if (result != 0) {
                throw WlException("Failed to set listener for ", interfaceName());
            }
        }
    }

    [[nodiscard]] inline wl_proxy* proxy() const { return reinterpret_cast<wl_proxy*>(value()); }
    [[nodiscard]] inline obj_t* value() const { return m_value.get(); }
};

/**
 * Concept that can be used to detect if a given type parameter is a child
 * of WlObject, with any object specification.
 */
template<typename T, typename spec_t = T::spec_t>
concept IsWlObject = std::is_base_of_v<WlObject<spec_t>, T>;

/**
 * Macro to define members of the WlObjectSpec concept based on libwayland's
 * naming conventions. Pass in the name of the Wayland object in lower snake case and
 * the supported interface version.
 */
#define WL_SPEC_BASE(p_name, p_version)                   \
    using obj_t = p_name;                                                \
    static constexpr wl_interface const& interface = p_name##_interface; \
    static constexpr std::uint32_t version = p_version;

/**
 * Macro to define members of the WlObjectSpecCustomDeleter concept based on libwayland's
 * naming conventions for objects that have "destroy" methods.
 * Pass in the name of the Wayland object in lower snake case.
 */
#define WL_SPEC_DESTROY(p_name)         \
    static constexpr auto deleter = &p_name##_destroy;

/**
 * Macro to define members of the WlObjectSpecCustomDeleter concept based on libwayland's
 * naming conventions for objects that have "release" methods.
 * Pass in the name of the Wayland object in lower snake case.
 */
#define WL_SPEC_RELEASE(p_name)                        \
    static constexpr auto deleter = &p_name##_release; \

/**
 * Macro to define members of the WlObjectSpecListener concept based on libwayland's
 * naming conventions. Pass in the name of the Wayland object in lower snake case and
 * the supported interface version.
 */
#define WL_SPEC_LISTENER(p_name)       \
    static p_name##_listener listener; \

/**
 * This function can be used in listeners for events that the application doesn't
 * want to handle, since nullptr isn't a valid handler.
 */
template<typename... args_t>
void noHandler(args_t... args) { }


/**
 * Helper for classOfFunction, do not use directly.
 * Extract the enclosing class of a member function pointer by creating a function
 * that has the desired value as its return type.
 */
template<IsWlObject self_t, typename... args_t>
self_t extractMemberFunctionEnclosingClass(void(self_t::*)(args_t...));

/**
 * Helper for eventHandler, do not use directly.
 * Returns the enclosing class of a member function pointer.
 */
template<auto func>
using classOfFunction = std::remove_reference_t<decltype(extractMemberFunctionEnclosingClass(func))>;

/**
 * This template will generate a static function that can be passed to libwayland
 * as a listener callback. It will forward all calls to a member function by interpreting
 * the data pointer as a "this" pointer.
 */
template<auto func, IsWlObject self_t = classOfFunction<func>, typename... args_t>
void eventHandler(void* data, typename self_t::obj_t*, args_t... args) {
    std::invoke(func, reinterpret_cast<self_t*>(data), args...);
}

/**
 * Safely extracts the value of a WlObject pointer, returning null if the object is null.
 */
template<typename T>
auto getValue(T const& ptr) -> decltype(ptr->value()) {
    if (ptr == nullptr) {
        return nullptr;
    }

    return ptr->value();
}