#pragma once

/**
 * @file valuable.h
 * @author Gallasko
 * @brief Definition of valuables objects
 * @version 1.0
 * @date 2022-04-12
 *
 * @copyright Copyright (c) 2022
 *
 */

#include <queue>
#include <vector>
#include <memory>
#include <string>
#include <stdexcept>
#include <unordered_map>

#include "token.h"
#include "Memory/elementtype.h"

namespace pg
{

    /**
     * @class RuntimeException
     *
     * Class to handle exceptions thrown during runtime execution.
     * Any kinds of error happening in the interpreter will be reported as RuntimeException
     *
     * To get the message of the exception the catch statement must read the virtual function 'what()' inherited by std::runtime_error
     * This message contains the type of the exception and in which line it appears
     */
    class RuntimeException : public std::runtime_error
    {
    public:
        /**
         * @brief Construct a new Runtime Exception object
         *
         * @param token   The token of the exception, it contains data about the position of the exception in the script file
         * @param message The actual message of the exception
         */
        RuntimeException(const Token& token, const std::string& message) noexcept : std::runtime_error(createErrorMessage(token, message)) {}
        virtual ~RuntimeException() = default;

        /** Function used to create the message of the exception */
        std::string createErrorMessage(const Token& token, const std::string& message) const noexcept;
    };

    /**
     * @class Valuable
     * @brief An abstract class representing all the different types of compound value the interpreter can use
     */
    class Valuable
    {
    public:
        virtual ~Valuable() {}

        /**
         * @brief Get the Value object
         *
         * @return An ElementType value of the underlying valuable object
         *
         * A pure virtual methode that need to be implemented by all the different valuable objects,
         * used to retrieve the value of the object when no arguments are passed to the valuable
         */
        virtual const ElementType& getElement() const = 0;

        /**
         * @brief Get the Value object
         *
         * @return The underlying valuable object
         *
         * A pure virtual methode that need to be implemented by all the different valuable objects,
         * used to retrieve the valuable of the object when no arguments are passed to the valuable
         */
        virtual std::shared_ptr<Valuable> getValue() const = 0;

        /**
         * @brief Get the Value object
         *
         * @return The underlying valuable object
         *
         * A pure virtual methode that need to be implemented by all the different valuable objects,
         * used to retrieve the valuable of the object when a list of arguments is passed to the valuable
         */
        virtual std::shared_ptr<Valuable> getValue(std::queue<std::shared_ptr<Valuable>>& args) = 0;

        /**
         * @brief Get the Type of Valuable object
         *
         * @return The name of the type of the valuable object
         *
         * A pure virtual methode that need to be implemented by all the different valuable objects,
         * used to retrieve the type of the Valuable object
         */
        virtual std::string getType() const = 0;
    };

    // Type definition

    /** Type definition for a pointer to a Valuable */
    typedef std::shared_ptr<Valuable> ValuablePtr;

    /** Type definition for a queue of Valuable */
    typedef std::queue<std::shared_ptr<Valuable>> ValuableQueue;

    /**
     * @class Variable
     * @brief Represents a simple variable object as a Valuable
     *
     * Hold data of a single variable and can be accessed like a Valuable object
     *
     * @see Valuable
     */
    class Variable : public Valuable
    {
    public:
        /**
         * @brief Construct a new Variable object
         *
         * @param value The value of the variable
         */
        Variable(const ElementType& value) : value(value) {}

        /**
         * @brief Construct a copy of a Variable object
         *
         * @param other The variable to be copied
         */
        Variable(const Variable& other) : value(other.value) {}

        /**
         * @brief Get the Element object
         *
         * @return A reference to the underlying value
         *
         * Override of the getElement() method of Valuable
         */
        virtual const ElementType& getElement() const override { return value; }

        /**
         * @brief Get the Value object
         *
         * @return A variable valuable of the value stored
         *
         * Override of the getValue() method of Valuable
         */
        virtual std::shared_ptr<Valuable> getValue() const override { return std::make_shared<Variable>(value); }

        /**
         * @brief Get the Value object
         *
         * @return An exception because this methode shouldn't be called on a variable
         *
         * Override of the getValue(std::queue<ElementType>&) method of Valuable
         */
        virtual std::shared_ptr<Valuable> getValue(std::queue<std::shared_ptr<Valuable>>&) override { throw std::runtime_error("No argument expected for a variable"); };

        /**
         * @brief Get the Type of Valuable object
         *
         * @return The name of the type of the valuable object
         *
         * Override of the getType() method of Valuable
         */
        virtual std::string getType() const override { return "Variable"; }

    private:
        /** The variable stored by this Valuable */
        ElementType value;
    };

    template<typename T>
    inline std::shared_ptr<Variable> makeVar(const T& value) { return std::make_shared<Variable>(ElementType { value }); }

    /**
     * @struct Arity
     * @brief A structure that holds the number of arguments required for a function/method call
     */
    struct Arity
    {
        /** Minimun number of arguments required */
        int min = 0;

        /** Maximum number of arguments acceptable */
        int max = 0;
    };

    /// Forward Declaration
    class VisitorInterpreter;
    class VisitorReference;
    class Environment;

    class Expression;
    typedef std::shared_ptr<Expression> ExprPtr;

    class Statement;
    typedef std::shared_ptr<Statement> StatementPtr;

    class ClassInstance;

    /**
     * @class Function
     * @brief Represents a function object as a Valuable
     *
     * Hold data of a single variable and can be accessed like a Valuable object
     *
     * @see Valuable
     */
    class Function : public Valuable
    {
    public:
        /** Construct a new Function Valuable object */
        Function(std::shared_ptr<Environment> env, const std::string& name, const Token& token, VisitorInterpreter* visitor, std::queue<ExprPtr> argsList, StatementPtr body);

        /** Construct a standalone copy of Function object */
        Function(std::shared_ptr<VisitorReference> visitorRef, std::shared_ptr<Function> base);

        /** Construct a copy of Function object */
        Function(const Function& other);

        virtual ~Function() {}

        /**
         * @brief Get the Element object
         *
         * @return A reference to the underlying value
         *
         * Override of the getElement() method of Valuable
         */
        virtual const ElementType& getElement() const override { return name; }

        /** Return a reference to the function name if it is not in a call expression */
        virtual std::shared_ptr<Valuable> getValue() const override;

        /** Return a reference to the value obtained from the function call */
        virtual std::shared_ptr<Valuable> getValue(std::queue<std::shared_ptr<Valuable>>& args) override;

        /**
         * @brief Get the Type of Valuable object
         *
         * @return The name of the type of the valuable object
         *
         * Override of the getType() method of Valuable
         */
        virtual std::string getType() const override { return "Function"; }

        virtual std::shared_ptr<Function> bind(std::shared_ptr<ClassInstance> instance);

        inline std::shared_ptr<Environment> getEnv() const { return env; }

        inline Token getToken() const { return token; }

        inline VisitorInterpreter* getVisitor() const { return visitor; }

    protected:
        /** The piece of code to be executed when a function call is made */
        virtual std::shared_ptr<Valuable> call(std::queue<std::shared_ptr<Valuable>>& args);

        /**
         * @brief Helper function to set the arity of the function
         *
         * @param min Minimun number of arguments required (if set to -1 then the function takes unlimited arguments)
         * @param max Maximum number of arguments acceptable (if set to -1 then the function takes unlimited arguments)
         */
        void setArity(int min, int max) { this->arity.min = min; this->arity.max = max; }

        /**
         * @brief Helper function to set the arity of the function
         *
         * @param arity Arity struct holding the arity of the function
         */
        void setArity(const Arity& arity) { this->arity = arity; }

        /** A pointer to the parent environment object*/
        std::shared_ptr<Environment> env;

        /** The name of the function */
        ElementType name;

        /** The token of the function (for exception purposes only) */
        Token token;

        /** A pointer to the visitor object to be able to execute the body of the function */
        VisitorInterpreter* visitor;

    private:
        std::queue<ExprPtr> argsList;

        /** The list of the parameter name to be pushed in scope during a function call */
        std::vector<std::string> paramList;

        /** The body of the function */
        std::queue<StatementPtr> body;

        /** The arity of the function */
        Arity arity;
    };

    class Class : public Valuable
    {
    public:
        /** Construct a new Function Valuable object */
        Class(std::shared_ptr<Environment> env, const std::string& name, const Token& token, VisitorInterpreter* visitor, const std::unordered_map<std::string, std::shared_ptr<Function>>& methods);
        /** Construct a copy of Function object */
        Class(const Class& other);

        /**
         * @brief Get the Element object
         *
         * @return A reference to the underlying value
         *
         * Override of the getElement() method of Valuable
         */
        virtual const ElementType& getElement() const override { return name; }

        /** Return a reference to the class name if it is not in a call expression */
        virtual std::shared_ptr<Valuable> getValue() const override;

        /** Return a reference to the ClassInstance obtained from the class call */
        virtual std::shared_ptr<Valuable> getValue(std::queue<std::shared_ptr<Valuable>>& args) override;

        /**
         * @brief Get the Type of Valuable object
         *
         * @return The name of the type of the valuable object
         *
         * Override of the getType() method of Valuable
         */
        virtual std::string getType() const override { return "Class"; }

        std::shared_ptr<Function> findMethodByName(const std::string& name) const;

    private:
        /** A pointer to the parent environment object*/
        std::shared_ptr<Environment> env;

        /** The name of the function */
        ElementType name;

        /** The token of the function (for exception purposes only) */
        Token token;

        /** A pointer to the visitor object to be able to execute the body of the class */
        VisitorInterpreter* visitor;

        /** A Map of all the bound methods of the class */
        std::unordered_map<std::string, std::shared_ptr<Function>> methods;
    };

    class IteratorInstance;

    class ClassInstance : public Valuable
    {
    friend class IteratorInstance;
    public:
        struct Field
        {
            std::string key;

            std::shared_ptr<Valuable> value;

            Field(const std::string& key, std::shared_ptr<Valuable> value) : key(key), value(value) {}

            bool operator==(const std::string& match) const { return match == key; }
        };

    public:
        /** Construct a new Function Valuable object */
        ClassInstance(const Class *klass);
        /** Construct a copy of Function object */
        ClassInstance(const ClassInstance& other);

        void setMethods(const std::unordered_map<std::string, std::shared_ptr<Function>>& methods);

        /**
         * @brief Get the Element object
         *
         * @return A reference to the underlying value
         *
         * Override of the getElement() method of Valuable
         */
        virtual const ElementType& getElement() const override;

        /** Return a reference to the class name if it is not in a call expression */
        virtual std::shared_ptr<Valuable> getValue() const override;

        /** Return a reference to the ClassInstance obtained from the class call */
        virtual std::shared_ptr<Valuable> getValue(std::queue<std::shared_ptr<Valuable>>& args) override;

        /**
         * @brief Get the Type of Valuable object
         *
         * @return The name of the type of the valuable object
         *
         * Override of the getType() method of Valuable
         */
        virtual std::string getType() const override { return "ClassInstance"; }

        inline size_t getSize() const noexcept { return fields.size(); }

        std::shared_ptr<Valuable> get(const Token& token) const;
        void set(const Token& token, std::shared_ptr<Valuable> value);

        inline const std::unordered_map<std::string, std::shared_ptr<Function>>& getMethods() const { return boundMethods; }
        inline const std::vector<Field>& getFields() const { return fields; }

        void pushback(std::shared_ptr<Valuable> value);
        void remove(const std::string &key);

    protected:
        std::shared_ptr<Function> findMethod(const std::string& name) const;

        /** A pointer to the parent class object*/
        const Class *klass;

        ElementType name;

        std::unordered_map<std::string, std::shared_ptr<Function>> boundMethods;
        std::vector<Field> fields;
    };

    class IteratorInstance : public ClassInstance
    {
    public:
        IteratorInstance(const Class *klass, std::shared_ptr<ClassInstance> instance) : ClassInstance(klass), refFields(instance->fields) {}
        virtual std::string getType() const override { return "IteratorInstance"; }

        std::vector<Field>& refFields;

        size_t index = 0;
    };

    struct ListElement;

    class AtFunction : public Function
    {
    public:
        AtFunction(ExprPtr self, std::shared_ptr<Environment> env, const std::string& name, const Token& token, VisitorInterpreter* visitor, std::queue<ExprPtr> argsList, StatementPtr body, std::shared_ptr<ClassInstance> instance = nullptr);

        virtual std::shared_ptr<Function> bind(std::shared_ptr<ClassInstance> instance) override;

        virtual ValuablePtr call(ValuableQueue& args) override;

        virtual std::string getType() const override { return "List"; }

    private:
        ExprPtr self;

        std::shared_ptr<ClassInstance> instance = nullptr;
    };

    class SetFunction : public Function
    {
    public:
        SetFunction(ExprPtr self, std::shared_ptr<Environment> env, const std::string& name, const Token& token, VisitorInterpreter* visitor, std::queue<ExprPtr> argsList, StatementPtr body, std::shared_ptr<ClassInstance> instance = nullptr);

        virtual std::shared_ptr<Function> bind(std::shared_ptr<ClassInstance> instance) override;

        virtual ValuablePtr call(ValuableQueue& args) override;

        virtual std::string getType() const override { return "List"; }

    private:
        ExprPtr self;

        std::shared_ptr<ClassInstance> instance = nullptr;
    };

    class PushbackFunction : public Function
    {
    public:
        PushbackFunction(ExprPtr self, std::shared_ptr<Environment> env, const std::string& name, const Token& token, VisitorInterpreter* visitor, std::queue<ExprPtr> argsList, StatementPtr body, std::shared_ptr<ClassInstance> instance);

        virtual ValuablePtr call(ValuableQueue& args) override;

        virtual std::string getType() const override { return "List"; }

    private:
        ExprPtr self;

        std::shared_ptr<ClassInstance> instance;
    };

    class SizeFunction : public Function
    {
    public:
        SizeFunction(ExprPtr self, std::shared_ptr<Environment> env, const std::string& name, const Token& token, VisitorInterpreter* visitor, std::queue<ExprPtr> argsList, StatementPtr body, std::shared_ptr<ClassInstance> instance);

        virtual ValuablePtr call(ValuableQueue& args) override;

        virtual std::string getType() const override { return "List"; }

    private:
        ExprPtr self;

        std::shared_ptr<ClassInstance> instance;
    };

    class EraseFunction : public Function
    {
    public:
        EraseFunction(ExprPtr self, std::shared_ptr<Environment> env, const std::string& name, const Token& token, VisitorInterpreter* visitor, std::queue<ExprPtr> argsList, StatementPtr body, std::shared_ptr<ClassInstance> instance);

        virtual ValuablePtr call(ValuableQueue& args) override;

        virtual std::string getType() const override { return "List"; }

    private:
        ExprPtr self;

        std::shared_ptr<ClassInstance> instance;
    };

    class BeginFunction : public Function
    {
    public:
        BeginFunction(ExprPtr self, std::shared_ptr<Environment> env, const std::string& name, const Token& token, VisitorInterpreter* visitor, std::queue<ExprPtr> argsList, StatementPtr body, std::shared_ptr<IteratorInstance> instance);

        virtual ValuablePtr call(ValuableQueue& args) override;

        virtual std::string getType() const override { return "List"; }

    private:
        ExprPtr self;

        std::shared_ptr<IteratorInstance> instance;
    };

    class EndFunction : public Function
    {
    public:
        EndFunction(ExprPtr self, std::shared_ptr<Environment> env, const std::string& name, const Token& token, VisitorInterpreter* visitor, std::queue<ExprPtr> argsList, StatementPtr body, std::shared_ptr<IteratorInstance> instance);

        virtual ValuablePtr call(ValuableQueue& args) override;

        virtual std::string getType() const override { return "List"; }

    private:
        ExprPtr self;

        std::shared_ptr<IteratorInstance> instance;
    };

    class CurrentFunction : public Function
    {
    public:
        CurrentFunction(ExprPtr self, std::shared_ptr<Environment> env, const std::string& name, const Token& token, VisitorInterpreter* visitor, std::queue<ExprPtr> argsList, StatementPtr body, std::shared_ptr<IteratorInstance> instance);

        virtual ValuablePtr call(ValuableQueue& args) override;

        virtual std::string getType() const override { return "List"; }

    private:
        ExprPtr self;

        std::shared_ptr<IteratorInstance> instance;
    };

    class NextFunction : public Function
    {
    public:
        NextFunction(ExprPtr self, std::shared_ptr<Environment> env, const std::string& name, const Token& token, VisitorInterpreter* visitor, std::queue<ExprPtr> argsList, StatementPtr body, std::shared_ptr<IteratorInstance> instance);

        virtual ValuablePtr call(ValuableQueue& args) override;

        virtual std::string getType() const override { return "List"; }

    private:
        ExprPtr self;

        std::shared_ptr<IteratorInstance> instance;
    };

    class IteratorFunction : public Function
    {
    public:
        IteratorFunction(ExprPtr self, std::shared_ptr<Environment> env, const std::string& name, const Token& token, VisitorInterpreter* visitor, std::queue<ExprPtr> argsList, StatementPtr body, std::shared_ptr<ClassInstance> instance);

        virtual ValuablePtr call(ValuableQueue& args) override;

        virtual std::string getType() const override { return "List"; }

    private:
        ExprPtr self;

        std::shared_ptr<ClassInstance> instance;
    };

    // TODO create it() function -> have 0 argument and return an iterator object !

}
