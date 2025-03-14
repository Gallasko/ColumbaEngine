#pragma once

#include "interpreter.h"

#include "ECS/system.h"



namespace pg
{
    struct CallableIntepretedFunction;

    class InterpreterSystem : public System<>
    {
    public:
        InterpreterSystem(std::shared_ptr<Environment> env, std::shared_ptr<ClassInstance> sysInstance) : env(env), sysInstance(sysInstance)
        {
            LOG_THIS_MEMBER("Interpreter System");
        }

        virtual void addToRegistry(ComponentRegistry *registry) override;

        virtual void execute() override;

        virtual std::string getSystemName() const override { return name; }

        // Todo ?
        void onEvent(_unique_id, std::shared_ptr<ClassInstance>)
        {
            LOG_THIS_MEMBER("Interpreter System");
        }

    private:
        std::shared_ptr<Environment> env;
        std::shared_ptr<ClassInstance> sysInstance;
        std::shared_ptr<CallableIntepretedFunction> executeMethod;

        std::string name;
    };

}