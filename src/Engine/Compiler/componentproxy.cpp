#include "stdafx.h"

#include "componentproxy.h"

#include "vm.h"

#include "ECS/entitysystem.h"

namespace pg
{
    namespace
    {
        static constexpr char const * DOM = "Component Proxy";

        Value getStandardComponentProperty(void* component, const std::string& name, VM* vm)
        {
            auto comp = static_cast<StandardComponent*>(component);

            if (comp->has(name))
                return vm->elementToValue(comp->properties.at(name));

            return INT_VAL(-1);
        }

        void setStandardComponentProperty(void* component, const std::string& name, VM* vm, Value value)
        {
            auto comp = static_cast<StandardComponent*>(component);

            ElementType newValue = vm->valueToElement(value);

            comp->setWithEvent(name, newValue);
        }

        // Metamethods of the proxy class. The property opcodes call ComponentProxy directly and never get here,
        // these only serve the accesses that go through the generic metamethod lookup (index access for example)
        Value proxyGet(VM* vm, int argCount, Value* args)
        {
            if (argCount < 2 or not IS_INSTANCE(args[0]) or not IS_STRING(args[1]))
            {
                vm->runtimeError("__get requires a proxy and a property name");

                return INT_VAL(0);
            }

            auto self = vm->asInstance(args[0]);

            if (self->proxyMeta == nullptr)
            {
                vm->runtimeError("ComponentProxy is not bound to a component");

                return INT_VAL(-1);
            }

            return ComponentProxy::getProperty(vm, self, vm->asString(args[1]));
        }

        Value proxySet(VM* vm, int argCount, Value* args)
        {
            if (argCount < 3 or not IS_INSTANCE(args[0]) or not IS_STRING(args[1]))
            {
                vm->runtimeError("__set requires a proxy, a property name and a value");

                return INT_VAL(0);
            }

            auto self = vm->asInstance(args[0]);
            Value newValue = args[2];

            if (self->proxyMeta == nullptr)
            {
                vm->runtimeError("ComponentProxy is not bound to a component");

                return newValue;
            }

            auto name = vm->asString(args[1]);

            if (not ComponentProxy::setProperty(vm, self, name, newValue))
                vm->runtimeError("Property '" + name + "' is read-only");

            return newValue;
        }
    }

    void ComponentProxy::registerWithVM(VM* vm)
    {
        Value klassValue = vm->createClass("ComponentProxy");

        vm->addNativeMethod(klassValue, "__get", proxyGet);
        vm->addNativeMethod(klassValue, "__set", proxySet);

        vm->defineGlobal("ComponentProxy", vm->retainValue(klassValue));

        vm->componentProxyClass = vm->asClass(klassValue);

        // Every StandardComponent, whatever its runtime type name, goes through this one entry:
        // its properties are looked up by name in the component at access time
        ComponentProxyMetadata standardMeta;

        standardMeta.componentTypeName = "StandardComponent";
        standardMeta.dynamicGetter = getStandardComponentProperty;
        standardMeta.dynamicSetter = setStandardComponentProperty;

        ComponentProxyRegistry::instance().registerMetadata(standardMeta);
    }

    Value ComponentProxy::createProxy(VM* vm, const ComponentProxyMetadata* metadata, void* component)
    {
        if (vm->componentProxyClass == nullptr)
            throw std::runtime_error("ComponentProxy class not registered with VM");

        Value proxyValue = vm->createInstance(vm->componentProxyClass);

        auto proxy = vm->asInstance(proxyValue);

        proxy->proxyTarget = component;
        proxy->proxyMeta = metadata;

        return proxyValue;
    }

    Value ComponentProxy::getProperty(VM* vm, ObjInstance* proxy, const std::string& name)
    {
        auto metadata = proxy->proxyMeta;
        auto property = metadata->findProperty(name);

        if (property != nullptr)
        {
            if (property->getter)
                return property->getter(proxy->proxyTarget, vm);

            LOG_WARNING(DOM, "No getter function for property '" << name << "' !");

            return INT_VAL(-1);
        }

        if (name == "__className")
            return vm->createString(metadata->componentTypeName);

        if (metadata->dynamicGetter)
            return metadata->dynamicGetter(proxy->proxyTarget, name, vm);

        return INT_VAL(-1);
    }

    bool ComponentProxy::setProperty(VM* vm, ObjInstance* proxy, const std::string& name, Value value)
    {
        auto metadata = proxy->proxyMeta;
        auto property = metadata->findProperty(name);

        if (property == nullptr)
        {
            if (metadata->dynamicSetter)
                metadata->dynamicSetter(proxy->proxyTarget, name, vm, value);

            return true;
        }

        if (not property->writable)
            return false;

        if (property->setter)
        {
            property->setter(proxy->proxyTarget, vm, value);
        }
        else
        {
            LOG_WARNING(DOM, "No setter function for property '" << name << "' !");
        }

        return true;
    }
}
