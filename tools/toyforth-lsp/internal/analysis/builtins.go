package analysis

import "strings"

type Hover struct {
	Contents string
	Range    Range
}

type builtinDoc struct {
	Signature   string
	Description string
}

var builtinDocs = map[string]builtinDoc{
	"dup":       {Signature: "dup ( x -- x x )", Description: "Duplicate the top stack item."},
	"drop":      {Signature: "drop ( x -- )", Description: "Discard the top stack item."},
	"swap":      {Signature: "swap ( a b -- b a )", Description: "Swap the top two stack items."},
	"over":      {Signature: "over ( a b -- a b a )", Description: "Copy the second stack item to the top."},
	"rot":       {Signature: "rot ( a b c -- b c a )", Description: "Rotate the top three stack items."},
	"swapd":     {Signature: "swapd ( a b c -- b a c )", Description: "Swap the two items below the top stack item."},
	"nip":       {Signature: "nip ( a b -- b )", Description: "Discard the second stack item and keep the top item."},
	"tuck":      {Signature: "tuck ( a b -- b a b )", Description: "Copy the top stack item beneath the next item."},
	"pick":      {Signature: "pick ( ... n -- ... x )", Description: "Copy the stack item at depth n to the top."},
	"roll":      {Signature: "roll ( ... n -- ... )", Description: "Move the stack item at depth n to the top."},
	"empty":     {Signature: "empty ( -- )", Description: "Clear the entire data stack."},
	"+":         {Signature: "+ ( a b -- a+b )", Description: "Add two numeric values."},
	"-":         {Signature: "- ( a b -- a-b )", Description: "Subtract the top value from the next value."},
	"*":         {Signature: "* ( a b -- a*b )", Description: "Multiply two numeric values."},
	"/":         {Signature: "/ ( a b -- a/b )", Description: "Divide the next value by the top value."},
	"%":         {Signature: "% ( a b -- a%b )", Description: "Compute the remainder of integer division."},
	"mod":       {Signature: "mod ( a b -- a%b )", Description: "Compute the modulo of two values."},
	"abs":       {Signature: "abs ( x -- |x| )", Description: "Replace a number with its absolute value."},
	"neg":       {Signature: "neg ( x -- -x )", Description: "Negate the top numeric value."},
	"max":       {Signature: "max ( a b -- max )", Description: "Keep the larger of two values."},
	"min":       {Signature: "min ( a b -- min )", Description: "Keep the smaller of two values."},
	"sqrt":      {Signature: "sqrt ( n -- f )", Description: "Compute the square root of a numeric value."},
	"pow":       {Signature: "pow ( base exp -- f )", Description: "Raise a numeric base to a numeric exponent."},
	"exp":       {Signature: "exp ( n -- f )", Description: "Compute e raised to a numeric value."},
	"log":       {Signature: "log ( n -- f )", Description: "Compute the natural logarithm of a numeric value."},
	"log10":     {Signature: "log10 ( n -- f )", Description: "Compute the base-10 logarithm of a numeric value."},
	"sin":       {Signature: "sin ( n -- f )", Description: "Compute the sine of a numeric value in radians."},
	"cos":       {Signature: "cos ( n -- f )", Description: "Compute the cosine of a numeric value in radians."},
	"tan":       {Signature: "tan ( n -- f )", Description: "Compute the tangent of a numeric value in radians."},
	"floor":     {Signature: "floor ( n -- i )", Description: "Round a numeric value down to an integer."},
	"ceil":      {Signature: "ceil ( n -- i )", Description: "Round a numeric value up to an integer."},
	"round":     {Signature: "round ( n -- i )", Description: "Round a numeric value to the nearest integer."},
	"pred":      {Signature: "pred ( i -- i-1 )", Description: "Decrement an integer by one."},
	"succ":      {Signature: "succ ( i -- i+1 )", Description: "Increment an integer by one."},
	"square":    {Signature: "square ( n -- n*n )", Description: "Square an integer or float."},
	"cube":      {Signature: "cube ( n -- n*n*n )", Description: "Cube an integer or float."},
	"pi":        {Signature: "pi ( -- f )", Description: "Push pi as a floating-point value."},
	"e":         {Signature: "e ( -- f )", Description: "Push Euler's number as a floating-point value."},
	"tau":       {Signature: "tau ( -- f )", Description: "Push tau, equal to two pi, as a floating-point value."},
	"and":       {Signature: "and ( a b -- res )", Description: "Logical AND for booleans, bitwise AND for integers."},
	"or":        {Signature: "or ( a b -- res )", Description: "Logical OR for booleans, bitwise OR for integers."},
	"xor":       {Signature: "xor ( a b -- res )", Description: "Logical XOR for booleans, bitwise XOR for integers."},
	"not":       {Signature: "not ( a -- res )", Description: "Logical NOT for booleans, bitwise NOT for integers."},
	"shl":       {Signature: "shl ( n bits -- n<<bits )", Description: "Left shift an integer by the specified number of bits."},
	"shr":       {Signature: "shr ( n bits -- n>>bits )", Description: "Right shift an integer by the specified number of bits."},
	"==":        {Signature: "== ( a b -- bool )", Description: "Compare two values for equality. Lists compare structurally."},
	"!=":        {Signature: "!= ( a b -- bool )", Description: "Compare two values for inequality. Lists compare structurally."},
	"<":         {Signature: "< ( a b -- bool )", Description: "Check numeric or string ordering: whether the next value is less than the top value."},
	">":         {Signature: "> ( a b -- bool )", Description: "Check numeric or string ordering: whether the next value is greater than the top value."},
	"<=":        {Signature: "<= ( a b -- bool )", Description: "Check numeric or string ordering: whether the next value is less than or equal to the top value."},
	">=":        {Signature: ">= ( a b -- bool )", Description: "Check numeric or string ordering: whether the next value is greater than or equal to the top value."},
	"if":        {Signature: "if ( cond block -- )", Description: "Execute the block when the condition is truthy."},
	"ifelse":    {Signature: "ifelse ( cond then else -- )", Description: "Execute one of two blocks based on the condition."},
	"while":     {Signature: "while ( cond body -- )", Description: "Repeat while the condition block yields a truthy value."},
	"try":       {Signature: "try ( body handler -- )", Description: "Execute body and run handler if body reports an error."},
	"error":     {Signature: "error ( msg -- )", Description: "Raise a runtime error, optionally using a string message."},
	"infra":     {Signature: "infra ( list quot -- list )", Description: "Execute a quotation using a list as a temporary stack and collect the resulting stack into a list."},
	"cond":      {Signature: "cond ( clauses -- )", Description: "Evaluate predicate/body clause pairs and execute the body for the first true predicate."},
	"cleave":    {Signature: "cleave ( x quot-list -- ... )", Description: "Apply several quotations independently to the same value and leave all outputs on the stack."},
	"construct": {Signature: "construct ( x quot-list -- list )", Description: "Apply several quotations independently to the same value and collect their outputs into one list."},
	"times":     {Signature: "times ( n block -- )", Description: "Execute a block a fixed number of times."},
	"replicate": {Signature: "replicate ( n block -- list )", Description: "Execute a block n times and collect exactly one result from each execution into a list."},
	"each":      {Signature: "each ( list|string block -- )", Description: "Execute a block for each item in a list or each byte in a string."},
	"map":       {Signature: "map ( list|string block -- list|string )", Description: "Apply a block to each item in a list or byte in a string and collect one result per item."},
	"fold":      {Signature: "fold ( init list|string quot -- acc )", Description: "Reduce a list or string from left to right by applying a quotation to the accumulator and each item."},
	"exec":      {Signature: "exec ( block -- ... )", Description: "Execute a quoted block."},
	"i":         {Signature: "i ( block -- ... )", Description: "Alias for exec. Execute a quoted block."},
	"app2":      {Signature: "app2 ( x y quot -- x' y' )", Description: "Apply a quotation independently to two stack items."},
	"dip":       {Signature: "dip ( x block -- x )", Description: "Execute a block while hiding the top item of the stack."},
	"keep":      {Signature: "keep ( x block -- x ... )", Description: "Execute a block while keeping a copy of the top item on the stack."},
	"bi":        {Signature: "bi ( x block block -- ... ... )", Description: "Apply two blocks independently to the same stack item."},
	"filter":    {Signature: "filter ( list|string pred -- list|string )", Description: "Keep sequence items for which a predicate quotation returns true."},
	"some":      {Signature: "some ( list|string pred -- bool )", Description: "Check whether any sequence item satisfies a predicate quotation."},
	"all":       {Signature: "all ( list|string pred -- bool )", Description: "Check whether every sequence item satisfies a predicate quotation."},
	"split":     {Signature: "split ( list|string pred -- true false ) | ( str sep -- list )", Description: "Partition a list or string with a predicate quotation, or split a string using a separator."},
	"splitmid":  {Signature: "splitmid ( list|string -- left right )", Description: "Split a list or string into left and right halves."},
	"merge":     {Signature: "merge ( seq seq pred -- seq )", Description: "Merge two sorted lists or strings of the same type. The predicate runs in a stack sandbox and must leave a boolean on top."},
	"linrec":    {Signature: "linrec ( pred then rec1 rec2 -- ... )", Description: "Linear recursion scheme: run then when pred is true, otherwise run rec1, recurse, then run rec2."},
	"binrec":    {Signature: "binrec ( pred then rec1 rec2 -- ... )", Description: "Binary recursion scheme: run then when pred is true, otherwise run rec1, recurse over two produced values, then run rec2."},
	"genrec":    {Signature: "genrec ( pred then before after -- ... )", Description: "General recursion scheme: run then when pred is true, otherwise run before, recurse, then run after."},
	"treerec":   {Signature: "treerec ( tree leaf node -- result )", Description: "Recursively transform a tree using a leaf quotation and a node quotation."},
	"print":     {Signature: "print ( x -- )", Description: "Print a value."},
	"printf":    {Signature: "printf ( x -- )", Description: "Print a value without the default formatting used by print."},
	".":         {Signature: ". ( x -- x )", Description: "Print the top stack item."},
	".s":        {Signature: ".s ( -- )", Description: "Print the current stack state."},
	".S":        {Signature: ".S ( -- )", Description: "Print the current stack using source-style value formatting."},
	"cr":        {Signature: "cr ( -- )", Description: "Print a newline."},
	"key":       {Signature: "key ( -- x )", Description: "Read a key from input."},
	"input":     {Signature: "input ( -- x )", Description: "Read input from stdin."},
	"clear":     {Signature: "clear ( -- )", Description: "Clear the terminal screen."},
	"page":      {Signature: "page ( -- )", Description: "Alias for clear."},
	"typeof":    {Signature: "typeof ( x -- str )", Description: "Push a string naming the input type."},
	"bool?":     {Signature: "bool? ( x -- bool )", Description: "Check whether the input is a boolean."},
	"int?":      {Signature: "int? ( x -- bool )", Description: "Check whether the input is an integer."},
	"float?":    {Signature: "float? ( x -- bool )", Description: "Check whether the input is a float."},
	"str?":      {Signature: "str? ( x -- bool )", Description: "Check whether the input is a string."},
	"symbol?":   {Signature: "symbol? ( x -- bool )", Description: "Check whether the input is a symbol."},
	"list?":     {Signature: "list? ( x -- bool )", Description: "Check whether the input is a list."},
	"number?":   {Signature: "number? ( x -- bool )", Description: "Check whether the input is an integer or float."},
	"nan?":      {Signature: "nan? ( x -- bool )", Description: "Check whether the input is a NaN float."},
	"inf?":      {Signature: "inf? ( x -- bool )", Description: "Check whether the input is an infinite float."},
	"word?":     {Signature: "word? ( name -- bool )", Description: "Check whether a symbol or string names a dictionary word."},
	"var?":      {Signature: "var? ( name -- bool )", Description: "Check whether a symbol or string names a visible dynamic capture."},
	"inf":       {Signature: "inf ( -- f )", Description: "Push positive infinity as a floating-point value."},
	"nan":       {Signature: "nan ( -- f )", Description: "Push NaN as a floating-point value."},
	"body":      {Signature: "body ( 'name -- quot )", Description: "Push the quotation body for a user-defined word."},
	"intern":    {Signature: "intern ( str -- symbol )", Description: "Convert a string to a symbol."},
	"name":      {Signature: "name ( symbol -- str )", Description: "Convert a symbol to a string."},
	"words":     {Signature: "words ( -- list )", Description: "Push a sorted list of dictionary word names as strings."},
	"see":       {Signature: "see ( 'name -- str )", Description: "Push a source-like representation of a word definition."},
	"load":      {Signature: "load ( path -- )", Description: "Load and execute a Toy source file in the current context."},
	"readf":     {Signature: "readf ( path -- str )", Description: "Read the entire content of a file into a string."},
	"readl":     {Signature: "readl ( path -- list )", Description: "Read a file and return a list of its lines."},
	"writef":    {Signature: "writef ( path content -- )", Description: "Write string content to a file."},
	"delf":      {Signature: "delf ( path -- )", Description: "Delete a file."},
	"exists?":   {Signature: "exists? ( path -- bool )", Description: "Check if a file exists."},
	"geth":      {Signature: "geth ( list idx -- value ) | ( str idx -- char )", Description: "Fetch a list element or one-character string by index."},
	"seth":      {Signature: "seth ( list idx value -- list ) | ( str idx char -- str )", Description: "Return a collection with one element or character replaced."},
	"slice":     {Signature: "slice ( coll start end -- coll )", Description: "Return a sub-list or substring from start up to, but not including, end."},
	"take":      {Signature: "take ( coll n -- coll )", Description: "Return the first n items or characters from a list or string."},
	"dropn":     {Signature: "dropn ( coll n -- coll )", Description: "Return a list or string with the first n items or characters removed."},
	"len":       {Signature: "len ( list|string -- n )", Description: "Get the length of a list or string."},
	"first":     {Signature: "first ( list|string -- x|char )", Description: "Get the first list element or one-byte string."},
	"rest":      {Signature: "rest ( list|string -- rest )", Description: "Get all sequence items except the first one."},
	"uncons":    {Signature: "uncons ( list|string -- head tail )", Description: "Destructure a list or string into its head and tail."},
	"cons":      {Signature: "cons ( x list -- list ) | ( char str -- str )", Description: "Add one item to the front of a list or one-byte string to the front of a string."},
	"append":    {Signature: "append ( list x -- list ) | ( str char -- str )", Description: "Add one item to the end of a list or one-byte string to the end of a string."},
	"concat":    {Signature: "concat ( list1 list2 -- list ) | ( str1 str2 -- str )", Description: "Concatenate two lists or two strings."},
	"join":      {Signature: "join ( list sep -- str )", Description: "Join a list of strings using a separator."},
	"trim":      {Signature: "trim ( str -- str )", Description: "Remove leading and trailing whitespace from a string."},
	"upper":     {Signature: "upper ( str -- str )", Description: "Convert a string to uppercase."},
	"lower":     {Signature: "lower ( str -- str )", Description: "Convert a string to lowercase."},
	"range":     {Signature: "range ( start end -- list )", Description: "Create a list of integers from start up to, but not including, end."},
	"empty?":    {Signature: "empty? ( list|string -- bool )", Description: "Check whether a list or string is empty."},
	"rand":      {Signature: "rand ( -- n )", Description: "Push a random integer."},
	"sleep":     {Signature: "sleep ( ms -- )", Description: "Pause execution for a number of milliseconds."},
	"argc":      {Signature: "argc ( -- n )", Description: "Push the number of script arguments."},
	"argv":      {Signature: "argv ( -- list )", Description: "Push script arguments as a list of strings."},
	"getenv":    {Signature: "getenv ( key -- value|[] )", Description: "Read an environment variable, or push an empty list when it is not set."},
	"setenv":    {Signature: "setenv ( key value -- bool )", Description: "Set an environment variable and push whether it succeeded."},
	"pwd":       {Signature: "pwd ( -- str )", Description: "Push the current working directory."},
	"shell":     {Signature: "shell ( cmd -- output )", Description: "Run a shell command and push its output as a string."},
	"time":      {Signature: "time ( -- epoch )", Description: "Push the current Unix epoch time in seconds."},
	"clock":     {Signature: "clock ( -- ticks )", Description: "Push C clock ticks for rough process timing."},
	"exit":      {Signature: "exit ( -- )", Description: "Terminate the interpreter."},
	"bye":       {Signature: "bye ( -- )", Description: "Terminate the interpreter."},
	"def":       {Signature: "def ( 'name block -- )", Description: "Bind a quoted symbol to a block definition."},
	":":         {Signature: ": name ... ;", Description: "Define a word using Forth-style syntax."},
}

func LookupHover(index DocumentIndex, pos Position) (Hover, bool) {
	word, tok, ok := lookupTokenAt(index, pos)
	if !ok {
		return Hover{}, false
	}

	if tok.Kind == tokenKindVariable {
		local, ok := lookupLocalBinding(index, word, pos)
		if !ok {
			return Hover{}, false
		}
		return Hover{
			Contents: "```toy\n$" + local.Name + "\n```\nLocal binding from `{ " + local.Name + " }`.",
			Range:    tok.Range,
		}, true
	}

	if doc, ok := builtinDocs[word]; ok {
		return Hover{
			Contents: "```toy\n" + doc.Signature + "\n```\n" + doc.Description,
			Range:    tok.Range,
		}, true
	}

	if sym, ok := index.Definitions[word]; ok {
		header := sym.Name
		if sym.StackEffect != "" {
			header += " ( " + sym.StackEffect + " )"
		}

		body := sym.Detail
		switch {
		case sym.Doc != "" && sym.StackEffect != "":
			body = sym.Doc + "\n\nStack effect: `" + sym.StackEffect + "`"
		case sym.Doc != "":
			body = sym.Doc
		case sym.StackEffect != "":
			body = sym.StackEffect
		}

		return Hover{
			Contents: strings.TrimSpace("```toy\n" + header + "\n```\n" + body),
			Range:    tok.Range,
		}, true
	}

	return Hover{}, false
}
