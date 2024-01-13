
#include <concepts>
#include <iostream>
#include <memory>
#include <wayland-client.h>
#include <xdg-shell.h>

#include <sycl/sycl.hpp>

#include <aux/unique-handle.hh>

inline namespace wayland
{
    struct empty_type { };
    template <class> constexpr wl_interface const *const interface_ptr = nullptr;
    template <class T> concept wl_class = (interface_ptr<T> != nullptr);
    template <wl_class T> void (*deleter)(T*) = nullptr;
    template <wl_class T> struct listener_type { };

#define INTERN_WL_CLASS_CONCEPT(CLASS, DELETER, LISTENER)               \
    template <> constexpr wl_interface const *const interface_ptr<CLASS> = &CLASS##_interface; \
    template <> void (*deleter<CLASS>)(CLASS*) = DELETER;               \
    template <> struct listener_type<CLASS> : LISTENER { };
    INTERN_WL_CLASS_CONCEPT(wl_display,    wl_display_disconnect, empty_type)
    INTERN_WL_CLASS_CONCEPT(wl_registry,   wl_registry_destroy,   wl_registry_listener)
    INTERN_WL_CLASS_CONCEPT(wl_compositor, wl_compositor_destroy, empty_type)
    INTERN_WL_CLASS_CONCEPT(wl_shm,        wl_shm_destroy,        wl_shm_listener)
    INTERN_WL_CLASS_CONCEPT(wl_seat,       wl_seat_destroy,       wl_seat_listener)
    INTERN_WL_CLASS_CONCEPT(wl_surface,    wl_surface_destroy,    wl_surface_listener)
    INTERN_WL_CLASS_CONCEPT(wl_shm_pool,   wl_shm_pool_destroy,   empty_type)
    INTERN_WL_CLASS_CONCEPT(wl_buffer,     wl_buffer_destroy,     wl_buffer_listener)
    INTERN_WL_CLASS_CONCEPT(wl_keyboard,   wl_keyboard_destroy,   wl_keyboard_listener)
    INTERN_WL_CLASS_CONCEPT(wl_pointer,    wl_pointer_destroy,    wl_pointer_listener)
    INTERN_WL_CLASS_CONCEPT(wl_touch,      wl_touch_destroy,      wl_touch_listener)
    INTERN_WL_CLASS_CONCEPT(xdg_wm_base,   xdg_wm_base_destroy,   xdg_wm_base_listener)
#undef INTERN_WL_CLASS_CONCEPT

    template <class T> concept wl_class_with_listener = wl_class<T> && !std::is_base_of_v<empty_type, listener_type<T>>;

    template <wl_class T>
    auto make_unique(T* raw = nullptr) noexcept {
        return std::unique_ptr<T, decltype (deleter<T>)>(raw, deleter<T>);
    }
    template <wl_class T>
    using unique_ptr_type = decltype (make_unique<T>());

    template <class> class wrapper;
    template <class T> wrapper(T*) -> wrapper<T>;

    template <wl_class T>
    class wrapper<T> {
    public:
        wrapper(T* raw = nullptr) : ptr{make_unique(raw)} { }
        operator T*() const { return this->ptr.get(); }

    private:
        unique_ptr_type<T> ptr;
    };

    template <wl_class_with_listener T>
    class wrapper<T> {
    private:
        static constexpr auto new_default_listener() {
            static constexpr auto N = sizeof (listener_type<T>) / sizeof (void*);
            return new listener_type<T>{
                []<size_t... I>(std::index_sequence<I...>) noexcept {
                    return listener_type<T>{
                        ([](auto...) noexcept {
                            (void) I;
                        })...
                    };
                }(std::make_index_sequence<N>())
            };
        }

    public:
        wrapper(T* raw = nullptr)
            : ptr{make_unique(raw)}
            , listener{new_default_listener()}
        {
            if (ptr != nullptr) {
                if (0 != wl_proxy_add_listener(reinterpret_cast<wl_proxy*>(operator T*()),
                                               reinterpret_cast<void(**)(void)>(this->listener.get()),
                                               nullptr)) {
                    throw std::runtime_error("failed to add listener...");
                }
            }
        }
        operator T*() const { return this->ptr.get(); }
        listener_type<T>* operator->() const { return this->listener.get(); }

    private:
        unique_ptr_type<T> ptr;
        std::unique_ptr<listener_type<T>> listener;
    };

    template <wl_class T>
    auto registry_bind(wl_registry* registry, uint32_t name, uint32_t version) noexcept {
        return static_cast<T*>(::wl_registry_bind(registry, name, interface_ptr<T>, version));
    }

    inline auto lamed(auto&& closure) noexcept {
        static auto cache = closure;
        return [](auto... args) {
            return cache(args...);
        };
    }
} // ::wayland

int main() {
    auto que = sycl::queue();
    std::cout << que.get_device().get_info<sycl::info::device::name>() << std::endl;

    auto display = wrapper<wl_display>{wl_display_connect(nullptr)};
    if (!display) {
        return 1;
    }
    auto registry = wrapper<wl_registry>{wl_display_get_registry(display)};
    if (!registry) {
        return 2;
    }

    wrapper<wl_compositor> compositor;
    registry->global = lamed([&](auto, auto registry, uint32_t name, std::string_view interface, uint32_t version) {
        if (interface == interface_ptr<wl_compositor>->name) {
            compositor = wrapper{registry_bind<wl_compositor>(registry, name, version)};
        }
    });
    wl_display_roundtrip(display);
    std::cout << compositor << std::endl;

    return 0;
}
