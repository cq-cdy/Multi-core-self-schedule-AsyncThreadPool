#include <iostream>
#include <memory>

class Any {
public:
//    template<class T>
//    //explicit Any(T data):base_(new Storage_<T>(data)){} //一定要new，因为是只能指针，指向地址
//    //explicit Any(T data):base_(std::make_unique<Storage_<T>(std::move(data))){}

    template<class T>
    Any(T data):base_(std::make_unique<Storage_<T>>(std::move(data))){

    }
    template<class T>
    T cast() {
        Storage_ <T> *data_p = dynamic_cast<Storage_ <T>*> (base_.get());
        if (data_p == nullptr) {
            throw std::bad_cast();
        }
        return data_p->data_;
    }

    Any() = default;

    Any(Any &&) = default;

    Any(const Any &) = delete;

    Any &operator=(const Any &) = delete;

    Any &operator=(Any &) = delete;

    Any &operator=(Any &&) = default;

private:
    class Base {
    public:
        virtual  ~Base() = default;
    };

    template<class T>
    class Storage_ :public Base{
    public:
        explicit Storage_(T data) : data_(data) {}
        T data_;
    };

    std::unique_ptr<Base> base_;
};

#if 0
#include <stdexcept>
#include <type_traits>
class Any {
public:
    //Any() = default;
    Any() : destruct_fn(nullptr) {}
    Any(const Any& other) : destruct_fn(other.destruct_fn) {
        if (other.base) {
            base = other.base->clone();
        } else if (other.destruct_fn) {
            other.destruct_fn(&other.inline_storage, &inline_storage);
        }
    }

    Any(Any&& other) noexcept : base(std::move(other.base)), destruct_fn(other.destruct_fn) {
        if (other.destruct_fn) {
            other.destruct_fn(&other.inline_storage, &inline_storage);
        }
    }

    template <class T, typename = std::enable_if_t<!std::is_same_v<std::decay_t<T>, Any>>>
    Any(T&& data) : destruct_fn(nullptr) {
        if constexpr (std::is_trivially_copyable_v<T> && sizeof(T) <= sizeof(inline_storage)) {
            new (&inline_storage) T(std::forward<T>(data));
            destruct_fn = [](const void* src, void* dst) {
                new (dst) T(*reinterpret_cast<const T*>(src));
            };
        } else {
            base = std::make_unique<Storage_<T>>(std::forward<T>(data));
        }
    }


    template <typename T>
    Any& operator=(T&& data) {
        this->~Any();
        new (this) Any(std::forward<T>(data));
        return *this;
    }

    Any& operator=(const Any& other) {
        if (&other != this) {
            this->~Any();
            new (this) Any(other);
        }
        return *this;
    }

    Any& operator=(Any&& other) noexcept {
        if (&other != this) {
            this->~Any();
            new (this) Any(std::move(other));
        }
        return *this;
    }

    template <typename T>
    [[nodiscard]] T cast() const {
        if constexpr (std::is_trivially_copyable_v<T> && sizeof(T) <= sizeof(base)) {
            return *reinterpret_cast<const T*>(&inline_storage);
        } else {
            if (base) {
                auto* derived = dynamic_cast<Storage_<T>*>(base.get());
                if (derived) {
                    return derived->data_;
                }
            }
        }
        throw std::bad_cast();
    }

    bool has_value() const { return (base != nullptr) || (inline_storage != 0); }

    void swap(Any& other) noexcept {
        Any temp = std::move(*this);
        *this = std::move(other);
        other = std::move(temp);
    }

    ~Any() {
        if (base) {
            base.reset();
        }
        if (destruct_fn) {
            destruct_fn(&inline_storage, nullptr);
        }
    }

private:
    class Base {
    public:
        Base() = default;
        virtual ~Base() = default;
        virtual std::unique_ptr<Base> clone() const = 0;
    };

    template <class T>
    class Storage_ : public Base {
    public:
        explicit Storage_(T data) : data_(std::move(data)) {}
        [[nodiscard]] std::unique_ptr<Base> clone() const override {
            return std::make_unique<Storage_<T>>(data_);
        }
        T data_;
    };

    using DestructFn = void(*)(const void*, void*);

    union {
        std::unique_ptr<Base> base = nullptr;
        alignas(std::max_align_t) char inline_storage[16]; // 16 bytes as an example; can be adjusted
    };

    DestructFn destruct_fn;
};

#endif

