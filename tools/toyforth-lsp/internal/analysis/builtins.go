package analysis

import "strings"

type Hover struct {
	Contents string
	Range    Range
}

type builtinDoc struct {
	StackEffect string
	Syntax      string
	Description string
}

var builtinDocs = map[string]builtinDoc{
	"dup":          {StackEffect: "x -- x x", Description: "Duplicate the top stack item."},
	"drop":         {StackEffect: "x --", Description: "Discard the top stack item."},
	"swap":         {StackEffect: "a b -- b a", Description: "Swap the top two stack items."},
	"over":         {StackEffect: "a b -- a b a", Description: "Copy the second stack item to the top."},
	"rot":          {StackEffect: "a b c -- b c a", Description: "Rotate the top three stack items."},
	"swapd":        {StackEffect: "a b c -- b a c", Description: "Swap the two items below the top stack item."},
	"nip":          {StackEffect: "a b -- b", Description: "Discard the second stack item and keep the top item."},
	"tuck":         {StackEffect: "a b -- b a b", Description: "Copy the top stack item beneath the next item."},
	"pick":         {StackEffect: "... n -- ... x", Description: "Copy the stack item at depth n to the top."},
	"roll":         {StackEffect: "... n -- ...", Description: "Move the stack item at depth n to the top."},
	"empty":        {StackEffect: "--", Description: "Clear the entire data stack."},
	"+":            {StackEffect: "a b -- a+b", Description: "Add two numeric values."},
	"-":            {StackEffect: "a b -- a-b", Description: "Subtract the top value from the next value."},
	"*":            {StackEffect: "a b -- a*b", Description: "Multiply two numeric values."},
	"/":            {StackEffect: "a b -- a/b", Description: "Divide the next value by the top value."},
	"%":            {StackEffect: "a b -- a%b", Description: "Compute the remainder of integer division."},
	"mod":          {StackEffect: "a b -- a%b", Description: "Compute the modulo of two values."},
	"abs":          {StackEffect: "x -- |x|", Description: "Replace a number with its absolute value."},
	"neg":          {StackEffect: "x -- -x", Description: "Negate the top numeric value."},
	"max":          {StackEffect: "a b -- max", Description: "Keep the larger of two values."},
	"min":          {StackEffect: "a b -- min", Description: "Keep the smaller of two values."},
	"sqrt":         {StackEffect: "n -- f", Description: "Compute the square root of a numeric value."},
	"pow":          {StackEffect: "base exp -- f", Description: "Raise a numeric base to a numeric exponent."},
	"exp":          {StackEffect: "n -- f", Description: "Compute e raised to a numeric value."},
	"log":          {StackEffect: "n -- f", Description: "Compute the natural logarithm of a numeric value."},
	"log10":        {StackEffect: "n -- f", Description: "Compute the base-10 logarithm of a numeric value."},
	"sin":          {StackEffect: "n -- f", Description: "Compute the sine of a numeric value in radians."},
	"cos":          {StackEffect: "n -- f", Description: "Compute the cosine of a numeric value in radians."},
	"tan":          {StackEffect: "n -- f", Description: "Compute the tangent of a numeric value in radians."},
	"floor":        {StackEffect: "n -- i", Description: "Round a numeric value down to an integer."},
	"ceil":         {StackEffect: "n -- i", Description: "Round a numeric value up to an integer."},
	"round":        {StackEffect: "n -- i", Description: "Round a numeric value to the nearest integer."},
	"pred":         {StackEffect: "i -- i-1", Description: "Decrement an integer by one."},
	"succ":         {StackEffect: "i -- i+1", Description: "Increment an integer by one."},
	"square":       {StackEffect: "n -- n*n", Description: "Square an integer or float."},
	"cube":         {StackEffect: "n -- n*n*n", Description: "Cube an integer or float."},
	"pi":           {StackEffect: "-- f", Description: "Push pi as a floating-point value."},
	"e":            {StackEffect: "-- f", Description: "Push Euler's number as a floating-point value."},
	"tau":          {StackEffect: "-- f", Description: "Push tau, equal to two pi, as a floating-point value."},
	"and":          {StackEffect: "a b -- res", Description: "Logical AND for booleans, bitwise AND for integers."},
	"or":           {StackEffect: "a b -- res", Description: "Logical OR for booleans, bitwise OR for integers."},
	"xor":          {StackEffect: "a b -- res", Description: "Logical XOR for booleans, bitwise XOR for integers."},
	"not":          {StackEffect: "a -- res", Description: "Logical NOT for booleans, bitwise NOT for integers."},
	"shl":          {StackEffect: "n bits -- n<<bits", Description: "Left shift an integer by the specified number of bits."},
	"shr":          {StackEffect: "n bits -- n>>bits", Description: "Right shift an integer by the specified number of bits."},
	"==":           {StackEffect: "a b -- bool", Description: "Compare two values for equality. Vectors and lists compare structurally."},
	"!=":           {StackEffect: "a b -- bool", Description: "Compare two values for inequality. Vectors and lists compare structurally."},
	"<":            {StackEffect: "a b -- bool", Description: "Check numeric or string ordering: whether the next value is less than the top value."},
	">":            {StackEffect: "a b -- bool", Description: "Check numeric or string ordering: whether the next value is greater than the top value."},
	"<=":           {StackEffect: "a b -- bool", Description: "Check numeric or string ordering: whether the next value is less than or equal to the top value."},
	">=":           {StackEffect: "a b -- bool", Description: "Check numeric or string ordering: whether the next value is greater than or equal to the top value."},
	"if":           {StackEffect: "bool|callable callable --", Description: "Execute the callable when the condition is true."},
	"ifelse":       {StackEffect: "bool|callable callable callable --", Description: "Execute one of two callables based on the condition."},
	"while":        {StackEffect: "callable callable --", Description: "Repeat while the condition callable yields true."},
	"try":          {StackEffect: "callable callable --", Description: "Execute body and run handler if body reports an error."},
	"error":        {StackEffect: "msg --", Description: "Raise a runtime error, optionally using a string message."},
	"infra":        {StackEffect: "vector callable -- vector", Description: "Execute a callable using a vector as a temporary stack and collect the resulting stack into a vector."},
	"cond":         {StackEffect: "clauses --", Description: "Evaluate predicate/body clause pairs and execute the body for the first true predicate."},
	"cleave":       {StackEffect: "x callable-vector -- ...", Description: "Apply several callables independently to the same value and leave all outputs on the stack."},
	"construct":    {StackEffect: "x callable-vector -- vector", Description: "Apply several callables independently to the same value and collect their outputs into one vector."},
	"times":        {StackEffect: "... n callable -- ...", Description: "Execute a callable a fixed number of times, threading the current stack through each run."},
	"replicate":    {StackEffect: "n callable -- vector", Description: "Execute a callable n times and collect exactly one result from each execution into a vector."},
	"each":         {StackEffect: "... vector|list|string callable -- ...", Description: "Execute a callable for each item in a vector or list, or each byte in a string, threading the current stack through each run."},
	"map":          {StackEffect: "vector|list|string callable -- vector|list|string", Description: "Apply a callable to each sequence item and preserve the input sequence family."},
	"fold":         {StackEffect: "init vector|list|string callable -- acc", Description: "Reduce a vector, list, or string from left to right by applying a callable to the accumulator and each item."},
	"exec":         {StackEffect: "callable -- ...", Description: "Execute a quoted symbol or quotation."},
	"i":            {StackEffect: "callable -- ...", Description: "Alias for exec. Execute a quoted symbol or quotation."},
	"app2":         {StackEffect: "x y callable -- x' y'", Description: "Apply a callable independently to two stack items."},
	"dip":          {StackEffect: "x callable -- x", Description: "Execute a callable while hiding the top item of the stack."},
	"keep":         {StackEffect: "x callable -- x ...", Description: "Execute a callable while keeping a copy of the top item on the stack."},
	"bi":           {StackEffect: "x callable callable -- ... ...", Description: "Apply two callables independently to the same stack item."},
	"filter":       {StackEffect: "vector|list|string callable -- vector|list|string", Description: "Keep sequence items for which a predicate callable returns true."},
	"some":         {StackEffect: "vector|list|string callable -- bool", Description: "Check whether any sequence item satisfies a predicate callable."},
	"all":          {StackEffect: "vector|list|string callable -- bool", Description: "Check whether every sequence item satisfies a predicate callable."},
	"split":        {StackEffect: "vector|list|string callable -- true false | string sep -- vector", Description: "Partition a vector, list, or string with a predicate callable, or split a string using a separator."},
	"splitmid":     {StackEffect: "vector|list|string -- left right", Description: "Split a vector, list, or string into left and right halves."},
	"merge":        {StackEffect: "seq seq callable -- seq", Description: "Merge two sorted sequences of the same type. The predicate callable runs in a stack sandbox and must leave a boolean on top."},
	"linrec":       {StackEffect: "callable callable callable callable -- ...", Description: "Linear recursion scheme: run then when pred is true, otherwise run rec1, recurse, then run rec2."},
	"binrec":       {StackEffect: "callable callable callable callable -- ...", Description: "Binary recursion scheme: run then when pred is true, otherwise run rec1, recurse over two produced values, then run rec2."},
	"genrec":       {StackEffect: "callable callable callable callable -- ...", Description: "General recursion scheme: run then when pred is true, otherwise run before, recurse, then run after."},
	"treerec":      {StackEffect: "tree callable callable -- result", Description: "Recursively transform a tree using leaf and node callables."},
	"print":        {StackEffect: "x --", Description: "Print a value."},
	"printf":       {StackEffect: "x --", Description: "Print a value without the default formatting used by print."},
	".":            {StackEffect: "x -- x", Description: "Print the top stack item."},
	".s":           {StackEffect: "--", Description: "Print the current stack state."},
	".S":           {StackEffect: "--", Description: "Print the current stack using source-style value formatting."},
	"cr":           {StackEffect: "--", Description: "Print a newline."},
	"key":          {StackEffect: "-- x", Description: "Read a key from input."},
	"input":        {StackEffect: "-- x", Description: "Read input from stdin."},
	"clear":        {StackEffect: "--", Description: "Clear the terminal screen."},
	"page":         {StackEffect: "--", Description: "Alias for clear."},
	"typeof":       {StackEffect: "x -- string", Description: "Push a string naming the input type."},
	"bool?":        {StackEffect: "x -- bool", Description: "Check whether the input is a boolean."},
	"int?":         {StackEffect: "x -- bool", Description: "Check whether the input is an integer."},
	"float?":       {StackEffect: "x -- bool", Description: "Check whether the input is a float."},
	"string?":      {StackEffect: "x -- bool", Description: "Check whether the input is a string."},
	"symbol?":      {StackEffect: "x -- bool", Description: "Check whether the input is a symbol."},
	"vector?":      {StackEffect: "x -- bool", Description: "Check whether the input is a vector."},
	"list?":        {StackEffect: "x -- bool", Description: "Check whether the input is a linked list."},
	"map?":         {StackEffect: "x -- bool", Description: "Check whether the input is a map."},
	"set?":         {StackEffect: "x -- bool", Description: "Check whether the input is a set."},
	"deque?":       {StackEffect: "x -- bool", Description: "Check whether the input is a deque."},
	"pqueue?":      {StackEffect: "x -- bool", Description: "Check whether the input is a priority queue."},
	"number?":      {StackEffect: "x -- bool", Description: "Check whether the input is an integer or float."},
	"sequence?":    {StackEffect: "x -- bool", Description: "Check whether the input can be iterated as a sequence: vector, list, or string."},
	"callable?":    {StackEffect: "x -- bool", Description: "Check whether the input can be executed now: a vector quotation or a symbol naming a defined word."},
	"nan?":         {StackEffect: "x -- bool", Description: "Check whether the input is a NaN float."},
	"inf?":         {StackEffect: "x -- bool", Description: "Check whether the input is an infinite float."},
	"word?":        {StackEffect: "symbol -- bool", Description: "Check whether a symbol names a dictionary word."},
	"var?":         {StackEffect: "symbol -- bool", Description: "Check whether a symbol names a visible dynamic capture."},
	"inf":          {StackEffect: "-- f", Description: "Push positive infinity as a floating-point value."},
	"nan":          {StackEffect: "-- f", Description: "Push NaN as a floating-point value."},
	"body":         {StackEffect: "'name -- quot", Description: "Push the quotation body for a user-defined word."},
	"intern":       {StackEffect: "string -- symbol", Description: "Convert a string to a symbol."},
	"name":         {StackEffect: "symbol -- string", Description: "Convert a symbol to a string."},
	"words":        {StackEffect: "-- vector", Description: "Push a sorted vector of dictionary word names as symbols."},
	"see":          {StackEffect: "'name -- string", Description: "Push a source-like representation of a word definition."},
	"doc":          {StackEffect: "'name -- string", Description: "Push documentation for a defined word as a string."},
	"apropos":      {StackEffect: "query -- vector", Description: "Push a sorted vector of quoted symbols whose names or docs match query."},
	"load":         {StackEffect: "path --", Description: "Load and execute a Toy source file in the current context."},
	"readf":        {StackEffect: "path -- string", Description: "Read the entire content of a file into a string."},
	"readl":        {StackEffect: "path -- vector", Description: "Read a file and return a vector of its lines."},
	"writef":       {StackEffect: "path content --", Description: "Write string content to a file."},
	"delf":         {StackEffect: "path --", Description: "Delete a file."},
	"exists?":      {StackEffect: "path -- bool", Description: "Check if a file exists."},
	"at":           {StackEffect: "vector|string idx -- value|char", Description: "Fetch a vector element or one-character string by index."},
	"set-at":       {StackEffect: "vector idx value -- vector | string idx char -- string", Description: "Return a collection with one element or character replaced."},
	">vector":      {StackEffect: "vector|list|string -- vector", Description: "Build a vector from a sequence, preserving order."},
	">list":        {StackEffect: "vector|list|string -- list", Description: "Build a list from a sequence, preserving order."},
	">map":         {StackEffect: "pairs -- map", Description: "Build a map from a vector or list of two-item key/value pairs. Duplicate keys are an error."},
	">set":         {StackEffect: "vector|list|string -- set", Description: "Build a set from a vector, list, or string, collapsing duplicate items."},
	">deque":       {StackEffect: "vector|list|string -- deque", Description: "Build a deque from a vector, list, or string, preserving front-to-back order."},
	">pqueue":      {StackEffect: "pairs -- pqueue", Description: "Build a priority queue from two-item [priority value] or (priority value) pairs. Lower priorities are returned first."},
	"contains?":    {StackEffect: "vector|list|string item -- bool", Description: "Check whether a sequence contains an item. String items are one-character strings."},
	"indexof":      {StackEffect: "vector|list|string item -- idx|-1", Description: "Return the first index of an item in a sequence, or -1 when absent."},
	"unique":       {StackEffect: "vector|list|string -- vector|list|string", Description: "Remove duplicate sequence items, preserving the first occurrence and input family."},
	"sort":         {StackEffect: "vector|list|string -- vector|list|string", Description: "Sort numbers or strings in natural ascending order, preserving the input family."},
	"has?":         {StackEffect: "map key -- bool | set item -- bool", Description: "Check whether a map contains a key or a set contains an item."},
	"get":          {StackEffect: "map key -- value", Description: "Fetch a map value. Missing keys are runtime errors; use has? when absence is expected."},
	"assoc":        {StackEffect: "map key value -- map", Description: "Return a map with key associated to value."},
	"dissoc":       {StackEffect: "map key -- map", Description: "Return a map without key. Missing keys leave the map unchanged."},
	"keys":         {StackEffect: "map -- vector", Description: "Return map keys in insertion order."},
	"values":       {StackEffect: "map -- vector", Description: "Return map values in insertion order."},
	"pairs":        {StackEffect: "map -- vector", Description: "Return map entries as two-item key/value vectors in insertion order."},
	"items":        {StackEffect: "set -- vector | deque -- vector", Description: "Return set items in insertion order or deque items from front to back."},
	"adjoin":       {StackEffect: "set item -- set", Description: "Return a set containing item."},
	"remove":       {StackEffect: "set item -- set", Description: "Return a set without item. Missing items leave the set unchanged."},
	"push-front":   {StackEffect: "deque item -- deque", Description: "Return a deque with item added at the front."},
	"push-back":    {StackEffect: "deque item -- deque", Description: "Return a deque with item added at the back."},
	"pop-front":    {StackEffect: "deque -- item deque", Description: "Remove and return the front item and updated deque."},
	"pop-back":     {StackEffect: "deque -- item deque", Description: "Remove and return the back item and updated deque."},
	"front":        {StackEffect: "deque -- item", Description: "Consume a non-empty deque and return its front item. Use dup front to preserve the deque."},
	"back":         {StackEffect: "deque -- item", Description: "Consume a non-empty deque and return its back item. Use dup back to preserve the deque."},
	"pqueue-push":  {StackEffect: "pqueue priority value -- pqueue", Description: "Return a priority queue with value inserted at a finite numeric priority."},
	"pqueue-peek":  {StackEffect: "pqueue -- value", Description: "Consume a priority queue and return the next value without exposing heap storage order. Use dup pqueue-peek to preserve the queue."},
	"pqueue-pop":   {StackEffect: "pqueue -- value pqueue", Description: "Remove and return the next value and updated priority queue."},
	"pqueue-drain": {StackEffect: "pqueue -- vector", Description: "Return all values in priority order without exposing heap storage order."},
	"slice":        {StackEffect: "vector|string start end -- vector|string", Description: "Return a sub-vector or substring from start up to, but not including, end."},
	"take":         {StackEffect: "vector|list|string n -- vector|list|string", Description: "Return the first n items or characters from a vector, list, or string."},
	"dropn":        {StackEffect: "vector|list|string n -- vector|list|string", Description: "Return a vector, list, or string with the first n items or characters removed."},
	"len":          {StackEffect: "collection -- n", Description: "Get the length of a vector, list, string, map, set, deque, or priority queue."},
	"first":        {StackEffect: "vector|list|string -- x|char", Description: "Get the first sequence element or one-byte string."},
	"rest":         {StackEffect: "vector|list|string -- rest", Description: "Get all sequence items except the first one."},
	"uncons":       {StackEffect: "vector|list|string -- head tail", Description: "Destructure a sequence into its head and tail."},
	"cons":         {StackEffect: "x vector|list -- vector|list | char string -- string", Description: "Add one item to the front of a vector or list, or one-byte string to the front of a string."},
	"append":       {StackEffect: "vector|list x -- vector|list | string char -- string", Description: "Add one item to the end of a vector or list, or one-byte string to the end of a string."},
	"concat":       {StackEffect: "vector vector -- vector | list list -- list | string string -- string", Description: "Concatenate two vectors, two lists, or two strings."},
	"reverse":      {StackEffect: "vector|list|string -- vector|list|string", Description: "Return a sequence with its items or bytes in reverse order."},
	"join":         {StackEffect: "vector|list sep -- string", Description: "Join a vector or list of strings using a separator."},
	"trim":         {StackEffect: "string -- string", Description: "Remove leading and trailing whitespace from a string."},
	"upper":        {StackEffect: "string -- string", Description: "Convert a string to uppercase."},
	"lower":        {StackEffect: "string -- string", Description: "Convert a string to lowercase."},
	"range":        {StackEffect: "start end -- vector", Description: "Create a vector of integers from start up to, but not including, end."},
	"empty?":       {StackEffect: "collection -- bool", Description: "Check whether a vector, list, string, map, set, deque, or priority queue is empty."},
	"char?":        {StackEffect: "x -- bool", Description: "Check whether the input is a one-byte string."},
	"letter?":      {StackEffect: "x -- bool", Description: "Check whether the input is a one-byte alphabetic string."},
	"digit?":       {StackEffect: "x -- bool", Description: "Check whether the input is a one-byte digit string."},
	"alnum?":       {StackEffect: "x -- bool", Description: "Check whether the input is a one-byte alphabetic or digit string."},
	"space?":       {StackEffect: "x -- bool", Description: "Check whether the input is a one-byte whitespace string."},
	"upper?":       {StackEffect: "x -- bool", Description: "Check whether the input is a one-byte uppercase string."},
	"lower?":       {StackEffect: "x -- bool", Description: "Check whether the input is a one-byte lowercase string."},
	"punct?":       {StackEffect: "x -- bool", Description: "Check whether the input is a one-byte punctuation string."},
	"rand":         {StackEffect: "-- n", Description: "Push a random integer."},
	"sleep":        {StackEffect: "ms --", Description: "Pause execution for a number of milliseconds."},
	"argc":         {StackEffect: "-- n", Description: "Push the number of script arguments."},
	"argv":         {StackEffect: "-- vector", Description: "Push script arguments as a vector of strings."},
	"env?":         {StackEffect: "key -- bool", Description: "Check whether an environment variable is set."},
	"getenv":       {StackEffect: "key -- value", Description: "Read an environment variable. Fails when it is not set; use env? when absence is expected."},
	"setenv":       {StackEffect: "key value -- bool", Description: "Set an environment variable and push whether it succeeded."},
	"pwd":          {StackEffect: "-- string", Description: "Push the current working directory."},
	"shell":        {StackEffect: "cmd -- output", Description: "Run a shell command and push its output as a string."},
	"time":         {StackEffect: "-- epoch", Description: "Push the current Unix epoch time in seconds."},
	"clock":        {StackEffect: "-- ticks", Description: "Push C clock ticks for rough process timing."},
	"exit":         {StackEffect: "--", Description: "Terminate the interpreter."},
	"bye":          {StackEffect: "--", Description: "Terminate the interpreter."},
	"def":          {StackEffect: "'name block --", Description: "Bind a quoted symbol to a block definition."},
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
		body := doc.Description
		switch {
		case doc.StackEffect != "":
			body = "stack: `" + doc.StackEffect + "`\n\n" + body
		case doc.Syntax != "":
			body = "syntax: `" + doc.Syntax + "`\n\n" + body
		}

		return Hover{
			Contents: strings.TrimSpace("```toy\n" + word + "\n```\n" + body),
			Range:    tok.Range,
		}, true
	}

	if sym, ok := index.Definitions[word]; ok {
		header := sym.Name

		body := sym.Detail
		switch {
		case sym.Doc != "" && sym.StackEffect != "":
			body = sym.Doc + "\n\nstack: `" + sym.StackEffect + "`"
		case sym.Doc != "":
			body = sym.Doc
		case sym.StackEffect != "":
			body = "stack: `" + sym.StackEffect + "`"
		}

		return Hover{
			Contents: strings.TrimSpace("```toy\n" + header + "\n```\n" + body),
			Range:    tok.Range,
		}, true
	}

	return Hover{}, false
}
