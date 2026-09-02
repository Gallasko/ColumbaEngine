Script Language API Reference
==============================

This document provides a comprehensive reference for the PgEngine script language syntax and built-in functionality.

Lexical Structure
-----------------

Comments
~~~~~~~~

Single-line comments start with ``//``::

    // This is a comment
    var x = 5;  // Comment after code

Block comments are not currently supported.

Identifiers
~~~~~~~~~~~

Identifiers must start with a letter or underscore, followed by letters, digits, or underscores::

    var myVariable;
    var _private;
    var counter123;

Keywords
~~~~~~~~

Reserved keywords::

    and       class     else      false     for
    fun       if        import    in        init
    not       or        return    this      true
    var       while

Data Types
----------

Number
~~~~~~

Numbers can be integers or floating-point values::

    var integer = 42;
    var negative = -10;
    var float = 3.14;
    var scientific = 1.5e10;

**Operations:**

* Arithmetic: ``+``, ``-``, ``*``, ``/``, ``%``
* Comparison: ``==``, ``!=``, ``<``, ``>``, ``<=``, ``>=``
* Unary: ``-x`` (negation)

String
~~~~~~

Strings are enclosed in double quotes::

    var str = "hello";
    var empty = "";
    var multiWord = "hello world";

**Operations:**

* Concatenation: ``"hello" + " " + "world"``
* Indexing: ``str[0]`` (zero-based)
* Negative indexing: ``str[-1]`` (last character)
* Length: ``strlen(str)`` (requires string module)

**Escape sequences:**

* ``\n`` - newline
* ``\t`` - tab
* ``\"`` - double quote
* ``\\`` - backslash

Boolean
~~~~~~~

Boolean literals::

    var isTrue = true;
    var isFalse = false;

**Operations:**

* Logical: ``and``, ``or``, ``not``
* Comparison: ``==``, ``!=``

Truthiness:

* ``false`` is falsy
* All other values are truthy

Table
~~~~~

Tables are associative arrays that can store both indexed and keyed values.

**Array syntax:**

::

    var numbers = [1, 2, 3, 4, 5];
    print(numbers[0]);  // 1
    print(numbers[4]);  // 5

**Dictionary syntax:**

::

    var person = [name: "Alice", age: 30];
    print(person["name"]);  // Alice
    print(person["age"]);   // 30

**Mixed syntax:**

::

    var mixed = [10, name: "Bob", 20, age: 25];
    print(mixed[0]);       // 10
    print(mixed["name"]);  // Bob
    print(mixed[1]);       // 20

**Modification:**

::

    var table = [a: 1, b: 2];
    table["a"] = 10;           // Modify existing
    table["c"] = 3;            // Add new key
    table[0] = 100;            // Modify by index

**Empty tables:**

::

    var empty = [];

Variables
---------

Declaration and Assignment
~~~~~~~~~~~~~~~~~~~~~~~~~~

Variables are declared with ``var``::

    var x;              // Declared but uninitialized (nil)
    var y = 10;         // Declared and initialized
    var name = "Alice"; // String variable

Assignment::

    x = 5;              // Assign value
    y = x + 10;         // Expression assignment

**Compound assignment:**

::

    x += 5;   // Equivalent to: x = x + 5
    x -= 3;   // Equivalent to: x = x - 3

**Increment and decrement:**

::

    ++x;      // Pre-increment: increment then return
    x++;      // Post-increment: return then increment
    --x;      // Pre-decrement
    x--;      // Post-decrement

Scope
~~~~~

Variables have lexical scope:

* **Global scope**: Declared at the top level
* **Local scope**: Declared inside functions or blocks
* **Block scope**: Variables declared in ``if``, ``for``, ``while`` blocks

Example::

    var global = "global";

    fun test()
    {
        var local = "local";

        if (true)
        {
            var block = "block";
            print(global);  // Accessible
            print(local);   // Accessible
            print(block);   // Accessible
        }
        // print(block);  // Error: not accessible
    }

Operators
---------

Arithmetic Operators
~~~~~~~~~~~~~~~~~~~~

.. list-table::
   :header-rows: 1
   :widths: 15 40 25

   * - Operator
     - Description
     - Example
   * - ``+``
     - Addition
     - ``5 + 3``
   * - ``-``
     - Subtraction
     - ``5 - 3``
   * - ``*``
     - Multiplication
     - ``5 * 3``
   * - ``/``
     - Division
     - ``10 / 2``
   * - ``%``
     - Modulo
     - ``10 % 3``
   * - ``-``
     - Unary negation
     - ``-x``

Comparison Operators
~~~~~~~~~~~~~~~~~~~~

.. list-table::
   :header-rows: 1
   :widths: 15 40 25

   * - Operator
     - Description
     - Example
   * - ``==``
     - Equal to
     - ``x == 5``
   * - ``!=``
     - Not equal to
     - ``x != 5``
   * - ``<``
     - Less than
     - ``x < 5``
   * - ``>``
     - Greater than
     - ``x > 5``
   * - ``<=``
     - Less than or equal
     - ``x <= 5``
   * - ``>=``
     - Greater or equal
     - ``x >= 5``

Logical Operators
~~~~~~~~~~~~~~~~~

.. list-table::
   :header-rows: 1
   :widths: 15 40 30

   * - Operator
     - Description
     - Example
   * - ``and``
     - Logical AND
     - ``x > 0 and y > 0``
   * - ``or``
     - Logical OR
     - ``x == 0 or y == 0``
   * - ``not``
     - Logical NOT
     - ``not x``

Short-circuit evaluation: ``and`` and ``or`` use short-circuit evaluation.

Operator Precedence
~~~~~~~~~~~~~~~~~~~

From highest to lowest precedence:

1. Parentheses: ``()``
2. Unary: ``-``, ``not``, ``++``, ``--``
3. Multiplicative: ``*``, ``/``, ``%``
4. Additive: ``+``, ``-``
5. Comparison: ``<``, ``>``, ``<=``, ``>=``
6. Equality: ``==``, ``!=``
7. Logical AND: ``and``
8. Logical OR: ``or``

Control Flow
------------

If Statement
~~~~~~~~~~~~

**Basic if:**

::

    if (condition)
    {
        // code
    }

**If-else:**

::

    if (condition)
    {
        // code when true
    }
    else
    {
        // code when false
    }

**If-else if-else chain:**

::

    if (condition1)
    {
        // code
    }
    else if (condition2)
    {
        // code
    }
    else if (condition3)
    {
        // code
    }
    else
    {
        // default code
    }

While Loop
~~~~~~~~~~

::

    while (condition)
    {
        // code
        // remember to modify condition to avoid infinite loop
    }

Example::

    var i = 0;
    while (i < 5)
    {
        print(i);
        i = i + 1;
    }

For Loop
~~~~~~~~

**Traditional for loop:**

::

    for (initialization; condition; increment)
    {
        // code
    }

Example::

    for (var i = 0; i < 5; i++)
    {
        print(i);
    }

**For-in loop** (iterate over tables):

::

    for (var key : table)
    {
        // key contains the current key
    }

Example::

    var person = [name: "Alice", age: 30];
    // To access the keys
    for (var key : person)
    {
        print(key);  // Prints: name, age
    }

    // To access the values
    for (var key : person)
    {
        print(person[key]);  // Prints: Alice, 30
    }

Array iteration::

    var numbers = [1, 2, 3, 4, 5];
    for (var value : numbers)
    {
        print(value);
    }

Functions
---------

Function Declaration
~~~~~~~~~~~~~~~~~~~~

::

    fun functionName(param1, param2, ...)
    {
        // function body
        return value;  // optional
    }

Example::

    fun add(a, b)
    {
        return a + b;
    }

    var result = add(5, 3);
    print(result);  // 8

Functions without explicit return value return ``0``.

First-Class Functions
~~~~~~~~~~~~~~~~~~~~~

Functions can be assigned to variables and passed as arguments::

    fun greet(name)
    {
        return "Hello, " + name;
    }

    var greeter = greet;
    print(greeter("World"));  // Hello, World

Anonymous Functions
~~~~~~~~~~~~~~~~~~~

Functions can be defined without a name::

    var square = fun(x)
    {
        return x * x;
    };

Closures
~~~~~~~~

Functions can capture variables from their enclosing scope::

    fun makeCounter()
    {
        var count = 0;

        fun increment()
        {
            count = count + 1;
            return count;
        }

        return increment;
    }

    var counter = makeCounter();
    print(counter());  // 1
    print(counter());  // 2
    print(counter());  // 3

Nested closures::

    fun outer()
    {
        var a = 1;
        fun middle()
        {
            var b = 2;
            fun inner()
            {
                return a + b;  // Captures both a and b
            }
            return inner;
        }
        return middle;
    }

    var mid = outer();
    var inn = mid();
    print(inn());  // 3

Classes
-------

Class Declaration
~~~~~~~~~~~~~~~~~

::

    class ClassName
    {
        init(param1, param2)
        {
            this.property1 = param1;
            this.property2 = param2;
        }

        methodName()
        {
            // method body
        }
    }

Constructor
~~~~~~~~~~~

The ``init`` method is the constructor, called when creating instances::

    class Point
    {
        init(x, y)
        {
            this.x = x;
            this.y = y;
        }
    }

    var point = Point(10, 20);

Instance Creation
~~~~~~~~~~~~~~~~~

Create instances by calling the class as a function::

    var instance = ClassName(arg1, arg2);

Instance Methods
~~~~~~~~~~~~~~~~

Methods defined in a class have access to ``this``::

    class Rectangle
    {
        init(width, height)
        {
            this.width = width;
            this.height = height;
        }

        area()
        {
            return this.width * this.height;
        }

        perimeter()
        {
            return 2 * (this.width + this.height);
        }
    }

    var rect = Rectangle(5, 10);
    print(rect.area());       // 50
    print(rect.perimeter());  // 30

Properties
~~~~~~~~~~

Access and modify properties using dot notation::

    class Person
    {
        init(name, age)
        {
            this.name = name;
            this.age = age;
        }
    }

    var person = Person("Alice", 30);
    print(person.name);  // Alice

    person.age = 31;        // Modify property
    person.city = "NYC";    // Add new property

Method Binding
~~~~~~~~~~~~~~

Methods maintain their binding to the instance::

    class Counter
    {
        init()
        {
            this.count = 0;
        }

        increment()
        {
            this.count = this.count + 1;
        }
    }

    var counter = Counter();
    var incr = counter.increment;
    incr();  // Still bound to counter instance

Module System
-------------

Import Statement
~~~~~~~~~~~~~~~~

Import modules using the ``import`` keyword::

    import "module_path"

Example::

    import "utils/helper"
    import "math"

The import path is relative to the script directory or uses registered module paths.

Native Modules
~~~~~~~~~~~~~~

Math Module
^^^^^^^^^^^

Import: ``import "math"``

Provides mathematical functions (specific functions depend on implementation).

File Module
^^^^^^^^^^^

Import: ``import "file"``

Functions:

* ``readFile(path)`` - Read file contents as string
* Returns file content or error

Example::

    import "file"

    var content = readFile("data.txt");
    print(content);

String Module
^^^^^^^^^^^^^

Import: ``import "string"``

Functions:

* ``strlen(str)`` - Get string length
* ``splitLines(str)`` - Split string into lines
* ``toInt(str)`` - Convert string to integer

Example::

    import "string"

    var text = "Hello\nWorld";
    var lines = splitLines(text);
    for (var i : lines) {
        print(lines[i]);
    }

Algorithm Module
^^^^^^^^^^^^^^^^

Import: ``import "algorithm"``

Provides common algorithms and data structure operations (specific functions depend on implementation).

Creating Custom Modules
~~~~~~~~~~~~~~~~~~~~~~~~

Create a ``.pg`` file with functions and variables::

    // mymodule.pg
    fun helper(x)
    {
        return x * 2;
    }

    var constant = 42;

Import and use in another script::

    import "mymodule"

    var result = helper(5);    // 10
    print(constant);        // 42

Built-in Functions
------------------

print()
~~~~~~~~~~

Print values for debugging::

    print(value)

Accepts any value type (numbers, strings, booleans, tables, functions, classes).

Example::

    print(42);
    print("Hello");
    print([1, 2, 3]);
    print(true);

__toString()
~~~~~~~~~~~~

Convert a value to string representation (when available)::

    var str = __toString(42);

Best Practices
--------------

Naming Conventions
~~~~~~~~~~~~~~~~~~

* Use camelCase for variable and function names: ``myVariable``, ``calculateTotal()``
* Use PascalCase for class names: ``MyClass``, ``CoffeeMaker``
* Prefix internal/helper functions with underscore: ``_helperFunction()``

Code Organization
~~~~~~~~~~~~~~~~~

* Group related functions into modules
* Keep functions small and focused
* Use meaningful variable and function names
* Add comments for complex logic

Performance Tips
~~~~~~~~~~~~~~~~

* Compile scripts to ``.pgc`` bytecode for faster loading
* Minimize global variables
* Use local variables when possible
* Avoid deep closure chains in hot paths

Error Handling
~~~~~~~~~~~~~~

* Check for ``nil`` values before use
* Validate function arguments
* Use defensive programming techniques

Example Programs
----------------

Fibonacci Sequence
~~~~~~~~~~~~~~~~~~

::

    fun fibonacci(n)
    {
        if (n <= 1)
        {
            return n;
        }

        return fibonacci(n - 1) + fibonacci(n - 2);
    }

    for (var i = 0; i < 10; i++)
    {
        print(fibonacci(i));
    }

File Processing
~~~~~~~~~~~~~~~

::

    import "file"
    import "string"

    var content = readFile("data.txt");
    var lines = splitLines(content);

    for (var i : lines)
    {
        var line = lines[i];
        print("Line " + __toString(i) + ": " + line);
    }

Object-Oriented Counter
~~~~~~~~~~~~~~~~~~~~~~~

::

    class Counter
    {
        init(start)
        {
            this.value = start;
        }

        increment()
        {
            this.value++;
            return this.value;
        }

        decrement()
        {
            this.value--;
            return this.value;
        }

        reset()
        {
            this.value = 0;
        }
    }

    var counter = Counter(10);
    print(counter.increment());  // 11
    print(counter.increment());  // 12
    print(counter.decrement());  // 11
    counter.reset();
    print(counter.value);        // 0

Table Manipulation
~~~~~~~~~~~~~~~~~~

::

    var inventory = [
        sword: 1,
        shield: 1,
        potion: 5
    ];

    // Add item
    inventory["armor"] = 1;

    // Update item
    inventory["potion"] = inventory["potion"] + 3;

    // List inventory
    for (var item : inventory)
    {
        print(item + ": " + __toString(inventory[item]));
    }
