#pragma once
#include <iostream>
#include <vector>
#include <string>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <filesystem>
#include <concepts>
#include <stdexcept>
#include <fstream>
#define th_err(err) {std::cerr << err << std::endl; exit(1);}
struct Any { template<typename T> operator T(); };
// #include <experimental/reflection> // C++26 Static Reflection
template <typename T>
constexpr size_t get_n() {
    return requires { T{Any{},Any{},Any{},Any{},Any{},Any{},Any{},Any{},Any{},Any{},Any{},Any{},Any{},Any{}}; } ? 14 :
                requires { T{Any{},Any{},Any{},Any{},Any{},Any{},Any{},Any{},Any{},Any{},Any{},Any{},Any{}}; } ? 13 :
                requires { T{Any{},Any{},Any{},Any{},Any{},Any{},Any{},Any{},Any{},Any{},Any{},Any{}}; } ? 12 :
                requires { T{Any{},Any{},Any{},Any{},Any{},Any{},Any{},Any{},Any{},Any{},Any{}}; } ? 11 :
                requires { T{Any{},Any{},Any{},Any{},Any{},Any{},Any{},Any{},Any{},Any{}}; } ? 10 :
                requires { T{Any{},Any{},Any{},Any{},Any{},Any{},Any{},Any{},Any{}}; } ? 9 :
                requires { T{Any{},Any{},Any{},Any{},Any{},Any{},Any{},Any{}}; } ? 8 :
                requires { T{Any{},Any{},Any{},Any{},Any{},Any{},Any{}}; } ? 7 :
                requires { T{Any{},Any{},Any{},Any{},Any{},Any{}}; } ? 6 :
                requires { T{Any{},Any{},Any{},Any{},Any{}}; } ? 5 :
                requires { T{Any{},Any{},Any{},Any{}}; } ? 4 :
                requires { T{Any{},Any{},Any{}}; } ? 3 :
                requires { T{Any{},Any{}}; } ? 2 :
                requires { T{Any{}}; } ? 1 : 0;
}
namespace Ser {

    // --- Error Handling ---
    inline void check_io(std::ios& stream, std::string_view op) {
        if (stream.fail()) {
            th_err(std::string("IO Error during: ") + std::string(op))
        }
    }

    // --- Concepts ---
    template<typename T>
    concept HasCustomSer = requires(const T a) { a.serialize(); };

    template<typename T>
    concept IsContainer = requires(T a) { a.begin(); a.end(); } && !std::is_same_v<T, std::string>;

    template<typename T>
    concept IsSmartPtr = requires(T a) { a.get(); typename T::element_type; };

    // --- Save Logic ---
    template<typename T> void save(std::ostream& os, const T& obj);
    template<typename T> void load(std::istream& is, T& obj);
    template<typename T> void save(std::ostream& os, const T& obj);
    template<typename T> void load(std::istream& is, T& obj);

    namespace Detail {
        // Any должен быть здесь

        // Макрос без fold expressions (так как __VA_ARGS__ это не пак)
        // Используем явный вызов для каждого аргумента
        #define DEFINE_STEP(N, ...) \
            template<typename T, typename Stream> \
            auto io_impl(Stream& s, T& obj, std::integral_constant<size_t, N>) \
            -> decltype(void([](auto& t){ auto& [__VA_ARGS__] = t; }(obj))) { \
                auto& [__VA_ARGS__] = obj; \
                if constexpr (requires{ save(s, a1); }) { \
                    SerializeAll(s, __VA_ARGS__); \
                } else { \
                    DeserializeAll(s, __VA_ARGS__); \
                } \
            }

        // Вспомогательные функции для имитации пака (через перегрузки)
        template<typename S, typename... Args>
        void SerializeAll(S& s, Args&... args) { (save(s, args), ...); }

        template<typename S, typename... Args>
        void DeserializeAll(S& s, Args&... args) { (load(s, args), ...); }

        DEFINE_STEP(1, a1)
        DEFINE_STEP(2, a1, a2)
        DEFINE_STEP(3, a1, a2, a3)
        DEFINE_STEP(4, a1, a2, a3, a4)
        DEFINE_STEP(5, a1, a2, a3, a4, a5)
        DEFINE_STEP(6, a1, a2, a3, a4, a5, a6)
        DEFINE_STEP(7, a1, a2, a3, a4, a5, a6, a7)
        DEFINE_STEP(8, a1, a2, a3, a4, a5, a6, a7, a8)
        DEFINE_STEP(9, a1, a2, a3, a4, a5, a6, a7, a8, a9)
        DEFINE_STEP(10, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10)
        DEFINE_STEP(11, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11)
        DEFINE_STEP(12, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12)
        DEFINE_STEP(13, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13)
        DEFINE_STEP(14, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14)
        
        #undef DEFINE_STEP
    }
    template<typename T>
    void save(std::ostream& os, const T& obj) {
        // 1. Primitives (Arithmetic, Enums)
        if constexpr (std::is_arithmetic_v<T> || std::is_enum_v<T>) {
            os.write(reinterpret_cast<const char*>(&obj), sizeof(T));
            check_io(os, "Write Primitive");
        }
        // 2. Strings
        else if constexpr (std::is_same_v<T, std::string>) {
            uint64_t sz = obj.size();
            save(os, sz);
            os.write(obj.data(), sz);
            check_io(os, "Write String Data");
        }
        // 3. Smart Pointers
        else if constexpr (IsSmartPtr<T>) {
            bool exists = (obj.get() != nullptr);
            save(os, exists);
            if (exists) save(os, *obj);
        }
        else if constexpr (requires { obj.first; obj.second; }) {
            save(os, obj.first);
            save(os, obj.second);
        }
        // 4. Containers (vector, unordered_map, unordered_set)
        else if constexpr (IsContainer<T>) {
            uint64_t sz = obj.size();
            save(os, sz);
            for (const auto& item : obj) {
                if constexpr (requires { item.first; item.second; }) { // Map support
                    save(os, item.first);
                    save(os, item.second);
                } else {
                    save(os, item);
                }
            }
        }
        // 5. Custom Structures with void* (uses provided serialize() method)
        else if constexpr (HasCustomSer<T>) {
            auto fields = obj.serialize();
            save(os, fields); // Recursively save the unordered_map
        }
        // 6. Aggregate Structures (Automatic Reflection for struct {int x, y; string s;})
        else if constexpr (std::is_aggregate_v<T>) {
            // ИНТЕГРАЦИЯ: Безопасно считаем количество полей
            constexpr size_t N = get_n<T>();

            if constexpr (N > 0) {
                // Вызываем нужную перегрузку. const_cast нужен, так как save принимает const T&, 
                // а Structured Bindings иногда капризничают с константностью.
                Detail::io_impl(os, const_cast<T&>(obj), std::integral_constant<size_t, N>{});
            }
        }
    }
    template<typename T>
    void load(std::istream& is, T& obj) {
        if constexpr (std::is_arithmetic_v<T> || std::is_enum_v<T>) {
            is.read(reinterpret_cast<char*>(&obj), sizeof(T));
            check_io(is, "Read Primitive");
        }
        else if constexpr (std::is_same_v<T, std::string>) {
            uint64_t sz;
            load(is, sz);
            if (sz > 512 * 1024 * 1024) th_err("Security: String exceeds 512MB");
            obj.resize(sz);
            is.read(obj.data(), sz);
            check_io(is, "Read String Content");
        }
        else if constexpr (IsSmartPtr<T>) {
            bool exists;
            load(is, exists);
            if (exists) {
                if (!obj) obj = std::make_unique<typename T::element_type>();
                load(is, *obj);
            } else {
                obj.reset();
            }
        }
        else if constexpr (requires { obj.first; obj.second; }) {
            load(is, obj.first);
            load(is, obj.second);
        }
        else if constexpr (IsContainer<T>) {
            uint64_t sz;
            load(is, sz);
            if (sz > 10'000'000) th_err("Security: Container size limit exceeded");
            
            // Очищаем контейнер перед загрузкой, чтобы не перемешивать старое с новым
            obj.clear();

            // 1. Обработка Map и Set
            if constexpr (requires { typename T::key_type; }) { 
                using Key = std::remove_const_t<typename T::key_type>;
                
                for (uint64_t i = 0; i < sz; ++i) {
                    if constexpr (requires { typename T::mapped_type; }) { // Map
                        Key k; typename T::mapped_type v;
                        load(is, k); 
                        load(is, v);
                        obj.emplace(std::move(k), std::move(v)); // Используем emplace для скорости
                    } else { // Set
                        Key k; 
                        load(is, k);
                        obj.insert(std::move(k));
                    }
                }
            } 
            // 2. Обработка Vector, List, Deque
            else { 
                if constexpr (requires { obj.resize(sz); }) {
                    // Для векторов делаем resize один раз и читаем прямо в ячейки
                    obj.resize(sz);
                    for (uint64_t i = 0; i < sz; ++i) {
                        load(is, obj[i]);
                    }
                } else {
                    // Для списков и прочего используем emplace_back
                    for (uint64_t i = 0; i < sz; ++i) {
                        typename T::value_type item;
                        load(is, item);
                        obj.emplace_back(std::move(item));
                    }
                }
            }
        }
        else if constexpr (HasCustomSer<T>) {
            std::unordered_map<std::string, std::vector<uint8_t>> fields;
            load(is, fields);
            // You must implement apply_fields or similar to restore void* data
            if constexpr (requires { obj.apply_fields(fields); }) {
                obj.apply_fields(fields);
            }
        }
        else if constexpr (std::is_aggregate_v<T>) {
            constexpr size_t N = get_n<T>();

            if constexpr (N > 0) {
                Detail::io_impl(is, obj, std::integral_constant<size_t, N>{});
            }
        }
    }
    template<typename T>
    std::vector<uint8_t> to_buffer(const T& obj) {
        std::vector<uint8_t> buffer;
        std::string data;
        struct VecStream : std::stringbuf {
            std::string& s;
            VecStream(std::string& str) : s(str) {}
            int sync() override { return 0; }
        };
        
        std::ostringstream oss;
        save(oss, obj);
        std::string s = oss.str();
        return std::vector<uint8_t>(s.begin(), s.end());
    }
    struct MemoryStream : std::streambuf {
        MemoryStream(uint8_t* data, size_t size) {
            char* ptr = reinterpret_cast<char*>(data);
            setg(ptr, ptr, ptr + size);
        }
    };

    template<typename T>
    T from_buffer(const std::vector<uint8_t>& buffer) {
        MemoryStream ms(const_cast<uint8_t*>(buffer.data()), buffer.size());
        std::istream is(&ms);
        T obj;
        load(is, obj);
        return obj;
    }
    template<typename T>
    void load_from_buffer(T& obj, const std::vector<uint8_t>& buffer) {
        MemoryStream ms(const_cast<uint8_t*>(buffer.data()), buffer.size());
        std::istream is(&ms);
        load(is, obj);
    }
}

// --- Example Usage ---
namespace fs = std::filesystem;

template<typename T>
void save_to_file(const fs::path& p, const T& obj) {
    std::ofstream ofs(p, std::ios::binary);
    if (ofs.fail()) th_err("Cannot open file for writing");
    Ser::save(ofs, obj);
}

template<typename T>
void load_from_file(const fs::path& p, T& obj) {
    std::ifstream ifs(p, std::ios::binary);
    if (ifs.fail()) th_err("Cannot open file for reading");
    Ser::load(ifs, obj);
}
