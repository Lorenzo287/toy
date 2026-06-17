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
	"dup":          {Signature: "dup ( x -- x x )", Description: "Duplicate the top stack item."},
	"drop":         {Signature: "drop ( x -- )", Description: "Discard the top stack item."},
	"swap":         {Signature: "swap ( a b -- b a )", Description: "Swap the top two stack items."},
	"over":         {Signature: "over ( a b -- a b a )", Description: "Copy the second stack item to the top."},
	"rot":          {Signature: "rot ( a b c -- b c a )", Description: "Rotate the top three stack items."},
	"swapd":        {Signature: "swapd ( a b c -- b a c )", Description: "Swap the two items below the top stack item."},
	"nip":          {Signature: "nip ( a b -- b )", Description: "Discard the second stack item and keep the top item."},
	"tuck":         {Signature: "tuck ( a b -- b a b )", Description: "Copy the top stack item beneath the next item."},
	"pick":         {Signature: "pick ( ... n -- ... x )", Description: "Copy the stack item at depth n to the top."},
	"roll":         {Signature: "roll ( ... n -- ... )", Description: "Move the stack item at depth n to the top."},
	"empty":        {Signature: "empty ( -- )", Description: "Clear the entire data stack."},
	"+":            {Signature: "+ ( a b -- a+b )", Description: "Add two numeric values."},
	"-":            {Signature: "- ( a b -- a-b )", Description: "Subtract the top value from the next value."},
	"*":            {Signature: "* ( a b -- a*b )", Description: "Multiply two numeric values."},
	"/":            {Signature: "/ ( a b -- a/b )", Description: "Divide the next value by the top value."},
	"%":            {Signature: "% ( a b -- a%b )", Description: "Compute the remainder of integer division."},
	"mod":          {Signature: "mod ( a b -- a%b )", Description: "Compute the modulo of two values."},
	"abs":          {Signature: "abs ( x -- |x| )", Description: "Replace a number with its absolute value."},
	"neg":          {Signature: "neg ( x -- -x )", Description: "Negate the top numeric value."},
	"max":          {Signature: "max ( a b -- max )", Description: "Keep the larger of two values."},
	"min":          {Signature: "min ( a b -- min )", Description: "Keep the smaller of two values."},
	"sqrt":         {Signature: "sqrt ( n -- f )", Description: "Compute the square root of a numeric value."},
	"pow":          {Signature: "pow ( base exp -- f )", Description: "Raise a numeric base to a numeric exponent."},
	"exp":          {Signature: "exp ( n -- f )", Description: "Compute e raised to a numeric value."},
	"log":          {Signature: "log ( n -- f )", Description: "Compute the natural logarithm of a numeric value."},
	"log10":        {Signature: "log10 ( n -- f )", Description: "Compute the base-10 logarithm of a numeric value."},
	"sin":          {Signature: "sin ( n -- f )", Description: "Compute the sine of a numeric value in radians."},
	"cos":          {Signature: "cos ( n -- f )", Description: "Compute the cosine of a numeric value in radians."},
	"tan":          {Signature: "tan ( n -- f )", Description: "Compute the tangent of a numeric value in radians."},
	"floor":        {Signature: "floor ( n -- i )", Description: "Round a numeric value down to an integer."},
	"ceil":         {Signature: "ceil ( n -- i )", Description: "Round a numeric value up to an integer."},
	"round":        {Signature: "round ( n -- i )", Description: "Round a numeric value to the nearest integer."},
	"pred":         {Signature: "pred ( i -- i-1 )", Description: "Decrement an integer by one."},
	"succ":         {Signature: "succ ( i -- i+1 )", Description: "Increment an integer by one."},
	"square":       {Signature: "square ( n -- n*n )", Description: "Square an integer or float."},
	"cube":         {Signature: "cube ( n -- n*n*n )", Description: "Cube an integer or float."},
	"pi":           {Signature: "pi ( -- f )", Description: "Push pi as a floating-point value."},
	"e":            {Signature: "e ( -- f )", Description: "Push Euler's number as a floating-point value."},
	"tau":          {Signature: "tau ( -- f )", Description: "Push tau, equal to two pi, as a floating-point value."},
	"and":          {Signature: "and ( a b -- res )", Description: "Logical AND for booleans, bitwise AND for integers."},
	"or":           {Signature: "or ( a b -- res )", Description: "Logical OR for booleans, bitwise OR for integers."},
	"xor":          {Signature: "xor ( a b -- res )", Description: "Logical XOR for booleans, bitwise XOR for integers."},
	"not":          {Signature: "not ( a -- res )", Description: "Logical NOT for booleans, bitwise NOT for integers."},
	"shl":          {Signature: "shl ( n bits -- n<<bits )", Description: "Left shift an integer by the specified number of bits."},
	"shr":          {Signature: "shr ( n bits -- n>>bits )", Description: "Right shift an integer by the specified number of bits."},
	"==":           {Signature: "== ( a b -- bool )", Description: "Compare two values for equality. Vectors and lists compare structurally."},
	"!=":           {Signature: "!= ( a b -- bool )", Description: "Compare two values for inequality. Vectors and lists compare structurally."},
	"<":            {Signature: "< ( a b -- bool )", Description: "Check numeric or string ordering: whether the next value is less than the top value."},
	">":            {Signature: "> ( a b -- bool )", Description: "Check numeric or string ordering: whether the next value is greater than the top value."},
	"<=":           {Signature: "<= ( a b -- bool )", Description: "Check numeric or string ordering: whether the next value is less than or equal to the top value."},
	">=":           {Signature: ">= ( a b -- bool )", Description: "Check numeric or string ordering: whether the next value is greater than or equal to the top value."},
	"if":           {Signature: "if ( bool|callable callable -- )", Description: "Execute the callable when the condition is true."},
	"ifelse":       {Signature: "ifelse ( bool|callable callable callable -- )", Description: "Execute one of two callables based on the condition."},
	"while":        {Signature: "while ( callable callable -- )", Description: "Repeat while the condition callable yields true."},
	"try":          {Signature: "try ( callable callable -- )", Description: "Execute body and run handler if body reports an error."},
	"error":        {Signature: "error ( msg -- )", Description: "Raise a runtime error, optionally using a string message."},
	"infra":        {Signature: "infra ( vector callable -- vector )", Description: "Execute a callable using a vector as a temporary stack and collect the resulting stack into a vector."},
	"cond":         {Signature: "cond ( clauses -- )", Description: "Evaluate predicate/body clause pairs and execute the body for the first true predicate."},
	"cleave":       {Signature: "cleave ( x callable-vector -- ... )", Description: "Apply several callables independently to the same value and leave all outputs on the stack."},
	"construct":    {Signature: "construct ( x callable-vector -- vector )", Description: "Apply several callables independently to the same value and collect their outputs into one vector."},
	"times":        {Signature: "times ( n callable -- )", Description: "Execute a callable a fixed number of times."},
	"replicate":    {Signature: "replicate ( n callable -- vector )", Description: "Execute a callable n times and collect exactly one result from each execution into a vector."},
	"each":         {Signature: "each ( vector|list|string callable -- )", Description: "Execute a callable for each item in a vector or list, or each byte in a string."},
	"map":          {Signature: "map ( vector|list|string callable -- vector|list|string )", Description: "Apply a callable to each sequence item and preserve the input sequence family."},
	"fold":         {Signature: "fold ( init vector|list|string callable -- acc )", Description: "Reduce a vector, list, or string from left to right by applying a callable to the accumulator and each item."},
	"exec":         {Signature: "exec ( callable -- ... )", Description: "Execute a quoted symbol or quotation."},
	"i":            {Signature: "i ( callable -- ... )", Description: "Alias for exec. Execute a quoted symbol or quotation."},
	"app2":         {Signature: "app2 ( x y callable -- x' y' )", Description: "Apply a callable independently to two stack items."},
	"dip":          {Signature: "dip ( x callable -- x )", Description: "Execute a callable while hiding the top item of the stack."},
	"keep":         {Signature: "keep ( x callable -- x ... )", Description: "Execute a callable while keeping a copy of the top item on the stack."},
	"bi":           {Signature: "bi ( x callable callable -- ... ... )", Description: "Apply two callables independently to the same stack item."},
	"filter":       {Signature: "filter ( vector|list|string callable -- vector|list|string )", Description: "Keep sequence items for which a predicate callable returns true."},
	"some":         {Signature: "some ( vector|list|string callable -- bool )", Description: "Check whether any sequence item satisfies a predicate callable."},
	"all":          {Signature: "all ( vector|list|string callable -- bool )", Description: "Check whether every sequence item satisfies a predicate callable."},
	"split":        {Signature: "split ( vector|list|string callable -- true false ) | ( str sep -- vector )", Description: "Partition a vector, list, or string with a predicate callable, or split a string using a separator."},
	"splitmid":     {Signature: "splitmid ( vector|list|string -- left right )", Description: "Split a vector, list, or string into left and right halves."},
	"merge":        {Signature: "merge ( seq seq callable -- seq )", Description: "Merge two sorted sequences of the same type. The predicate callable runs in a stack sandbox and must leave a boolean on top."},
	"linrec":       {Signature: "linrec ( callable callable callable callable -- ... )", Description: "Linear recursion scheme: run then when pred is true, otherwise run rec1, recurse, then run rec2."},
	"binrec":       {Signature: "binrec ( callable callable callable callable -- ... )", Description: "Binary recursion scheme: run then when pred is true, otherwise run rec1, recurse over two produced values, then run rec2."},
	"genrec":       {Signature: "genrec ( callable callable callable callable -- ... )", Description: "General recursion scheme: run then when pred is true, otherwise run before, recurse, then run after."},
	"treerec":      {Signature: "treerec ( tree callable callable -- result )", Description: "Recursively transform a tree using leaf and node callables."},
	"print":        {Signature: "print ( x -- )", Description: "Print a value."},
	"printf":       {Signature: "printf ( x -- )", Description: "Print a value without the default formatting used by print."},
	".":            {Signature: ". ( x -- x )", Description: "Print the top stack item."},
	".s":           {Signature: ".s ( -- )", Description: "Print the current stack state."},
	".S":           {Signature: ".S ( -- )", Description: "Print the current stack using source-style value formatting."},
	"cr":           {Signature: "cr ( -- )", Description: "Print a newline."},
	"key":          {Signature: "key ( -- x )", Description: "Read a key from input."},
	"input":        {Signature: "input ( -- x )", Description: "Read input from stdin."},
	"clear":        {Signature: "clear ( -- )", Description: "Clear the terminal screen."},
	"page":         {Signature: "page ( -- )", Description: "Alias for clear."},
	"typeof":       {Signature: "typeof ( x -- str )", Description: "Push a string naming the input type."},
	"bool?":        {Signature: "bool? ( x -- bool )", Description: "Check whether the input is a boolean."},
	"int?":         {Signature: "int? ( x -- bool )", Description: "Check whether the input is an integer."},
	"float?":       {Signature: "float? ( x -- bool )", Description: "Check whether the input is a float."},
	"str?":         {Signature: "str? ( x -- bool )", Description: "Check whether the input is a string."},
	"symbol?":      {Signature: "symbol? ( x -- bool )", Description: "Check whether the input is a symbol."},
	"vector?":      {Signature: "vector? ( x -- bool )", Description: "Check whether the input is a vector."},
	"list?":        {Signature: "list? ( x -- bool )", Description: "Check whether the input is a linked list."},
	"map?":         {Signature: "map? ( x -- bool )", Description: "Check whether the input is a map."},
	"set?":         {Signature: "set? ( x -- bool )", Description: "Check whether the input is a set."},
	"deque?":       {Signature: "deque? ( x -- bool )", Description: "Check whether the input is a deque."},
	"pqueue?":      {Signature: "pqueue? ( x -- bool )", Description: "Check whether the input is a priority queue."},
	"number?":      {Signature: "number? ( x -- bool )", Description: "Check whether the input is an integer or float."},
	"sequence?":    {Signature: "sequence? ( x -- bool )", Description: "Check whether the input can be iterated as a sequence: vector, list, or string."},
	"callable?":    {Signature: "callable? ( x -- bool )", Description: "Check whether the input can be executed now: a vector quotation or a symbol naming a defined word."},
	"nan?":         {Signature: "nan? ( x -- bool )", Description: "Check whether the input is a NaN float."},
	"inf?":         {Signature: "inf? ( x -- bool )", Description: "Check whether the input is an infinite float."},
	"word?":        {Signature: "word? ( symbol -- bool )", Description: "Check whether a symbol names a dictionary word."},
	"var?":         {Signature: "var? ( symbol -- bool )", Description: "Check whether a symbol names a visible dynamic capture."},
	"inf":          {Signature: "inf ( -- f )", Description: "Push positive infinity as a floating-point value."},
	"nan":          {Signature: "nan ( -- f )", Description: "Push NaN as a floating-point value."},
	"body":         {Signature: "body ( 'name -- quot )", Description: "Push the quotation body for a user-defined word."},
	"intern":       {Signature: "intern ( str -- symbol )", Description: "Convert a string to a symbol."},
	"name":         {Signature: "name ( symbol -- str )", Description: "Convert a symbol to a string."},
	"words":        {Signature: "words ( -- vector )", Description: "Push a sorted vector of dictionary word names as symbols."},
	"see":          {Signature: "see ( 'name -- str )", Description: "Push a source-like representation of a word definition."},
	"load":         {Signature: "load ( path -- )", Description: "Load and execute a Toy source file in the current context."},
	"readf":        {Signature: "readf ( path -- str )", Description: "Read the entire content of a file into a string."},
	"readl":        {Signature: "readl ( path -- vector )", Description: "Read a file and return a vector of its lines."},
	"writef":       {Signature: "writef ( path content -- )", Description: "Write string content to a file."},
	"delf":         {Signature: "delf ( path -- )", Description: "Delete a file."},
	"exists?":      {Signature: "exists? ( path -- bool )", Description: "Check if a file exists."},
	"geth":         {Signature: "geth ( vector idx -- value ) | ( str idx -- char )", Description: "Fetch a vector element or one-character string by index."},
	"seth":         {Signature: "seth ( vector idx value -- vector ) | ( str idx char -- str )", Description: "Return a collection with one element or character replaced."},
	">map":         {Signature: ">map ( pairs -- map )", Description: "Build a map from a vector or list of two-item key/value pairs. Duplicate keys are an error."},
	">set":         {Signature: ">set ( vector|list|string -- set )", Description: "Build a set from a vector, list, or string, collapsing duplicate items."},
	">deque":       {Signature: ">deque ( vector|list|string -- deque )", Description: "Build a deque from a vector, list, or string, preserving front-to-back order."},
	">pqueue":      {Signature: ">pqueue ( pairs -- pqueue )", Description: "Build a priority queue from two-item [priority value] or (priority value) pairs. Lower priorities are returned first."},
	"has?":         {Signature: "has? ( map key -- bool ) | ( set item -- bool )", Description: "Check whether a map contains a key or a set contains an item."},
	"get":          {Signature: "get ( map key -- value )", Description: "Fetch a map value. Missing keys are runtime errors; use has? when absence is expected."},
	"assoc":        {Signature: "assoc ( map key value -- map )", Description: "Return a map with key associated to value."},
	"dissoc":       {Signature: "dissoc ( map key -- map )", Description: "Return a map without key. Missing keys leave the map unchanged."},
	"keys":         {Signature: "keys ( map -- vector )", Description: "Return map keys in insertion order."},
	"values":       {Signature: "values ( map -- vector )", Description: "Return map values in insertion order."},
	"pairs":        {Signature: "pairs ( map -- vector )", Description: "Return map entries as two-item key/value vectors in insertion order."},
	"items":        {Signature: "items ( set -- vector ) | ( deque -- vector )", Description: "Return set items in insertion order or deque items from front to back."},
	"adjoin":       {Signature: "adjoin ( set item -- set )", Description: "Return a set containing item."},
	"remove":       {Signature: "remove ( set item -- set )", Description: "Return a set without item. Missing items leave the set unchanged."},
	"push-front":   {Signature: "push-front ( deque item -- deque )", Description: "Return a deque with item added at the front."},
	"push-back":    {Signature: "push-back ( deque item -- deque )", Description: "Return a deque with item added at the back."},
	"pop-front":    {Signature: "pop-front ( deque -- item deque )", Description: "Remove and return the front item and updated deque."},
	"pop-back":     {Signature: "pop-back ( deque -- item deque )", Description: "Remove and return the back item and updated deque."},
	"front":        {Signature: "front ( deque -- item )", Description: "Return the front item of a non-empty deque."},
	"back":         {Signature: "back ( deque -- item )", Description: "Return the back item of a non-empty deque."},
	"pqueue-push":  {Signature: "pqueue-push ( pqueue priority value -- pqueue )", Description: "Return a priority queue with value inserted at a finite numeric priority."},
	"pqueue-peek":  {Signature: "pqueue-peek ( pqueue -- value )", Description: "Return the next value without exposing or changing heap storage order."},
	"pqueue-pop":   {Signature: "pqueue-pop ( pqueue -- value pqueue )", Description: "Remove and return the next value and updated priority queue."},
	"pqueue-drain": {Signature: "pqueue-drain ( pqueue -- vector )", Description: "Return all values in priority order without exposing heap storage order."},
	"slice":        {Signature: "slice ( coll start end -- coll )", Description: "Return a sub-vector or substring from start up to, but not including, end."},
	"take":         {Signature: "take ( coll n -- coll )", Description: "Return the first n items or characters from a vector, list, or string."},
	"dropn":        {Signature: "dropn ( coll n -- coll )", Description: "Return a vector, list, or string with the first n items or characters removed."},
	"len":          {Signature: "len ( collection -- n )", Description: "Get the length of a vector, list, string, map, set, deque, or priority queue."},
	"first":        {Signature: "first ( vector|list|string -- x|char )", Description: "Get the first sequence element or one-byte string."},
	"rest":         {Signature: "rest ( vector|list|string -- rest )", Description: "Get all sequence items except the first one."},
	"uncons":       {Signature: "uncons ( vector|list|string -- head tail )", Description: "Destructure a sequence into its head and tail."},
	"cons":         {Signature: "cons ( x vector|list -- vector|list ) | ( char str -- str )", Description: "Add one item to the front of a vector or list, or one-byte string to the front of a string."},
	"append":       {Signature: "append ( vector|list x -- vector|list ) | ( str char -- str )", Description: "Add one item to the end of a vector or list, or one-byte string to the end of a string."},
	"concat":       {Signature: "concat ( vector vector -- vector ) | ( list list -- list ) | ( str str -- str )", Description: "Concatenate two vectors, two lists, or two strings."},
	"reverse":      {Signature: "reverse ( vector|list|string -- vector|list|string )", Description: "Return a sequence with its items or bytes in reverse order."},
	"join":         {Signature: "join ( vector|list sep -- str )", Description: "Join a vector or list of strings using a separator."},
	"trim":         {Signature: "trim ( str -- str )", Description: "Remove leading and trailing whitespace from a string."},
	"upper":        {Signature: "upper ( str -- str )", Description: "Convert a string to uppercase."},
	"lower":        {Signature: "lower ( str -- str )", Description: "Convert a string to lowercase."},
	"range":        {Signature: "range ( start end -- vector )", Description: "Create a vector of integers from start up to, but not including, end."},
	"empty?":       {Signature: "empty? ( collection -- bool )", Description: "Check whether a vector, list, string, map, set, deque, or priority queue is empty."},
	"char?":        {Signature: "char? ( x -- bool )", Description: "Check whether the input is a one-byte string."},
	"letter?":      {Signature: "letter? ( x -- bool )", Description: "Check whether the input is a one-byte alphabetic string."},
	"digit?":       {Signature: "digit? ( x -- bool )", Description: "Check whether the input is a one-byte digit string."},
	"alnum?":       {Signature: "alnum? ( x -- bool )", Description: "Check whether the input is a one-byte alphabetic or digit string."},
	"space?":       {Signature: "space? ( x -- bool )", Description: "Check whether the input is a one-byte whitespace string."},
	"upper?":       {Signature: "upper? ( x -- bool )", Description: "Check whether the input is a one-byte uppercase string."},
	"lower?":       {Signature: "lower? ( x -- bool )", Description: "Check whether the input is a one-byte lowercase string."},
	"punct?":       {Signature: "punct? ( x -- bool )", Description: "Check whether the input is a one-byte punctuation string."},
	"rand":         {Signature: "rand ( -- n )", Description: "Push a random integer."},
	"sleep":        {Signature: "sleep ( ms -- )", Description: "Pause execution for a number of milliseconds."},
	"argc":         {Signature: "argc ( -- n )", Description: "Push the number of script arguments."},
	"argv":         {Signature: "argv ( -- vector )", Description: "Push script arguments as a vector of strings."},
	"env?":         {Signature: "env? ( key -- bool )", Description: "Check whether an environment variable is set."},
	"getenv":       {Signature: "getenv ( key -- value )", Description: "Read an environment variable. Fails when it is not set; use env? when absence is expected."},
	"setenv":       {Signature: "setenv ( key value -- bool )", Description: "Set an environment variable and push whether it succeeded."},
	"pwd":          {Signature: "pwd ( -- str )", Description: "Push the current working directory."},
	"shell":        {Signature: "shell ( cmd -- output )", Description: "Run a shell command and push its output as a string."},
	"time":         {Signature: "time ( -- epoch )", Description: "Push the current Unix epoch time in seconds."},
	"clock":        {Signature: "clock ( -- ticks )", Description: "Push C clock ticks for rough process timing."},
	"exit":         {Signature: "exit ( -- )", Description: "Terminate the interpreter."},
	"bye":          {Signature: "bye ( -- )", Description: "Terminate the interpreter."},
	"def":          {Signature: "def ( 'name block -- )", Description: "Bind a quoted symbol to a block definition."},
	":":            {Signature: ": name ... ;", Description: "Define a word using Forth-style syntax."},
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
			Contents: "```toy\n$" + local.Name + "\n```\nLocal binding from `| " + local.Name + " |`.",
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
