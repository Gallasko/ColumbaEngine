#include "stdafx.h"

#include "expression.h"
#include "visitor.h"

namespace pg
{
    void BinaryExpression::accept(Visitor* visitor)
    {
        visitor->visit(this);
    }

    void LogicExpression::accept(Visitor* visitor)
    {
        visitor->visit(this);
    }

    void UnaryExpression::accept(Visitor* visitor)
    {
        visitor->visit(this);
    }

    void PreFixExpression::accept(Visitor* visitor)
    {
        visitor->visit(this);
    }

    void PostFixExpression::accept(Visitor* visitor)
    {
        visitor->visit(this);
    }

    void CompoundAtom::accept(Visitor* visitor)
    {
        visitor->visit(this);
    }

    void Atom::accept(Visitor* visitor)
    {
        visitor->visit(this);
    }

    void List::accept(Visitor* visitor)
    {
        visitor->visit(this);
    }

    void This::accept(Visitor* visitor)
    {
        visitor->visit(this);
    }

    void Var::accept(Visitor* visitor)
    {
        visitor->visit(this);
    }

    void Assign::accept(Visitor* visitor)
    {
        visitor->visit(this);
    }

    void CallExpression::accept(Visitor* visitor)
    {
        visitor->visit(this);
    }

    void Get::accept(Visitor* visitor)
    {
        visitor->visit(this);
    }

    void Set::accept(Visitor* visitor)
    {
        visitor->visit(this);
    }

    void AnonymousFunction::accept(Visitor* visitor)
    {
        visitor->visit(this);
    }

    void IndexGet::accept(Visitor* visitor)
    {
        visitor->visit(this);
    }

    void IndexSet::accept(Visitor* visitor)
    {
        visitor->visit(this);
    }

}
