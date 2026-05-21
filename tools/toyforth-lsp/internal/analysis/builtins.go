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
	"dup":      {Signature: "dup ( x -- x x )", Description: "Duplicate the top stack item."},
	"drop":     {Signature: "drop ( x -- )", Description: "Discard the top stack item."},
	"swap":     {Signature: "swap ( a b -- b a )", Description: "Swap the top two stack items."},
	"over":     {Signature: "over ( a b -- a b a )", Description: "Copy the second stack item to the top."},
	"rot":      {Signature: "rot ( a b c -- b c a )", Description: "Rotate the top three stack items."},
	"swapd":    {Signature: "swapd ( a b c -- b a c )", Description: "Swap the two items below the top stack item."},
	"nip":      {Signature: "nip ( a b -- b )", Description: "Discard the second stack item and keep the top item."},
	"tuck":     {Signature: "tuck ( a b -- b a b )", Description: "Copy the top stack item beneath the next item."},
	"pick":     {Signature: "pick ( ... n -- ... x )", Description: "Copy the stack item at depth n to the top."},
	"roll":     {Signature: "roll ( ... n -- ... )", Description: "Move the stack item at depth n to the top."},
	"empty":    {Signature: "empty ( -- )", Description: "Clear the entire data stack."},
	"+":        {Signature: "+ ( a b -- a+b )", Description: "Add two numeric values."},
	"-":        {Signature: "- ( a b -- a-b )", Description: "Subtract the top value from the next value."},
	"*":        {Signature: "* ( a b -- a*b )", Description: "Multiply two numeric values."},
	"/":        {Signature: "/ ( a b -- a/b )", Description: "Divide the next value by the top value."},
	"%":        {Signature: "% ( a b -- a%b )", Description: "Compute the remainder of integer division."},
	"mod":      {Signature: "mod ( a b -- a%b )", Description: "Compute the modulo of two values."},
	"abs":      {Signature: "abs ( x -- |x| )", Description: "Replace a number with its absolute value."},
	"neg":      {Signature: "neg ( x -- -x )", Description: "Negate the top numeric value."},
	"succ":     {Signature: "succ ( n -- n+1 )", Description: "Increment a numeric value."},
	"pred":     {Signature: "pred ( n -- n-1 )", Description: "Decrement a numeric value."},
	"max":      {Signature: "max ( a b -- max )", Description: "Keep the larger of two values."},
	"min":      {Signature: "min ( a b -- min )", Description: "Keep the smaller of two values."},
	"and":      {Signature: "and ( a b -- res )", Description: "Logical AND for booleans, bitwise AND for integers."},
	"or":       {Signature: "or ( a b -- res )", Description: "Logical OR for booleans, bitwise OR for integers."},
	"xor":      {Signature: "xor ( a b -- res )", Description: "Logical XOR for booleans, bitwise XOR for integers."},
	"not":      {Signature: "not ( a -- res )", Description: "Logical NOT for booleans, bitwise NOT for integers."},
	"shl":      {Signature: "shl ( n bits -- n<<bits )", Description: "Left shift an integer by the specified number of bits."},
	"shr":      {Signature: "shr ( n bits -- n>>bits )", Description: "Right shift an integer by the specified number of bits."},
	"==":       {Signature: "== ( a b -- bool )", Description: "Compare two values for equality."},
	"!=":       {Signature: "!= ( a b -- bool )", Description: "Compare two values for inequality."},
	"<":        {Signature: "< ( a b -- bool )", Description: "Check whether the next value is less than the top value."},
	">":        {Signature: "> ( a b -- bool )", Description: "Check whether the next value is greater than the top value."},
	"<=":       {Signature: "<= ( a b -- bool )", Description: "Check whether the next value is less than or equal to the top value."},
	">=":       {Signature: ">= ( a b -- bool )", Description: "Check whether the next value is greater than or equal to the top value."},
	"if":       {Signature: "if ( cond block -- )", Description: "Execute the block when the condition is truthy."},
	"ifelse":   {Signature: "ifelse ( cond then else -- )", Description: "Execute one of two blocks based on the condition."},
	"while":    {Signature: "while ( cond body -- )", Description: "Repeat while the condition block yields a truthy value."},
	"times":    {Signature: "times ( n block -- )", Description: "Execute a block a fixed number of times."},
	"each":     {Signature: "each ( list block -- )", Description: "Execute a block for each item in a list."},
	"map":      {Signature: "map ( list block -- list )", Description: "Apply a block to each item in a list and collect one result per item."},
	"fold":     {Signature: "fold ( init list quot -- acc )", Description: "Reduce a list from left to right by applying a quotation to the accumulator and each item."},
	"exec":     {Signature: "exec ( block -- ... )", Description: "Execute a quoted block."},
	"i":        {Signature: "i ( block -- ... )", Description: "Alias for exec. Execute a quoted block."},
	"app2":     {Signature: "app2 ( x y quot -- x' y' )", Description: "Apply a quotation independently to two stack items."},
	"dip":      {Signature: "dip ( x block -- x )", Description: "Execute a block while hiding the top item of the stack."},
	"keep":     {Signature: "keep ( x block -- x ... )", Description: "Execute a block while keeping a copy of the top item on the stack."},
	"bi":       {Signature: "bi ( x block block -- ... ... )", Description: "Apply two blocks independently to the same stack item."},
	"split":    {Signature: "split ( list pred -- true false )", Description: "Partition a list by executing a predicate block for each item while preserving the surrounding stack."},
	"splitmid": {Signature: "splitmid ( list -- left right )", Description: "Split a list into left and right halves."},
	"linrec":   {Signature: "linrec ( pred then rec1 rec2 -- ... )", Description: "Linear recursion scheme: run then when pred is true, otherwise run rec1, recurse, then run rec2."},
	"binrec":   {Signature: "binrec ( pred then rec1 rec2 -- ... )", Description: "Binary recursion scheme: run then when pred is true, otherwise run rec1, recurse over two produced values, then run rec2."},
	"print":    {Signature: "print ( x -- )", Description: "Print a value."},
	"printf":   {Signature: "printf ( x -- )", Description: "Print a value without the default formatting used by print."},
	".":        {Signature: ". ( x -- x )", Description: "Print the top stack item."},
	".s":       {Signature: ".s ( -- )", Description: "Print the current stack state."},
	"cr":       {Signature: "cr ( -- )", Description: "Print a newline."},
	"key":      {Signature: "key ( -- x )", Description: "Read a key from input."},
	"input":    {Signature: "input ( -- x )", Description: "Read input from stdin."},
	"clear":    {Signature: "clear ( -- )", Description: "Clear the terminal screen."},
	"page":     {Signature: "page ( -- )", Description: "Alias for clear."},
	"words":    {Signature: "words ( -- )", Description: "Print the known dictionary words."},
	"see":      {Signature: "see ( 'name -- )", Description: "Show a source-like representation of a word definition."},
	"load":     {Signature: "load ( path -- )", Description: "Load and execute a Toy source file in the current context."},
	"readf":    {Signature: "readf ( path -- str )", Description: "Read the entire content of a file into a string."},
	"readl":    {Signature: "readl ( path -- list )", Description: "Read a file and return a list of its lines."},
	"writef":   {Signature: "writef ( path content -- )", Description: "Write string content to a file."},
	"delf":     {Signature: "delf ( path -- )", Description: "Delete a file."},
	"exists?":  {Signature: "exists? ( path -- bool )", Description: "Check if a file exists."},
	"geth":     {Signature: "geth ( list idx -- list value )", Description: "Fetch a list element by index without consuming the list."},
	"seth":     {Signature: "seth ( list idx value -- )", Description: "Store a list element by index. Consumes the list, index, and value."},
	"len":      {Signature: "len ( list|string -- list|string n )", Description: "Get the length of a list or string without consuming it."},
	"first":    {Signature: "first ( list -- list x )", Description: "Get the first element of a list without consuming the list."},
	"rest":     {Signature: "rest ( list -- list rest )", Description: "Get all elements of a list except the first one, without consuming the original list."},
	"uncons":   {Signature: "uncons ( list -- head tail )", Description: "Destructure a list into its head and tail."},
	"cons":     {Signature: "cons ( x list -- list )", Description: "Add an element x to the front of a list, returning a new list."},
	"append":   {Signature: "append ( list x -- list )", Description: "Add an element x to the end of a list, returning a new list."},
	"concat":   {Signature: "concat ( list1 list2 -- list ) | ( str1 str2 -- str )", Description: "Concatenate two lists or two strings."},
	"join":     {Signature: "join ( list sep -- str )", Description: "Join a list of strings using a separator."},
	"trim":     {Signature: "trim ( str -- str )", Description: "Remove leading and trailing whitespace from a string."},
	"upper":    {Signature: "upper ( str -- str )", Description: "Convert a string to uppercase."},
	"lower":    {Signature: "lower ( str -- str )", Description: "Convert a string to lowercase."},
	"splits":   {Signature: "splits ( str sep -- list )", Description: "Split a string into a list of substrings using a separator."},
	"range":    {Signature: "range ( start end -- list )", Description: "Create a list of integers from start up to, but not including, end."},
	"empty?":   {Signature: "empty? ( list -- list bool )", Description: "Check if a list is empty without consuming it."},
	"rand":     {Signature: "rand ( -- n )", Description: "Push a random integer."},
	"sleep":    {Signature: "sleep ( ms -- )", Description: "Pause execution for a number of milliseconds."},
	"time":     {Signature: "time ( -- n )", Description: "Push the current time value."},
	"exit":     {Signature: "exit ( code -- )", Description: "Terminate the interpreter."},
	"bye":      {Signature: "bye ( -- )", Description: "Terminate the interpreter."},
	"def":      {Signature: "def ( 'name block -- )", Description: "Bind a quoted symbol to a block definition."},
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
