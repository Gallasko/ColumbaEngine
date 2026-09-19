#include "stdafx.h"

#include "statement.h"
#include "interpreter.h"

namespace pg
{

    void ExpressionStatement::accept(Visitor* visitor)
    {
        visitor->visitStatement(this);
    }

    void VariableStatement::accept(Visitor* visitor)
    {
        visitor->visitStatement(this);
    }

    void FunctionStatement::accept(Visitor* visitor)
    {
        visitor->visitStatement(this);
    }

    void ClassStatement::accept(Visitor* visitor)
    {
        visitor->visitStatement(this);
    }

    void BlockStatement::accept(Visitor* visitor)
    {
        visitor->visitStatement(this);
    }

    void IfStatement::accept(Visitor* visitor)
    {
        visitor->visitStatement(this);
    }

    void WhileStatement::accept(Visitor* visitor)
    {
        visitor->visitStatement(this);
    }

    void ReturnStatement::accept(Visitor* visitor)
    {
        visitor->visitStatement(this);
    }

    void ImportStatement::accept(Visitor* visitor)
    {
        visitor->visitStatement(this);
    }

    void ForStatement::accept(Visitor* visitor)
    {
        visitor->visitStatement(this);
    }

    void ForInStatement::accept(Visitor* visitor)
    {
        visitor->visitStatement(this);
    }

    void BreakStatement::accept(Visitor* visitor)
    {
        visitor->visitStatement(this);
    }

    void ContinueStatement::accept(Visitor* visitor)
    {
        visitor->visitStatement(this);
    }

    void DPrintStatement::accept(Visitor* visitor)
    {
        visitor->visitStatement(this);
    }

}
