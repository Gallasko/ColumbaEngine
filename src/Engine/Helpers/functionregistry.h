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

    // Base class for function wrapper
    struct IFunctionWrapper {
        virtual ~IFunctionWrapper() = default;
        virtual std::any call(const std::vector<std::any>& args) = 0;
    };

    // Templated derived class for wrapping actual function
    template<typename R, typename... Args>
    class FunctionWrapper : public IFunctionWrapper {
    public:
        FunctionWrapper(std::function<R(Args...)> func) : func_(func) {}

        std::any call(const std::vector<std::any>& args) override {
            if (args.size() != sizeof...(Args)) throw std::runtime_error("Argument count mismatch");
            return callImpl(args, std::index_sequence_for<Args...>{});
        }

    private:
        template<std::size_t... Is>
        std::any callImpl(const std::vector<std::any>& args, std::index_sequence<Is...>) {
            return func_(std::any_cast<Args>(args[Is])...);
        }

        std::function<R(Args...)> func_;
    };

    // Helper to deduce function traits
    template<typename T>
    struct FunctionTraits;

    // Function pointer
    template<typename R, typename... Args>
    struct FunctionTraits<R(*)(Args...)> {
        using return_type = R;
        using function_type = std::function<R(Args...)>;
    };

    // std::function
    template<typename R, typename... Args>
    struct FunctionTraits<std::function<R(Args...)>> {
        using return_type = R;
        using function_type = std::function<R(Args...)>;
    };

    // Lambda or functor
    template<typename T>
    struct FunctionTraits {
    private:
        using call_type = FunctionTraits<decltype(&T::operator())>;
    public:
        using return_type = typename call_type::return_type;
        using function_type = typename call_type::function_type;
    };

    // Lambda operator()
    template<typename C, typename R, typename... Args>
    struct FunctionTraits<R(C::*)(Args...) const> {
        using return_type = R;
        using function_type = std::function<R(Args...)>;
    };

    class FunctionRegistry {
    public:
        template<typename F>
        void add(const std::string& name, F func) {
            using Traits = FunctionTraits<F>;
            functions_[name] = std::make_unique<FunctionWrapper<typename Traits::function_type::result_type, typename Traits::function_type::argument_type::type...>>(Traits::function_type(func));
        }

        std::any call(const std::string& name, const std::vector<std::any>& args) {
            auto it = functions_.find(name);
            if (it == functions_.end()) throw std::runtime_error("Function not found");
            return it->second->call(args);
        }

    private:
        std::unordered_map<std::string, std::unique_ptr<IFunctionWrapper>> functions_;
    };
}