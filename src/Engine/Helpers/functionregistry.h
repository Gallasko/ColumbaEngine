#include <functional>
#include <unordered_map>
#include <string>
#include <memory>
#include <any>
#include <vector>
#include <tuple>
#include <type_traits>
#include <stdexcept>

namespace pg {
    // Base interface for all wrappers
    struct IFunctionWrapper {
        virtual ~IFunctionWrapper() = default;
        virtual std::any call(const std::vector<std::any>& args) = 0;
    };

    // Wrapper for non-void return
    template<typename R, typename... Args>
    class FunctionWrapper : public IFunctionWrapper {
    public:
        explicit FunctionWrapper(std::function<R(Args...)> f) : func(std::move(f)) {}

        std::any call(const std::vector<std::any>& args) override {
            if (args.size() != sizeof...(Args)) throw std::runtime_error("Argument count mismatch");
            return callImpl(args, std::index_sequence_for<Args...>{});
        }

    private:
        template<std::size_t... I>
        std::any callImpl(const std::vector<std::any>& args, std::index_sequence<I...>) {
            return func(std::any_cast<std::remove_cv_t<std::remove_reference_t<Args>>>(args[I])...);
        }

        std::function<R(Args...)> func;
    };

    // Wrapper specialization for void return
    template<typename... Args>
    class FunctionWrapper<void, Args...> : public IFunctionWrapper {
    public:
        explicit FunctionWrapper(std::function<void(Args...)> f) : func(std::move(f)) {}

        std::any call(const std::vector<std::any>& args) override {
            if (args.size() != sizeof...(Args)) throw std::runtime_error("Argument count mismatch");
            callImpl(args, std::index_sequence_for<Args...>{});
            return {};
        }

    private:
        template<std::size_t... I>
        void callImpl(const std::vector<std::any>& args, std::index_sequence<I...>) {
            func(std::any_cast<std::remove_cv_t<std::remove_reference_t<Args>>>(args[I])...);
        }

        std::function<void(Args...)> func;
    };

    class FunctionRegistry {
    public:
        FunctionRegistry() = default;
        FunctionRegistry(FunctionRegistry&&) noexcept = default;
        FunctionRegistry& operator=(FunctionRegistry&&) noexcept = default;
        FunctionRegistry(const FunctionRegistry&) = delete;
        FunctionRegistry& operator=(const FunctionRegistry&) = delete;

        // Directly add a std::function with signature R(Args...)
        template<typename R, typename... Args>
        void add(const std::string& name, std::function<R(Args...)> func) {
            functions_[name] = std::make_unique<FunctionWrapper<R, Args...>>(std::move(func));
        }

        // Add any callable (e.g., lambda); deduced to std::function
        template<typename Func>
        void add(const std::string& name, Func func) {
            add(name, std::function(std::move(func)));
        }

        // Call stored function by name; returns R
        template<typename R, typename... Args>
        R call(const std::string& name, Args&&... args) {
            auto it = functions_.find(name);
            if (it == functions_.end()) throw std::runtime_error("Function not found: " + name);
            std::vector<std::any> packed{std::forward<Args>(args)...};
            std::any result = it->second->call(packed);
            if constexpr (!std::is_void_v<R>) return std::any_cast<R>(result);
        }

    private:
        std::unordered_map<std::string, std::unique_ptr<IFunctionWrapper>> functions_;
    };


}