/** @mainpage

@section main-intro Introduction

The libt3config library provides functions for reading and writing simple
structured configuration files.

libt3config is part of the <a href="http://os.ghalkes.nl/t3/">Tilde Terminal
Toolkit (T3)</a>.

Information is available about the @ref syntax "syntax of the configuration files",
and @ref schema "schema syntax". Finally there is the
<a class="el" href="modules.html">API documentation</a>.

@section main-example Example

The example below shows a small program which loads a configuration file and
prints it to screen. It can be passed a schema which will be used for
validating the configuration, and can optionally enable the include mechanism.

@include example.c

@section main-guidelines Guidelines

The API of libt3config is designed such that it is possible to write code that
only checks for errors in a limited number of places. That is, functions that
fetch or store values in the config, all accept @c NULL as the pointer to the
(sub-)config to add to, and successful return values are 0 to allow logical
OR-ing of the values to check for simple errors. For functions that fetch
values, reasonable defaults are provided.So, the following code is guaranteed
not to crash:

@code
int has_errors = 0;
t3_config_t *config = t3_config_new();
has_errors |= config == NULL;
has_errors |= t3_config_add_bool(config, "flag", t3_true);
t3_bool value = t3_config_get_bool(t3_config_get(config, "flag"));
@endcode

*/

//==========================================================

/** @page syntax Syntax of configuration files.

@section syntax-intro Introduction

Configuration files as used by libt3config are structured files. They consist
of a set of key/value expressions, where each key is assigned a type based on
the assigned value. Each key may only occur once.

@section types Data types

There are six data types available: integers, floating point numbers, strings,
booleans, lists and sections. The data type of each item is determined from the
input as follows:

@subsection integers Integers

Integers consist of a either an optional sign, followed by a series of digits
from @c 0 through @c 9, or of @c 0x followed by a series of hexadecimal digits
(i.e. @c 0 through @c 9 and @c a through @c f in either lower or upper case).

@subsection floating_point Floating point numbers

Floating point numbers consist of an optional sign, zero or more decimal
digits, a point, zero or more optional digits and an optional exponent. The
exponent consists of the letter @c e (or @c E), an optional exponent and one or
more decimal digits. The special words @c nan, @c inf and @c infinity are also
accepted as floating point numbers.

@subsection strings Strings

Strings are text enclosed in either @" or '. Strings may not include newline
characters. To include the delimiting character in the string, repeat the
character twice (i.e. <tt>'foo''bar'</tt> encodes the string <tt>foo'bar</tt>).
Multiple strings may be concatenated by using a plus sign (+). To split a
string accross multiple lines, use string concatenation using the plus sign,
where a plus sign must appear before the newline after each substring.

@subsection booleans Booleans

Booleans are one of the special words @c yes, @c no, @c true or @c false.

@subsection lists Lists

Lists are items enclosed by parentheses and separated by commas. A list may
contain items of any datatype, including lists. Alternately, a list can be
specified by repeatedly using the list name preceeded by a @%-sign and
assigning a single value to each of them. The list is inserted in the parsed
configuration at the first occurence of the name.

@subsection sections Sections

Sections are delimited by curly braces ({}), and contain key/value expressions.
Key/value expressions are separated by newlines or semi-colons. For all but the
section type, the key/value expressions are of the form <tt>key = value</tt>.
For sections, the key/value expression is of the form <tt>key { @<key/value
expressions@> }</tt>. Key names may consist of the letters a through z, in both
upper- and lowercase, the digits 0 through 9, hyphen and underscore. The first
character of a key name may not be a digit or a hyphen. Each key may only occur
once.

@subsection syntax-example Example

@verbatim
# integer:
i = 9
# floating point number:
f = 1.0
# strings:
s1 = "A simple string"
s2 = 'Another string, with embedded double quotes: "'
s3 = "Embedded double qoutes "" in a double quoted string"
s4 = "Multi-part string" +
    ' split over multiple lines'
# boolean:
b = true
# lists:
l1 = ( 1, yes, "text", ( 7, 8, 9 ), { key = "value" } )
# The following is equivalent to: l2 = ( 1, yes, "text" )
%l2 = 1
%l2 = yes
%l2 = "text"
# sections:
sect {
  foo = yes
  bar = 9
  sect {
    l1 = ( true, false, yes, no )
  }
}
@endverbatim

@section include File Inclusion

Libt3config provides a mechanism to include other files from a configuration
file. An included file must be a valid configuration file in itself. By default
the include mechanism not used. To enable it, set either the
::T3_CONFIG_INCLUDE_DFLT or the ::T3_CONFIG_INCLUDE_USER flag in the
::t3_config_opts_t struct passed to ::t3_config_read_file or
::t3_config_read_buffer (or the equivalent schema loading functions). Depending
on the flag passed, different members of the ::t3_config_opts_t struct must be
filled in.

Once the include mechanism has been enabled, files can be included using the
percent-style list specification syntax:

@verbatim
%include = "file1.inc"
%include = "file2.inc"
@endverbatim

The include mechanism prevents recursive inclusion, by keeping track of the
included file names. If the same name is included again from a deeper nested
file, it will trigger an error and abort the parsing process. This does not
preclude multiple inclusion of the same file at different points in the
inclusion hierarchy. For example, the following is perfectly valid (as long as
<tt>file1.inc</tt> does not recursively include itself):

@verbatim
%include = "file1.inc"

nested {
  %include = "file1.inc"
}
@endverbatim
*/

//==========================================================

/** @page schema Schemas

@section schema-intro Introduction

When using libt3config to read configuration files, it is possible to define
a schema to which the configuration data must conform. This schema can define
the type of the different entries, permitted values, conflicts between keys
and more. This page describes the schema syntax and semantics.

@section basics Basics

Schemas are expressed as configuration files. They therefore use the same
syntax as normal configuration files. However, because they describe other
configuration files, they must also obey a specific structure (or schema).

At the top level, four specific items are allowed: first a @c types section
describing new types. Second, an @c allowed-keys section defining the
permissible keys and their types. Third an @c item-type string defining the
required type of keys not specifically allowed by the @c allowed-keys section.
Finally, a list of strings named @c constraint which describes further
restrictions.

The @c allowed-keys section defines the different key names that are allowed.
Each key used in the @c allowed-keys section must itself be a section with at
least the @c type key set to a string describing the type. For example, to
define a configuration which may have two keys, @c version and @c value, both
of integer type, one might write the following:

@verbatim
allowed-keys {
  version {
    type = "int"
  }
  number {
    type = "int"
  }
}
@endverbatim

Each key description section, such as @c version in the example above, has a
set of permissible keys, depending on the value of its @c type key. The @c
constraint list is always allowed. If @c type is set to @c list or @c
section, the @c item-type key is allowed, with type string. If @c type is set
to @c section, an @c allowed-keys section, exactly like the @c allowed-keys
section at the top level, is allowed.

The example below declares that the only allowed key is named @c car, which
may contain any of the keys @c make, @c model and @c registration, all with
string type.

@verbatim
allowed-keys {
  car {
    type = "section"
    allowed-keys {
      make {
        type = "string"
      }
      model {
        type = "string"
      }
      registration {
        type = "string"
      }
    }
  }
}
@endverbatim

@section types User-defined types

There are seven pre-defined data type names: @c int, @c number, @c bool, @c
string, @c list, @c section and @c any. However, it is often useful to create
user defined types. The example above allowed a single car to be defined.
However, maybe we want to define a list of cars, each of which should be
described by a section with the @c make, @c model and @c registration keys.
This can be easily achieved by defining a @c car type in the @c types top-level
section:

@verbatim
types {
  car {
    type = "section"
    allowed-keys {
      make {
        type = "string"
      }
      model {
        type = "string"
      }
      registration {
        type = "string"
      }
    }
  }
}

allowed-keys {
  car {
    type = "list"
    item-type = "car"
  }
}
@endverbatim

If a type is listed in the @c types section, it may be used in the @c type key
of any definition, be it a @c types definition, a definition in @c allowed-keys
or an @c item-type definition.

@section constraints Constraints

Although the @c allowed-keys and typed keys mechanisms works well to restrict
the structure of the configuration file, it is by no means a complete method
for limiting the possible inputs. Therefore, further constraints can be added
in a separate language. These constraints are expressed within the schema in
strings, in a list named @c constraint, which may be added both at the top
level of the schema, or at the same nesting level as the @c type keys.

@subsection constraint_basics Constraint basics

When constraints are placed on simple types (integer, floating point number,
string or boolean), the value of the key may be accessed by using a percent
sign (@%). For example:

@verbatim
allowed-keys {
  version {
    type = "int"
    %constraint = "% > 0"
  }
}
@endverbatim

indicates that the value of the key @c version must be greater than 0.

There are six comparison operators: = (equals), != (not equals), @<, @<=,
@> and @>=. Note that for booleans and strings, only the = and != comparison
operators are valid. Constants follow the same rules as described above for
constants in the configuration file.

When the type of the constant does not match the type of the value, the
comparison is always false. For constraints on simple types as above this will
be detected when loading the schema, and an error will be reported. However,
for more complicated constraints it is not always possible to determine the
type of the comparison operands from the schema, in which case the comparison
will simply result in a false result.

The schema language also supports the boolean operators @& (and), | (or),
^ (exclusive or) and ! (not). These can be used to build more complex
constraints.

@subsection aggregate_types Aggregate types

For lists it is also possible to put a constraint on their size. To do this,
use the hash sign (@#) instead of the percent sign, and treat its value as an
integer:

@verbatim
allowed-keys {
  cars {
    type = "list"
    %constraint = "# >= 2"
  }
}
@endverbatim

Constraints on section types can use the names of the constituent keys. When
used in a comparison, the key is replaced with its value. When used alone, it
evaluates to true if the key is present, and to false if it is absent:

@verbatim
allowed-keys {
  version { type = "int" }
  number { type = "int"
}
%constraint = "version"     # Assert that the version key is present
%constraint = "number = 1"  # Assert that the number key must have value 1
@endverbatim

Note that if @c number is absent, the evaluation of <tt>number = 1</tt> would
result in @c false, thereby deeming the configuration invalid. If the key may
be absent, then the constraint should be <tt>!number | number = 1</tt>.

For constraints on sections, the @% symbol is invalid. The @# symbol however
evaluates to the number of keys in the section. Sometimes it is also useful to
limit the number of keys in a subset of the possible keys. This is possible
using the <tt>@#(key1, key2, ...)</tt> syntax. When evaluated, it will be
replaced by the number of keys out of the listed set that is present in the
configuration.

The @# operator can also be used on expressions that denote a list. This allows
one to compare sizes of different lists in a section, or by using references
(see next sub-section), even with values from a completely different part of
the configuration file.

@verbatim
allowed-keys {
  cars {
    type = "list"
  }
}
%constraint = "#cars >= 2"
@endverbatim


@subsection references References

One can refer to other keys in the configuration file, using a syntax
similar to the Un*x file-name syntax:

@verbatim
allowed-keys {
  foo {
    type = "int"
    %constraint = "/bar"
  }
  bar {
    type = "int"
  }
}
@endverbatim

This indicates that if @c foo is present, then so must @c bar. In this specific
example the constraints can of course also be expressed by the top-level
constraint <tt>!foo | bar</tt>, but remember that these types of expressions
may also be used on types. Furthermore, the values of string keys may be used
in the path by enclosing them in square brackets ([]):

@verbatim
allowed-keys {
  car {
    type = "section"
    allowed-keys {
      owner { type = "string" }
      make { type = "string" }
      mode { type = "string" }
    }
    %constraint = "/owner/[owner]/name"
  }
  owner {
    type = "section"
    item-type ="section"
  }
}
@endverbatim

The constraint in the example indicates that if the @c car section is present,
there must be a section in the top-level owner section with the name indicated
by the string in <tt>/car/owner</tt>, which must further contain a @c name key.

Note that references that do not start with a slash (/) will be resolved
relative to the current section, instead of as an absolute reference.

@subsection descriptions Descriptions

The ::t3_config_validate function can return the text of the constraint on
detection of a constraint validation. However, in most cases these constraints
will not make much sense to users. Therefore it is possible to include a
descriptive string in a constraint, which will be returned instead:

@verbatim
allowed-keys {
  foo {
    type = "int"
    %constraint = "{'foo' may only be used simultaneously with 'bar'} /bar"
  }
  bar {
    type = "int"
  }
}
@endverbatim

The descriptive string must be enclosed in curly braces ({}), and must be
the first item in the constraint.

*/
