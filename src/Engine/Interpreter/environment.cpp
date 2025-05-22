/**
 * @file environment.cpp
 * @author Pigeon Codeur
 * @brief Implementation of the environment object
 * @version 0.1
 * @date 2022-04-12
 *
 * @copyright Copyright (c) 2022
 *
 */
#include "stdafx.h"

#include "environment.h"

namespace pg
{
    /**
     * @brief Declare a new valuable in the current scope
     *
     * This function is used when a declaration expression is encountered in the interpreter.
     *
     * @param name  The name of the Valuable to be declared
     * @param value The value of the Valuable to be declared
     */
    void Environment::declareValue(const std::string& name, std::shared_ptr<Valuable> value)
    {
        variableTable[name] = value;
    }

    /**
     * @brief Assign a new value to a valuable in scope
     *
     * This function is used when a assignement expression is encountered in the interpreter.
     * It assigns a new value to the variable name in scope.
     * If the variable name is not declared yet this function throw a runtime exception.
     *
     * @param name  The name of the Valuable to be assigned
     * @param token The token of the Valuable to be assigned (for exception purposes)
     * @param value The value of the Valuable to be assigned
     */
    void Environment::assignValue(const std::string& name, const Token& token, std::shared_ptr<Valuable> value)
    {
        const auto it = variableTable.find(name);
        if(it != variableTable.end())
        {
            variableTable[name] = value;
            return;
        }

        if(enclosing) return enclosing->assignValue(name, token, value);

        throw RuntimeException(token, "Valuable '" + name + "' must be declared first before assignment.");
    }

    /**
     * @brief Get the Value object
     *
     * A simple getter to get a value from a Valuable object
     *
     * @param name  The name of the Valuable to retrieve
     * @param token The token of the Valuable to retrieve (for exception purposes)
     *
     * @return A reference to the Valuable object in the current scope if founded, otherwise it throw a runtime exception
     */
    std::shared_ptr<Valuable> Environment::getValue(const std::string& name, const Token& token) const
    {
        const auto it = variableTable.find(name);
        if(it != variableTable.end()) return variableTable.at(name);

        if(enclosing) return enclosing->getValue(name, token);

        throw RuntimeException(token, "Undefined Valuable '" + name + "'.");
    }

}
