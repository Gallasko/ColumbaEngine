#include "stdafx.h"

#include "expression.h"
#include "valuable.h"
#include "interpreter.h"

namespace pg
{
    std::shared_ptr<Valuable> BinaryExpression::accept(Visitor* visitor)
    {
        return visitor->visit(this);
    }

    std::shared_ptr<Valuable> LogicExpression::accept(Visitor* visitor)
    {
        return visitor->visit(this);
    }

    std::shared_ptr<Valuable> UnaryExpression::accept(Visitor* visitor)
    {
        return visitor->visit(this);
    }

    std::shared_ptr<Valuable> PreFixExpression::accept(Visitor* visitor)
    {
        return visitor->visit(this);
    }

    std::shared_ptr<Valuable> PostFixExpression::accept(Visitor* visitor)
    {
        return visitor->visit(this);
    }

    std::shared_ptr<Valuable> CompoundAtom::accept(Visitor* visitor)
    {
        return visitor->visit(this);
    }

    std::shared_ptr<Valuable> Atom::accept(Visitor* visitor)
    {
        return visitor->visit(this);
    }

    std::shared_ptr<Valuable> List::accept(Visitor* visitor)
    {
        return visitor->visit(this);
    }

    std::shared_ptr<Valuable> This::accept(Visitor* visitor)
    {
        return visitor->visit(this);
    }

    std::shared_ptr<Valuable> Var::accept(Visitor* visitor)
    {
        return visitor->visit(this);
    }

    std::shared_ptr<Valuable> Assign::accept(Visitor* visitor)
    {
        return visitor->visit(this);
    }

    std::shared_ptr<Valuable> CallExpression::accept(Visitor* visitor)
    {
        return visitor->visit(this);
    }

    std::shared_ptr<Valuable> Get::accept(Visitor* visitor)
    {
        return visitor->visit(this);
    }

    std::shared_ptr<Valuable> Set::accept(Visitor* visitor)
    {
        return visitor->visit(this);
    }

}
