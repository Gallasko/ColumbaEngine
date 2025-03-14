/**
 * @file valuable.cpp
 * @author Gallasko
 * @brief Implementation of the valuables objects
 * @version 1.0
 * @date 2022-04-12
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#include "valuable.h"
#include "environment.h"
#include "expression.h"
#include "statement.h"
#include "interpreter.h"

namespace pg
{

    /**
     * @brief Function used to create the message of the exception
     * 
     * @param token   The token of the exception, it contains data about the position of the exception in the script file
     * @param message The actual message of the exception
     * 
     * @return A string representation of the exception
     */
    std::string RuntimeException::createErrorMessage(const Token& token, const std::string& message) const noexcept
    {
        std::string errStr = "Runtime Error: " + message + " at line " + std::to_string(token.line) + ", column " + std::to_string(token.column);
        
        return errStr;
    }

    /**
     * @brief Construct a new Function Valuable object
     * 
     * @param env       The parent scope of the function for Valuable resolution
     * @param name      The name of the function to be constructed
     * @param token     The token of the function (for exception purposes)
     * @param visitor   A pointer to the visitor object to be able to execute the body of the function
     * @param paramList A list of the parameters of the function
     * @param body      The function body, which is a list of statements to be executed
     */
    Function::Function(std::shared_ptr<Environment> env, const std::string& name, const Token& token, VisitorInterpreter* visitor, std::queue<ExprPtr> argsList, StatementPtr body) : 
        env(std::make_shared<Environment>(env)),
        name(ElementType{name}),
        token(token),
        visitor(visitor),
        argsList(argsList)
    {
        // Loop over the parameters to push there name in the scope of the function
        while(argsList.size() > 0)
        {
            auto expr = argsList.front();

            paramList.push_back(expr->getName());

            // TODO doesn t increment min if an argument can be defaulted
            arity.min++;
            arity.max++;

            argsList.pop();
        }

        // Queue the funciton body
        this->body.push(body);
    }

    /**
     * @brief Construct a standalone copy of Function object
     * 
     * @param visitorRef 
     * @param base 
     */
    Function::Function(std::shared_ptr<VisitorReference> visitorRef, std::shared_ptr<Function> base) : env(base->env), name(base->name), token(base->token), visitor(visitorRef.get()), paramList(base->paramList), body(base->body), arity(base->arity)
    {

    }

    /**
     * @brief Construct a copy of Function object
     * 
     * @param other The function to copy
     */
    Function::Function(const Function& other) : env(other.env), name(other.name), token(other.token), visitor(other.visitor), paramList(other.paramList), body(other.body), arity(other.arity)
    {
    }

    /**
     * @brief Return a reference to the function name if it is not in a call expression
     * 
     * @return The name of the function
     */
    std::shared_ptr<Valuable> Function::getValue() const
    {
        return std::make_shared<Variable>(name);
    }

    /**
     * @brief Return a reference to the value obtained from the function call
     * 
     * @param args A list of the arguments to be passed to the function
     * @return A reference to the value obtained from the function call
     * 
     * Can throw an exception if the number of arguments doens't match number of parameters acceptable by the function
     */
    std::shared_ptr<Valuable> Function::getValue(std::queue<std::shared_ptr<Valuable>>& args)
    {
        // Check if the number of arguments of the function call matches the number of parameters acceptable
        if((args.size() < arity.min) or (args.size() > arity.max))
            throw RuntimeException(token, "Invalid number of arguments for function call: '" + token.text + "' expected between: " + std::to_string(arity.min) + " and " + std::to_string(arity.max) + ", provided: " + std::to_string(args.size()) + ".");

        // Return the value of the function call
        return this->call(args);
    }

    std::shared_ptr<Function> Function::bind(std::shared_ptr<ClassInstance> instance)
    {
        std::shared_ptr<Environment> closure = std::make_shared<Environment>(env);

        closure->declareValue("this", instance);

        return std::make_shared<Function>(closure, token.text, token, visitor, argsList, body.front());
    }

    /**
     * @brief The piece of code to be executed when a function call is made
     * 
     * @param args  A list of the arguments to be passed to the function
     * @return A reference to the value obtained from the function call
     * 
     * This function create a new scope for the function call push the arguments in it and call the function body
     * This methode can be overriden to define system functions.
     */
    std::shared_ptr<Valuable> Function::call(std::queue<std::shared_ptr<Valuable>>& args)
    {
        int i = 0;

        // Create a new scope for the function call to correctly handle any recursive function
        auto currentEnv = std::make_shared<Environment>(env);

        // Loop over the arguments send and push them in scope
        while(args.size() > 0)
        {
            currentEnv->declareValue(paramList.at(i), args.front());

            i++;
            args.pop();
        }

        // Execute the function body to get the resulting value
        auto value = executeBlock(body, visitor, std::make_shared<Environment>(currentEnv));

        // Clear any return flag in case the function returned early
        visitor->resetReturnFlags();

        // Return the value calculated
        return value;
    }

    Class::Class(std::shared_ptr<Environment> env, const std::string& name, const Token& token, VisitorInterpreter* visitor, const std::unordered_map<std::string, std::shared_ptr<Function>>& methods) : 
        env(std::make_shared<Environment>(env)),
        name(ElementType{name}),
        token(token),
        visitor(visitor),
        methods(methods)
    {
    }

    Class::Class(const Class& other) : env(other.env), name(other.name), token(other.token), visitor(other.visitor)
    {
    }

    std::shared_ptr<Valuable> Class::getValue() const
    {
        return std::make_shared<Variable>(name);
    }

    std::shared_ptr<Valuable> Class::getValue(std::queue<std::shared_ptr<Valuable>>& args)
    {
        auto instance = std::make_shared<ClassInstance>(this);

        std::unordered_map<std::string, std::shared_ptr<Function>> boundMethods;

        std::shared_ptr<Function> initializerMethod = nullptr;
        for (const auto& method : methods)
        {
            boundMethods[method.first] = method.second->bind(instance);

            if (method.first == "init")
                initializerMethod = boundMethods[method.first];
        }

        instance->setMethods(boundMethods);

        if (initializerMethod != nullptr)
            initializerMethod->getValue(args);

        return instance;
    }

    std::shared_ptr<Function> Class::findMethodByName(const std::string& name) const
    {
        const auto it = methods.find(name);

        if (it != methods.end())
            return it->second;
        
        return nullptr;
    }

    ClassInstance::ClassInstance(const Class *klass) : 
        klass(klass)
    {
        if (klass != nullptr)
            name = ElementType{"Instance of "} + klass->getElement();
        else
            name = ElementType{"Instance of System Class"};
    }

    ClassInstance::ClassInstance(const ClassInstance& other) : klass(other.klass), name(other.name), boundMethods(other.boundMethods)
    {
    }

    void ClassInstance::setMethods(const std::unordered_map<std::string, std::shared_ptr<Function>>& methods)
    {
        this->boundMethods = methods;
    }

    const ElementType& ClassInstance::getElement() const
    {
        return name;
    }

    std::shared_ptr<Valuable> ClassInstance::getValue() const
    {
        return std::make_shared<Variable>(name);
    }

    std::shared_ptr<Valuable> ClassInstance::getValue(std::queue<std::shared_ptr<Valuable>>&)
    {
        return std::make_shared<Variable>(name);
    }

    std::shared_ptr<Valuable> ClassInstance::get(const Token& token) const
    {
        const auto it = std::find(fields.begin(), fields.end(), token.text);

        if (it != fields.end())
            return it->value;

        const auto method = findMethod(token.text);

        if (method != nullptr)
            return method;

        throw RuntimeException(token, "Undefined property '" + token.text + "'.");
    }

    void ClassInstance::set(const Token& token, std::shared_ptr<Valuable> value)
    {
        const auto it = std::find(fields.begin(), fields.end(), token.text);

        if (it != fields.end())
        {
            it->value = value;
        }
        else
        {
            fields.emplace_back(token.text, value);
        }
    }

    void ClassInstance::pushback(std::shared_ptr<Valuable> value)
    {
        auto size = std::to_string(getSize());
        const auto it = std::find(fields.begin(), fields.end(), size);

        if (it != fields.end())
        {
            it->value = value;
        }
        else
        {
            fields.emplace_back(size, value);
        }
    }

    void ClassInstance::remove(const std::string& key)
    {
        const auto it = std::find(fields.begin(), fields.end(), key);

        if (it != fields.end())
        {
            fields.erase(it);
        }
    }

    std::shared_ptr<Function> ClassInstance::findMethod(const std::string& name) const
    {
        const auto it = boundMethods.find(name);

        if (it != boundMethods.end())
            return it->second;
        
        return nullptr;
    }

    AtFunction::AtFunction(ExprPtr self, std::shared_ptr<Environment> env, const std::string& name, const Token& token, VisitorInterpreter* visitor, std::queue<ExprPtr> argsList, StatementPtr body, std::shared_ptr<ClassInstance> instance) :
        Function(env, name, token, visitor, argsList, body),
        self(self),
        instance(instance)
    {
        setArity(1, 1);
    }

    std::shared_ptr<Function> AtFunction::bind(std::shared_ptr<ClassInstance> instance)
    {
        std::shared_ptr<Environment> closure = std::make_shared<Environment>(env);

        closure->declareValue("this", instance);

        std::queue<ExprPtr> emptyQueue;
        return std::make_shared<AtFunction>(self, closure, token.text, token, visitor, emptyQueue, nullptr);
    }

    ValuablePtr AtFunction::call(ValuableQueue& args)
    {
        auto key = args.front()->getElement();
        args.pop();

        // Todo kind of hax to make it work with system created list (through the use of makeList())
        if (instance)
        {
            return instance->get(Token{TokenType::EXPRESSION, key.toString(), 0, 0});
        }

        // Create a new scope for the function call to correctly handle any recursive function
        auto currentEnv = std::make_shared<Environment>(env);

        std::queue<StatementPtr> body;

        std::shared_ptr<This> selfExpr;
        if (self->getType() == "This")
            selfExpr = std::static_pointer_cast<This>(self);
        else
            throw RuntimeException(Token{TokenType::EXPRESSION, "Unknown Token", 0, 0}, "Keyword 'This' is expected in a list at call !");

        auto selfToken = selfExpr->name;

        // Create the statement: return this."key"
        ExprPtr expr = std::make_shared<Get>(self, Token{TokenType::EXPRESSION, key.toString(), selfToken.line, selfToken.column});

        body.push(std::make_shared<ReturnStatement>(Token{TokenType::EXPRESSION, "return", selfToken.line, selfToken.column}, expr));

        // Execute the function body to get the resulting value
        auto value = executeBlock(body, visitor, std::make_shared<Environment>(currentEnv));

        // Clear any return flag in case the function returned early
        visitor->resetReturnFlags();

        // Return the value calculated
        return value;
    }

    SetFunction::SetFunction(ExprPtr self, std::shared_ptr<Environment> env, const std::string& name, const Token& token, VisitorInterpreter* visitor, std::queue<ExprPtr> argsList, StatementPtr body, std::shared_ptr<ClassInstance> instance) :
        Function(env, name, token, visitor, argsList, body),
        self(self),
        instance(instance)
    {
        setArity(2, 2);
    }

    std::shared_ptr<Function> SetFunction::bind(std::shared_ptr<ClassInstance> instance)
    {
        std::shared_ptr<Environment> closure = std::make_shared<Environment>(env);

        closure->declareValue("this", instance);

        std::queue<ExprPtr> emptyQueue;
        return std::make_shared<SetFunction>(self, closure, token.text, token, visitor, emptyQueue, nullptr);
    }

    ValuablePtr SetFunction::call(ValuableQueue& args)
    {
        // Create a new scope for the function call to correctly handle any recursive function
        auto currentEnv = std::make_shared<Environment>(env);

        std::queue<StatementPtr> body;

        auto key = args.front()->getElement();
        args.pop();

        auto valueKeyword = args.front()->getElement();

        // Todo kind of hax to make it work with system created list (through the use of makeList())
        if (instance)
        {
            auto value = args.front();
            instance->set(Token{TokenType::EXPRESSION, key.toString(), 0, 0}, value);
            return instance->get(Token{TokenType::EXPRESSION, key.toString(), 0, 0});
        }

        std::shared_ptr<This> selfExpr;
        if(self->getType() == "This")
            selfExpr = std::static_pointer_cast<This>(self);
        else
            throw RuntimeException(Token{TokenType::EXPRESSION, "Unknown Token", 0, 0}, "Keyword 'This' is expected in a list set call !");

        auto selfToken = selfExpr->name;

        // Create the statement: this."key" = "value" 
        ExprPtr expr;

        auto token = Token{TokenType::EXPRESSION, key.toString(), selfToken.line, selfToken.column};
        auto expr1 = std::make_shared<Atom>(valueKeyword);

        expr = std::make_shared<Set>(self, token, expr1);

        body.push(std::make_shared<ExpressionStatement>(expr));

        // Execute the function body to get the resulting value
        auto value = executeBlock(body, visitor, std::make_shared<Environment>(currentEnv));

        // Clear any return flag in case the function returned early
        visitor->resetReturnFlags();

        // Return the value calculated
        return value;
    }

    PushbackFunction::PushbackFunction(ExprPtr self, std::shared_ptr<Environment> env, const std::string& name, const Token& token, VisitorInterpreter* visitor, std::queue<ExprPtr> argsList, StatementPtr body, std::shared_ptr<ClassInstance> instance) :
        Function(env, name, token, visitor, argsList, body),
        self(self),
        instance(instance)
    {
        setArity(1, 1);
    }

    ValuablePtr PushbackFunction::call(ValuableQueue& args)
    {
        auto value = args.front();

        instance->pushback(value);

        // Return the value calculated
        return nullptr;
    }

    SizeFunction::SizeFunction(ExprPtr self, std::shared_ptr<Environment> env, const std::string& name, const Token& token, VisitorInterpreter* visitor, std::queue<ExprPtr> argsList, StatementPtr body, std::shared_ptr<ClassInstance> instance) :
        Function(env, name, token, visitor, argsList, body),
        self(self),
        instance(instance)
    {
        setArity(0, 0);
    }

    ValuablePtr SizeFunction::call(ValuableQueue&)
    {
        // Return the size of the current instance
        return std::make_shared<Variable>(ElementType { instance->getSize() });
    }

    EraseFunction::EraseFunction(ExprPtr self, std::shared_ptr<Environment> env, const std::string& name, const Token& token, VisitorInterpreter* visitor, std::queue<ExprPtr> argsList, StatementPtr body, std::shared_ptr<ClassInstance> instance) :
        Function(env, name, token, visitor, argsList, body),
        self(self),
        instance(instance)
    {
        setArity(1, 1);
    }

    ValuablePtr EraseFunction::call(ValuableQueue& args)
    {
        auto value = args.front()->getElement();

        instance->remove(value.toString());

        // Return the size of the current instance
        return nullptr;
    }

    BeginFunction::BeginFunction(ExprPtr self, std::shared_ptr<Environment> env, const std::string& name, const Token& token, VisitorInterpreter* visitor, std::queue<ExprPtr> argsList, StatementPtr body, std::shared_ptr<IteratorInstance> instance) :
        Function(env, name, token, visitor, argsList, body),
        self(self),
        instance(instance)
    {
        setArity(0, 0);
    }

    ValuablePtr BeginFunction::call(ValuableQueue&)
    {
        // Return the iterator instance
        return std::make_shared<Variable>(ElementType { 0 });
    }

    EndFunction::EndFunction(ExprPtr self, std::shared_ptr<Environment> env, const std::string& name, const Token& token, VisitorInterpreter* visitor, std::queue<ExprPtr> argsList, StatementPtr body, std::shared_ptr<IteratorInstance> instance) :
        Function(env, name, token, visitor, argsList, body),
        self(self),
        instance(instance)
    {
        setArity(0, 0);
    }

    ValuablePtr EndFunction::call(ValuableQueue&)
    {
        // Return the iterator instance
        return std::make_shared<Variable>(ElementType { instance->refFields.size() });
    }

    CurrentFunction::CurrentFunction(ExprPtr self, std::shared_ptr<Environment> env, const std::string& name, const Token& token, VisitorInterpreter* visitor, std::queue<ExprPtr> argsList, StatementPtr body, std::shared_ptr<IteratorInstance> instance) :
        Function(env, name, token, visitor, argsList, body),
        self(self),
        instance(instance)
    {
        setArity(0, 0);
    }

    ValuablePtr CurrentFunction::call(ValuableQueue&)
    {
        if (instance->index >= instance->refFields.size())
            return std::make_shared<Variable>(ElementType { instance->refFields.size() });
        
        std::queue<ExprPtr> emptyQueue;

        auto itValue = instance->refFields.at(instance->index);

        auto mapValue = std::make_shared<ClassInstance>(nullptr);

        mapValue->set(Token{TokenType::EXPRESSION, "first", token.line, token.column}, std::make_shared<Variable>(ElementType { itValue.key }));
        mapValue->set(Token{TokenType::EXPRESSION, "second", token.line, token.column}, itValue.value);

        return mapValue;
    }

    NextFunction::NextFunction(ExprPtr self, std::shared_ptr<Environment> env, const std::string& name, const Token& token, VisitorInterpreter* visitor, std::queue<ExprPtr> argsList, StatementPtr body, std::shared_ptr<IteratorInstance> instance) :
        Function(env, name, token, visitor, argsList, body),
        self(self),
        instance(instance)
    {
        setArity(0, 0);
    }

    ValuablePtr NextFunction::call(ValuableQueue&)
    {
        if (instance->index < instance->refFields.size())
            instance->index++;

        return nullptr;
    }

    IteratorFunction::IteratorFunction(ExprPtr self, std::shared_ptr<Environment> env, const std::string& name, const Token& token, VisitorInterpreter* visitor, std::queue<ExprPtr> argsList, StatementPtr body, std::shared_ptr<ClassInstance> instance) :
        Function(env, name, token, visitor, argsList, body),
        self(self),
        instance(instance)
    {
        setArity(0, 0);
    }

    ValuablePtr IteratorFunction::call(ValuableQueue&)
    {
        auto itInstance = std::make_shared<IteratorInstance>(nullptr, instance);

        auto itToken = token;
        itToken.text = "Iterator Function";

        std::queue<ExprPtr> emptyQueue;

        auto current = std::make_shared<CurrentFunction>(self, env, "It Current", itToken, visitor, emptyQueue, nullptr, itInstance);
        auto begin   = std::make_shared<BeginFunction>(self, env, "It Begin", itToken, visitor, emptyQueue, nullptr, itInstance);
        auto next    = std::make_shared<NextFunction>(self, env, "It Next", itToken, visitor, emptyQueue, nullptr, itInstance);
        auto end     = std::make_shared<EndFunction>(self, env, "It End", itToken, visitor, emptyQueue, nullptr, itInstance);

        std::unordered_map<std::string, std::shared_ptr<Function>> methods;

        methods["current"] = current;
        methods["begin"] = begin;
        methods["next"] = next;
        methods["end"] = end;

        itInstance->setMethods(methods);

        // Return the value calculated
        return itInstance;
    }

}
